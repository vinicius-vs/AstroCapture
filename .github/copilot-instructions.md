# Copilot instructions for AstroCapture

## Project documentation first

Before making architectural, workflow, or design decisions, always review the repository documentation under `docs/`, especially:

- `docs/architecture.md`
- `docs/capture-workflow.md`
- `docs/decisions/*.md`

These files capture the project’s intended architecture, workflow constraints, and key design decisions. When a change affects behavior, interfaces, or capture flow, treat these documents as the source of truth before proposing or implementing a fix.

## Build, test, and validation

This repository is a Qt 6 + CMake desktop application. The project is configured from the root `CMakeLists.txt` and the CI workflow mirrors the same commands.

- Configure the build:
  - `cmake -S . -B build`
- Build the app:
  - `cmake --build build --parallel 2`
- Run the full test suite:
  - `ctest --test-dir build --output-on-failure`
- Run the targeted Qt test binary directly:
  - `./build/astrocapture_tests`
- Run a single CTest entry if needed:
  - `ctest --test-dir build --output-on-failure -R astrocapture_tests`

There is no separate project-specific lint script or formatter. The build already enables compiler warnings via CMake (`-Wall -Wextra -Wpedantic`), so treat warnings as part of the normal validation flow.

## High-level architecture

The codebase is intentionally small today but follows the architecture described in `README.md`:

- `src/app/`: application bootstrap and metadata/constants used across the app.
- `src/presentation/`: Qt widgets, windows, and UI logic (`MainWindow` is the current concrete example).
- `src/application/`: intended use-case orchestration layer for workflows such as session creation and capture control.
- `src/domain/`: business entities and rules, including session, capture, and camera-related concepts.
- `src/infrastructure/`: camera implementations, SQLite persistence, and image/file storage.

The project is designed around a layered architecture rather than UI logic being spread across the whole codebase. In other words: keep presentation concerns in `src/presentation`, keep orchestration in `src/application` or `src/app`, and keep hardware/data concerns in `src/infrastructure`.

The README also makes two important architectural constraints explicit:

- Camera access is abstracted behind a common interface, with a real `libgphoto2` path and a simulated path for development/testing.
- Long-running capture work should not block the UI thread; use Qt Signals/Slots and worker-style execution for time-consuming operations.

This is a Linux-first project, and the current CI workflow validates against Ubuntu with Qt 6.

## Key conventions in this repository

- CMake is the source of truth for build targets and dependencies. If you add a new source file, update the corresponding target in `CMakeLists.txt` rather than relying on hidden build rules.
- The project targets Qt 6 Widgets and uses the Qt test framework (`QtTest`) for unit coverage. Keep test files under `tests/unit/` and prefer `QTEST_MAIN` patterns like the existing `ApplicationMetadataTest`.
- Use `QStringLiteral` for UI text and metadata that is fixed at compile time; this matches the existing `application_metadata.h` and `main_window.cpp` pattern.
- Keep business logic out of `main_window.cpp` when it is more than trivial UI glue. The current code is a minimal start-up shell, but the intended direction is to separate the view from use cases and domain rules.
- For capture-related work, keep camera and persistence concerns separate from the GUI. The README explicitly calls out SQLite persistence, file storage, and camera abstraction as distinct infrastructure responsibilities.
- Use the existing naming style (`MainWindow`, `ApplicationMetadataTest`, `applicationTitle()`) unless a domain-specific module clearly requires a different pattern.

## Current project state

The repository is in its early staged implementation. The app currently boots a minimal Qt window and exposes the application title/version. The long-term roadmap in the README targets capture workflows, session state, SQLite-backed metadata, and later stacking/editing features.

When adding new functionality, prefer adapting the existing structure to the layered architecture described above rather than creating ad hoc logic directly in the UI layer.
