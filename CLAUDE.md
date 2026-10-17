# Documentation & Project Files Maintenance

## README.md — keep it concise

`README.md` is the first file agents read each conversation. Its purpose is to give a quick project overview, core build commands, a quick-start snippet, and a summary of concepts — then link out to `docs/` for details. **Do not inline long how-to content into README.md.**

Rules:
- Keep `README.md` under ~200 lines. Prose should be tight; no padded explanations.
- Detailed setup instructions belong in `docs/building.md`.
- Example run instructions belong in `docs/examples.md` (create it when the first example is added).
- When you add or change a feature, update the relevant section in `README.md` (or the relevant `docs/` file) to reflect the change. Do not leave stale content.
- If you add a new `docs/` file, add a row for it in the "Further Documentation" table in `README.md`.

## CHANGELOG.md — user-facing change log

After successfully completing any task that changes code in the project, add an
entry to `CHANGELOG.md` under the `## [Unreleased]` section (create that section
at the top, above the most recent release, if it does not exist).

Rules:
- Keep entries very brief: state *what* was done, not *how* or *why*.
- Record only changes that affect users of nymph (public API, CLI, build
  outputs, bindings, runtime behavior). Do not log internal refactors,
  test-only changes, or documentation-only changes.
- Group entries under `### Added`, `### Changed`, `### Fixed`, `### Deprecated`,
  or `### Removed` as appropriate.
