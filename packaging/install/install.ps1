# MIT License © 2026 Binary Dice Games
#
# nymph installer for Windows (spec/07_release_pipeline/DESIGN.md section 4.9).
#
#   irm https://github.com/binary-dice-games/nymph/releases/latest/download/install.ps1 | iex
#   install.ps1 [-Version X.Y.Z] [-BinDir DIR]   download and install the release executable
#   install.ps1 -PathOnly DIR                    only put DIR on the user PATH (cmake --install runs this)
#   install.ps1 -Uninstall [-BinDir DIR]         remove DIR\nymph.exe and its PATH entry
#
# Installing downloads SHA256SUMS and the Windows executable from the
# release ($env:NYMPH_RELEASE_URL overrides the download base: a URL or a
# folder; $env:NYMPH_VERSION picks a version when piped to iex), checks its
# SHA-256, copies it to DIR\nymph.exe (default
# %LOCALAPPDATA%\Programs\nymph\bin, or <prefix>\bin when this script is
# <prefix>\share\nymph\install.ps1) and puts DIR on PATH.
#
# PATH: nothing changes when DIR is already on PATH. Otherwise DIR is
# appended to the user's Path (HKCU\Environment), never the machine's, and
# to this session's $env:Path. With $env:NYMPH_NO_MODIFY_PATH = '1'
# nothing is written. $env:NYMPH_PATH_STORE (tests only) names a file that
# stands in for the user's Path.
#
# Failures throw (with -File: exit code 1); a file whose SHA-256 differs is
# never installed. Runs in Windows PowerShell 5.1 and PowerShell 7.

