# Launcher — first run and extraction

This file documents the first-run flow and the extraction child process used to unpack a .nds.

- The extraction uses upstream's `Extract.Setup` in a child process: it prints questions and expects stdin answers. The child is run so the GUI does not block on `Console.ReadKey`.
- Consequences:
  - `-launcher` is dispatched before upstream's `CheckSetup` to avoid a "press any key" console stop.
  - `GameFiles.Problem()` signals the rest of the screen whether paths are missing or invalid.

Progress bar

- `SetupProgress` classifies each output line into a phase (writing files, unpacking, converting music, decompressing code) and moves asymptotically within that phase; total is unknown and a counting pass would require reading the cartridge twice.

UI behaviour

- During extraction the progress is drawn in the card; the console draws it with carriage returns only when stdout is a terminal. In a pipe or log the carriage return makes unreadable files.

ROM selection

- On Windows, Linux and macOS, choosing a ROM opens Fruity Prime's filesystem
  browser inside the existing launcher surface. The browser enumerates only
  the current directory, shows directories and `.nds` files, and validates a
  selected path before extraction.
- Android keeps the platform storage provider. Its real activity backend can
  return either a local path or a `content://` document; the existing setup
  flow copies non-local documents to app-owned temporary storage before
  extraction and removes that temporary copy afterward.
- Desktop must not request a native picker through Avalonia's
  `TopLevel.StorageProvider`: the desktop launcher uses Avalonia's headless
  backend and renders its `TopLevel` into the one GLFW/OpenTK game window.
  That top-level is never presented to the operating system, so it has no
  desktop platform dialog to open. Keeping selection in the existing surface
  preserves the one-window architecture across desktop targets.
- The browser's path field accepts typed paths and paste with Ctrl+V (Command+V
  on macOS). Paste reads the real GLFW window's clipboard and inserts into the
  Avalonia text field; the headless backend has no desktop clipboard of its own.
- The model is checked independently by
  `tools/rombrowsercheck/rombrowsercheck.csproj`, which links the current
  `RomFileBrowserModel.cs` and uses only temporary filesystem fixtures.
- On the desktop client, `-uishot DIR -browseronly` runs the browser
  smoke scenario through the headless dispatcher before game-file setup. It
  uses temporary fixtures to cover browser rows, navigation, selection, typed
  paths, resize and cancel, so this UI check runs without extracted assets or
  `paths.txt`.