- Follow the [Keep a Changelog](https://keepachangelog.com/) format already used
  in the file.

## Code documentation

Maintain clear, concise doc comments in code, especially for public APIs, classes, and module-level contracts. Prefer brief Doxygen comments that explain intent, parameters, return values, and failure modes.

## DESIGN.md — directory architecture docs

Some directories contain a `DESIGN.md` file that describes the architecture, key abstractions, and design decisions for the code in that directory.

**Contents:** Overview of the subsystem's purpose, a diagram or summary of key abstractions and their relationships, the public API contract, and the *why* behind non-obvious design decisions (trade-offs, constraints, alternatives rejected). Do not restate what the code already says — explain intent and reasoning.

**When to read:** Before making any change in a directory that has a `DESIGN.md`. Understanding the intended design prevents changes that technically compile but violate the subsystem's contracts or invariants.

**When to update:** After any change that affects the architecture, the public API surface, or a key design decision documented there. Keep it accurate; a stale `DESIGN.md` is worse than none.

**When to create:** Only when explicitly asked (the `/design` skill runs an interactive design session). Do not create a `DESIGN.md` speculatively.

## PLAN.md — feature implementation plans

Some features or directories contain a `PLAN.md` file that describes the ordered sequence of steps for implementing the feature. Each step is a self-contained, testable deliverable small enough for a human to review comfortably before the next step begins.

**Contents:** An introductory note pointing to the relevant `DESIGN.md`, then a numbered list of steps. Each step has a **Goal** (one sentence stating what is true when this step is done), **Deliverables** (the exact files created or changed and what each must contain), and **Tests** (the specific assertions that must pass before moving on). A **Completion Criteria** section at the end lists the overall pass/fail conditions for the entire feature.

**When to read:** Before implementing any step of a planned feature. If a `PLAN.md` exists, follow it — do not skip steps or reorder them without a clear reason. If a step is already done, verify its tests pass before continuing.

**When to update:** If implementation reveals that a step's deliverables or tests need adjustment, update the plan *before* diverging from it, so the document stays authoritative. Check off or remove completed steps when the feature ships.

**When to create:** Only when explicitly asked. Do not create a `PLAN.md` speculatively.

## Startup reading

At the start of every new conversation, always read `README.md` to understand the project's structure and goals. When task-specific details are needed, read the relevant `docs/` file and the in-code documentation.

## Tests

Always write automated tests validating any new or modified behavior. Maintain and update tests alongside code changes; tests should be concise, deterministic, and focused on public behavior. Add a test with `nymph_add_test(<name> <sources>)` in `tests/CMakeLists.txt`.

# nymph Coding Style Guide for Claude Code Assist

Use this guide when generating or editing code in this repository.

## General

- Match the style already used in nearby files first.
- Keep changes minimal and focused; do not reformat unrelated code.
- Preserve existing public API names and behavior unless explicitly requested.
- Prefer readable, explicit code over clever shortcuts.
- Use ASCII by default.

## C++ Style (Primary)

### Formatting

- Follow `.clang-format` exactly.
- Use 2-space indentation, never tabs.
- Keep line length around 80 columns.
- Use attached braces (`if (...) {`, `class X {`, `namespace N {`).
- Use one space before control statement parentheses (`if (...)`, `for (...)`).
- Use pointer alignment on the left (`Type* ptr`).
- Let includes be sorted by formatter rules.

### File Structure

- Start files with the MIT license header used in this repo
  (`// MIT License © 2026 Binary Dice Games`).
- Add `@file` and `@brief` Doxygen comments for public headers and key sources.
- Use section dividers for readability in larger files (for example: `// ── Section ──`).

### Includes

- In headers/sources, keep project includes grouped before standard library includes.
- Prefer explicit includes over relying on transitive includes.

### Naming

- Keep namespace style as `bdg::nymph::...` and close with a namespace comment.
- Use `snake_case` for types, functions and variables, following existing naming in each subsystem.
- Use trailing underscores for private member fields (`running_`, `mtx_`).
- Use descriptive local names (`payload_bytes`, `request_id`, `workers_mutex_`).

### API and Error Handling

- Prefer clear contract comments on public methods (`@param`, `@return`, failure behavior).
- Use `std::runtime_error` (and derived errors) for C++ error reporting where appropriate.
- At C ABI boundaries, catch all exceptions and convert to error codes/null handles.
- For optional results, prefer `std::optional` over sentinel values.

### Concurrency

- Keep lock scope tight.
- Use condition variables with explicit shutdown/stop flags.
- Never poll a plain `bool` written by another thread; use something
  synchronized (an atomic, a condition variable).

## Platform Support

nymph targets **Linux, MSYS2, and native Windows (MSVC)**. MSYS2 provides the
same POSIX layer as Linux and still defines `_WIN32`/`WIN32` (it uses the
mingw-w64 toolchain). Do not add a separate platform-suffixed file (e.g.
`foo_win.cpp`) for a small platform difference; avoid conditional
compilation (`#ifdef`, `#if defined(...)`) to branch on OS behavior unless
truly necessary. If a genuine platform difference arises, prefer a small,
narrowly-scoped `#if defined(_WIN32)` guard within the shared file over a
full file split; split into `_posix`/`_win` suffixed files only when the
divergence is large enough that a guard would make the shared file hard to
read. Native MSVC is stricter than GCC/Clang about template instantiation
order (e.g. it requires complete types where GCC/Clang tolerate an
incomplete forward declaration in some contexts) — do not assume something
that compiles cleanly on Linux/MSYS2 will compile on MSVC without
verification.

## Testing Style (GoogleTest)

- Use `TEST` / `TEST_F` with descriptive suite and test names.
- Prefer `ASSERT_*` for preconditions and `EXPECT_*` for subsequent checks.
- Group tests with clear section banners in larger test files.
- Keep tests deterministic and avoid timing-sensitive flakiness where possible.
- ctest runs every test case as its **own process, in parallel** (`-j`), so
  tests must not share anything across processes:
  - **No fixed TCP ports**; bind to port 0 and read back the assigned port.
  - **No fixed temp-file or directory names shared by several tests**;
    include the current gtest test name (`current_test_info()->name()`).
- Don't wait for "it's ready" with a fixed sleep; wait for the condition
  itself (with a bounded timeout).

## Dependencies

Third-party code lives in `extern/` as git submodules (see `.gitmodules`).
Do not edit files under `extern/`. Initialize a submodule lazily from
CMake with `nymph_ensure_submodules(extern/<name>)` inside the option block
that needs it, so a checkout without `--recursive` still configures.

## Claude Code Assist Behavioral Rules for This Repo

- Do not introduce broad stylistic rewrites.
- Do not change naming conventions in existing APIs.
- When adding new C++ files, mirror the style in nearby files.
- When adding bindings/tests, mirror style from sibling binding/test files.
- If style is ambiguous, prefer consistency with adjacent code over generic defaults.
