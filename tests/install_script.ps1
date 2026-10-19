# MIT License © 2026 Binary Dice Games
#
# spec/07_release_pipeline/PLAN.md step 3: packaging/install/install.ps1
# with a file standing in for the user's Path (NYMPH_PATH_STORE), so the
# real one is never touched: one entry, nothing twice, nothing for a folder
# already there, nothing with NYMPH_NO_MODIFY_PATH=1, -Uninstall restores
# it. Then the release install from a fake release folder, with the SHA-256
# check. Runs in PowerShell 7 on any system, and Windows PowerShell 5.1.
#
#   pwsh -NoProfile -File install_script.ps1 <install.ps1> <work dir>

param([string]$Script, [string]$Work)

$ErrorActionPreference = 'Stop'
$failures = 0
function Check([string]$Description, [scriptblock]$Condition) {
  $ok = $false
  try { $ok = [bool](& $Condition) } catch { $ok = $false }
  if ($ok) { Write-Host "ok   $Description" } else { Write-Host "FAIL $Description"; $script:failures++ }
}

if (Test-Path -LiteralPath $Work) { Remove-Item -LiteralPath $Work -Recurse -Force }
New-Item -ItemType Directory -Path $Work | Out-Null
$store = Join-Path $Work 'user-path.txt'
$bin = Join-Path $Work 'bin'
$original = 'C:\Windows;C:\Tools'

# Runs install.ps1 in a child PowerShell with the given environment.
$shell = (Get-Process -Id $PID).Path
function Invoke-Installer([hashtable]$Environment, [string[]]$Arguments) {
  $saved = @{}
  foreach ($name in @('NYMPH_PATH_STORE', 'NYMPH_NO_MODIFY_PATH', 'NYMPH_RELEASE_URL', 'NYMPH_INSTALL_ARCH', 'NYMPH_VERSION')) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name)
    [Environment]::SetEnvironmentVariable($name, $null)
  }
  [Environment]::SetEnvironmentVariable('NYMPH_PATH_STORE', $store)
  foreach ($key in $Environment.Keys) { [Environment]::SetEnvironmentVariable($key, $Environment[$key]) }
  try {
    $output = & $shell -NoProfile -ExecutionPolicy Bypass -File $Script @Arguments 2>&1 | Out-String
    return [pscustomobject]@{ Code = $LASTEXITCODE; Output = $output }
  } finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
  }
}
function Get-Store { return (Get-Content -LiteralPath $store -Raw) }

# ── PATH ──────────────────────────────────────────────────────────────────────

Set-Content -LiteralPath $store -Value $original -NoNewline
$r = Invoke-Installer @{} @('-PathOnly', $bin)
Check 'adds the folder to the user Path' { (Get-Store) -eq "$original;$bin" }
Check 'says so' { $r.Output -match 'added' }
$r = Invoke-Installer @{} @('-PathOnly', $bin)
Check 'a second run adds nothing' { (Get-Store) -eq "$original;$bin" }
Check 'and says it is already there' { $r.Output -match 'already on PATH' }
$r = Invoke-Installer @{} @('-PathOnly', ($bin.ToUpperInvariant() + '\'))
Check 'case and a trailing backslash do not matter' { (Get-Store) -eq "$original;$bin" }
$r = Invoke-Installer @{} @('-Uninstall', '-BinDir', $bin)
Check '-Uninstall restores the user Path' { (Get-Store) -eq $original }

$r = Invoke-Installer @{ NYMPH_NO_MODIFY_PATH = '1' } @('-PathOnly', $bin)
Check 'NYMPH_NO_MODIFY_PATH=1 writes nothing' { (Get-Store) -eq $original }
Check 'and prints the folder' { $r.Output -match [regex]::Escape($bin) }

Remove-Item -LiteralPath $store
$r = Invoke-Installer @{} @('-PathOnly', $bin)
Check 'an empty user Path gets just the folder' { (Get-Store) -eq $bin }

# ── Release install ───────────────────────────────────────────────────────────

$release = Join-Path $Work 'release'
New-Item -ItemType Directory -Path $release | Out-Null
$fake = Join-Path $Work 'fake.exe'
Set-Content -LiteralPath $fake -Value 'fake nymph' -NoNewline
$hash = (Get-FileHash -LiteralPath $fake -Algorithm SHA256).Hash.ToLowerInvariant()
$sums = @()
foreach ($platform in @('linux-x86_64', 'macos-arm64', 'windows-x86_64.exe')) {
  Copy-Item -LiteralPath $fake -Destination (Join-Path $release "nymph-9.8.7-$platform")
  $sums += "$hash  nymph-9.8.7-$platform"
}
Set-Content -LiteralPath (Join-Path $release 'SHA256SUMS') -Value ($sums -join "`n")

Set-Content -LiteralPath $store -Value $original -NoNewline
$installed = Join-Path $bin 'nymph.exe'
$r = Invoke-Installer @{ NYMPH_RELEASE_URL = $release; NYMPH_INSTALL_ARCH = 'AMD64' } @('-BinDir', $bin)
Check 'install: copies the Windows executable' { (Test-Path -LiteralPath $installed) -and ((Get-Content -LiteralPath $installed -Raw) -eq 'fake nymph') }
Check 'install: puts the folder on PATH' { (Get-Store) -eq "$original;$bin" }
Check 'install: exit code 0' { $r.Code -eq 0 }
$r = Invoke-Installer @{ NYMPH_RELEASE_URL = $release; NYMPH_INSTALL_ARCH = 'AMD64' } @('-BinDir', $bin, '-Version', '1.0.0')
Check 'install: an unknown version fails' { $r.Code -ne 0 -and $r.Output -match 'no nymph executable' }
$r = Invoke-Installer @{ NYMPH_RELEASE_URL = $release; NYMPH_INSTALL_ARCH = 'ARM64' } @('-BinDir', $bin)
Check 'install: a CPU without a release fails' { $r.Code -ne 0 }

Remove-Item -LiteralPath $installed
Add-Content -LiteralPath (Join-Path $release 'nymph-9.8.7-windows-x86_64.exe') -Value 'evil'
$r = Invoke-Installer @{ NYMPH_RELEASE_URL = $release; NYMPH_INSTALL_ARCH = 'AMD64' } @('-BinDir', $bin)
Check 'tampered release: fails, nothing installed' { $r.Code -ne 0 -and -not (Test-Path -LiteralPath $installed) }
Check 'tampered release: says why' { $r.Output -match 'not installed' }

Set-Content -LiteralPath $installed -Value 'x'
$r = Invoke-Installer @{} @('-Uninstall', '-BinDir', $bin)
Check 'uninstall: removes the executable and the Path entry' { -not (Test-Path -LiteralPath $installed) -and (Get-Store) -eq $original }

if ($failures -gt 0) { exit 1 }
Write-Host 'install.ps1: all checks passed'