param(
  [string]$Version = $env:NYMPH_VERSION,
  [string]$BinDir = '',
  [string]$PathOnly = '',
  [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0

function Write-Step([string]$Text) {
  Write-Host "nymph install: $Text"
}

# ── PATH ──────────────────────────────────────────────────────────────────────

function Get-UserPath {
  if ($env:NYMPH_PATH_STORE) {
    if (Test-Path -LiteralPath $env:NYMPH_PATH_STORE) {
      return [string](Get-Content -LiteralPath $env:NYMPH_PATH_STORE -Raw).TrimEnd("`r", "`n")
    }
    return ''
  }
  $value = [Environment]::GetEnvironmentVariable('Path', 'User')
  if ($null -eq $value) { return '' }
  return $value
}

function Set-UserPath([string]$Value) {
  if ($env:NYMPH_PATH_STORE) {
    Set-Content -LiteralPath $env:NYMPH_PATH_STORE -Value $Value -NoNewline
    return
  }
  # Also tells running programs (Explorer) that the environment changed.
  [Environment]::SetEnvironmentVariable('Path', $Value, 'User')
}

function Get-PathEntries([string]$Value) {
  return @($Value -split ';' | Where-Object { $_ -ne '' })
}

function Test-SameDir([string]$A, [string]$B) {
  return $A.TrimEnd('\', '/') -ieq $B.TrimEnd('\', '/')
}

function Test-OnPath([string]$Dir, [string]$Value) {
  foreach ($entry in Get-PathEntries $Value) {
    if (Test-SameDir $entry $Dir) { return $true }
  }
  return $false
}

function Add-ToPath([string]$Dir) {
  $user = Get-UserPath
  $processHasIt = Test-OnPath $Dir $env:Path
  $userHasIt = Test-OnPath $Dir $user
  if ($userHasIt -or ($processHasIt -and -not $env:NYMPH_PATH_STORE)) {
    Write-Step "$Dir is already on PATH"
    return
  }
  if ($env:NYMPH_NO_MODIFY_PATH -eq '1') {
    Write-Step "NYMPH_NO_MODIFY_PATH=1: PATH not changed; add $Dir to your user Path"
    return
  }
  $entries = Get-PathEntries $user
  Set-UserPath ((@($entries) + $Dir) -join ';')
  if (-not $processHasIt) {
    $env:Path = ([string]$env:Path).TrimEnd(';') + ';' + $Dir
  }
  Write-Step "added $Dir to your user Path; consoles opened from now on find nymph"
}

function Remove-FromPath([string]$Dir) {
  $user = Get-UserPath
  if (-not (Test-OnPath $Dir $user)) { return }
  $kept = @(Get-PathEntries $user | Where-Object { -not (Test-SameDir $_ $Dir) })
  Set-UserPath ($kept -join ';')
  Write-Step "removed $Dir from your user Path"
}

# ── Release download ──────────────────────────────────────────────────────────

function Get-ReleaseFile([string]$Base, [string]$Name, [string]$Target) {
  if ($Base -match '^https?://') {
    try {
      # Windows PowerShell 5.1 may default to TLS 1.0.
      [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    } catch { }
    $previous = $ProgressPreference
    $ProgressPreference = 'SilentlyContinue'
    try {
      Invoke-WebRequest -Uri "$Base/$Name" -OutFile $Target -UseBasicParsing
    } catch {
      throw "nymph install: could not download $Base/$Name ($($_.Exception.Message))"
    } finally {
      $ProgressPreference = $previous
    }
  } else {
    $folder = $Base -replace '^file:///?', ''
    $source = Join-Path $folder $Name
    if (-not (Test-Path -LiteralPath $source)) { throw "nymph install: could not download $source" }
    Copy-Item -LiteralPath $source -Destination $Target
  }
}

function Install-Release([string]$Dir) {
  $arch = $env:PROCESSOR_ARCHITECTURE
  if ($env:PROCESSOR_ARCHITEW6432) { $arch = $env:PROCESSOR_ARCHITEW6432 }
  # Tests on other systems: NYMPH_INSTALL_ARCH stands in for the CPU.
  if ($env:NYMPH_INSTALL_ARCH) { $arch = $env:NYMPH_INSTALL_ARCH }
  switch -regex ($arch) {
    '^(AMD64|x86_64)$' { $cpu = 'x86_64' }
    '^ARM64$' { $cpu = 'arm64' }
    default { throw "nymph install: no nymph release for this CPU ($arch); build it from source (docs/building.md)" }
  }
  $v = ''
  if ($Version) { $v = $Version.TrimStart('v') }
  if ($env:NYMPH_RELEASE_URL) {
    $base = $env:NYMPH_RELEASE_URL.TrimEnd('/')
  } elseif ($v) {
    $base = "https://github.com/binary-dice-games/nymph/releases/download/v$v"
  } else {
    $base = 'https://github.com/binary-dice-games/nymph/releases/latest/download'
  }

  $work = Join-Path ([IO.Path]::GetTempPath()) ("nymph-install-" + [Guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Path $work | Out-Null
  try {
    Get-ReleaseFile $base 'SHA256SUMS' (Join-Path $work 'SHA256SUMS')
    $versionPattern = if ($v) { [regex]::Escape($v) } else { '[0-9][^ ]*' }
    $pattern = "^([0-9a-f]{64})  (nymph-$versionPattern-windows-$cpu\.exe)$"
    $match = Get-Content -LiteralPath (Join-Path $work 'SHA256SUMS') |
      ForEach-Object { [regex]::Match($_, $pattern) } | Where-Object { $_.Success } | Select-Object -First 1
    if (-not $match) { throw "nymph install: $base has no nymph executable for windows-$cpu" }
    $expected = $match.Groups[1].Value
    $name = $match.Groups[2].Value
    $file = Join-Path $work $name
    Get-ReleaseFile $base $name $file
    $actual = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) {
      throw "nymph install: $base/$name has SHA-256 $actual, not $expected as SHA256SUMS says; not installed"
    }
    New-Item -ItemType Directory -Force -Path $Dir | Out-Null
    Copy-Item -LiteralPath $file -Destination (Join-Path $Dir 'nymph.exe') -Force
    Write-Step "installed $name as $(Join-Path $Dir 'nymph.exe')"
  } finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
  }
  Add-ToPath $Dir
}

# ── Main ──────────────────────────────────────────────────────────────────────

if (-not $BinDir) {
  $here = $PSScriptRoot
  if ($here -and ((Split-Path -Leaf $here) -eq 'nymph') -and ((Split-Path -Leaf (Split-Path -Parent $here)) -eq 'share')) {
    $BinDir = Join-Path (Split-Path -Parent (Split-Path -Parent $here)) 'bin'
  } elseif ($env:LOCALAPPDATA) {
    $BinDir = Join-Path $env:LOCALAPPDATA 'Programs\nymph\bin'
  } else {
    $BinDir = Join-Path $HOME '.local/bin'
  }
}

if ($PathOnly) {
  Add-ToPath $PathOnly
} elseif ($Uninstall) {
  $exe = Join-Path $BinDir 'nymph.exe'
  if (Test-Path -LiteralPath $exe) {
    Remove-Item -LiteralPath $exe -Force
    Write-Step "removed $exe"
  }
  Remove-FromPath $BinDir
} else {
  Install-Release $BinDir
}
