# ChessEngine

A bitboard chess engine in C++ with a Qt GUI.

```
app/       the GUI program (ChessEngineGUI): the starter
core/      engine library (ChessCore): simple, runtime efficient implementation. At least in my dreams
util/      logic on top of the core that isn't needed for search (ChessUtil), e.g. move notation
gui/       Qt widgets + Doorman (ChessGui): see gui/README.md (slop section- I dont like making frontends)

```

Dependencies only point one way: `app` → `gui` → `core`

## Versions and build

CMake 3.19+, a C++20 compiler and, for the GUI, Qt 6.5+.

- **Visual Studio:** *File → Open → Folder…*, pick this folder. Tell CMake where Qt is, e.g. in `CMakeSettings.json` / a preset: `CMAKE_PREFIX_PATH=C:/Qt/6.x/msvc2022_64`.
- **Qt Creator:** *Open Project…* → `CMakeLists.txt`, choose a Qt 6 kit.
- **Without Qt:** configure with `-DCHESS_BUILD_GUI=OFF` to build only the core lib.

To run `ChessEngineGUI.exe` outside the IDE, call `windeployqt` on it once so the Qt DLLs sit next to it.
