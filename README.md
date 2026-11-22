# ChessEngine

A bitboard chess engine in C++ with a GUI sponsored by Qt.

```
app/       the GUI program (ChessEngineGUI): the starter
core/      engine library (ChessCore): simple, runtime efficient implementation. At least in my dreams
util/      logic on top of the core that isn't needed for search (ChessUtil), e.g. move notation
engine/    the good stuff (or the bad stuff, for now). Split in search and evaluate, not using the principles yet.
gui/       Qt widgets + Doorman (ChessGui): see gui/README.md (slop section- I dont like making frontends)

```

## Versions and build

Required versions: CMake 3.19+, a C++20 compiler and Qt 6.5+. Runs best on Clang and MSVC.
*Move gen is decided by compiler and slower with gcc or your homebrewed compiler (you nerd).*

- **Visual Studio:** *File → Open → Folder…*, pick this folder. Tell CMake where Qt is, e.g. in `CMakeSettings.json` / a preset: `CMAKE_PREFIX_PATH=C:/Qt/6.x/msvc2022_64`.
- **Qt Creator:** *Open Project…* → `CMakeLists.txt`, choose a Qt 6 kit.
- **Without Qt:** configure with `-DCHESS_BUILD_GUI=OFF` to build only the core lib.


## AI disclaimer:
I used Generative AI as follows while writing code:
- UI creation and changes (Claude Opus 5.5)
- Research on Chess Engines (Claude Opus 5.5)

- Inline code suggestions across the project (Kimi K3 and GPT 5.6-Luna via GitHub Copilot)
- Code review and bugfixing
- Identifying performance issues 

AI was NOT used at all in design and conceptualization of the core or engine. 

What are you waiting for? Get the code, have fun and play the engine yourself!
