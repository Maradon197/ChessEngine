# ChessEngine

A bitboard chess engine in C++ with a Qt GUI.

```
core/      engine library (ChessCore): simple, runtime efficient implementation. At least in my dreams
console/   console test program (ChessEngineConsole): A leftover from early testing. Still works but wouldnt use
gui/       Qt widgets + Doorman (ChessGui): see gui/README.md
app/       the GUI program (ChessEngineGUI): the starter
```

Dependencies only point one way: `app` → `gui` → `core`

## Versions and build

CMake 3.19+, a C++20 compiler and, for the GUI, Qt 6.5+.

- **Visual Studio:** *File → Open → Folder…*, pick this folder. Tell CMake where Qt is, e.g. in `CMakeSettings.json` / a preset: `CMAKE_PREFIX_PATH=C:/Qt/6.x/msvc2022_64`.
- **Qt Creator:** *Open Project…* → `CMakeLists.txt`, choose a Qt 6 kit.
- **Without Qt:** configure with `-DCHESS_BUILD_GUI=OFF` to build only core and console.

To run `ChessEngineGUI.exe` outside the IDE, call `windeployqt` on it once so the Qt DLLs sit next to it.
