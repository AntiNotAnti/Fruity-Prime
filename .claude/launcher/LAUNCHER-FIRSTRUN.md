# Launcher — first run and extraction

This file documents the first-run flow and the extraction child process used to unpack a .nds.

- Path entries split at the first `=` only. Installation and external ROM
  directories may contain `=`; the local-server copy must preserve it too.

- The extraction uses upstream's `Extract.Setup` in a child process: it prints questions and expects stdin answers. The child is run so the GUI does not block on `Console.ReadKey`.
- Consequences:
  - `-launcher` is dispatched before upstream's `CheckSetup` to avoid a "press any key" console stop.
  - `GameFiles.Problem()` signals the rest of the screen whether paths are missing or invalid.

Game-file locations

- New ROM extractions under the installation's `files/` directory are recorded relative to `paths.txt`. Moving the installation with `paths.txt` and `files/` together therefore does not require extracting the ROM again.
- Older `paths.txt` files may contain absolute paths. If such a path is missing and matches an extracted game directory under the old `files/` layout, the reader uses the corresponding directory beside the current `paths.txt` when it exists. Existing absolute locations, including deliberately external game directories, are left alone.
- A separate local-server install receives only a copy of `paths.txt`, so its copied ROM entries are resolved to absolute paths pointing back at this installation's game files. The original `paths.txt` remains portable.
- Moving only the extracted `files/` directory to an arbitrary new location cannot be inferred automatically; keep it beside `paths.txt` or configure a new path.
- Run `dotnet run --project tools/pathstest/pathstest.csproj -c Release` for an asset-free regression that moves a temporary installation and checks both new and legacy paths.

Progress bar

- `SetupProgress` classifies each output line into a phase (writing files, unpacking, converting music, decompressing code) and moves asymptotically within that phase; total is unknown and a counting pass would require reading the cartridge twice.

UI behaviour

- During extraction the progress is drawn in the card; the console draws it with carriage returns only when stdout is a terminal. In a pipe or log the carriage return makes unreadable files.
