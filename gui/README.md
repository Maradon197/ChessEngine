# GUI – using the Doorman

`Doorman` is the only object the rest of the project talks to. Widgets register themselves with it; nothing touches a widget directly. `app/main.cpp` shows the complete wiring with the core:

```cpp
QApplication application(argc, argv);
Doorman doorman;
MainWindow window(doorman);   // creates and attaches all widgets
// connect core <-> doorman here (see app/main.cpp)
window.show();
return QApplication::exec();
```

The GUI depends on the core only through `DataTypes.hpp`; the core never includes anything from `gui/`.

## IN – core → GUI (public functions, callable from any thread)

| Area        | Functions                                                                                                                                                |
| ----------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Board       | `setPosition`, `playMove(move, label)`, `setLegalMoves`, `clearLegalMoves`, `setLastMove`, `clearLastMove`, `setBoardOrientation`, `setBoardInteractive` |
| Annotations | `setUserHighlights/Arrows`, `clearUserAnnotations`, `setEngineHighlights/Arrows`, `clearEngineAnnotations`                                               |
| Evaluation  | `setOverallCentipawns`, `setOverallMate`, `clearOverallEval`, `setCriterion(EvalCriterion, -3..3)`, `clearCriteria`                                      |
| Move tree   | `setTreeOrigin`, `addTreeNode(parent, move, position, label)` → id, `removeTreeNode`, `clearTree`, `setCurrentTreeNode`                                  |
| Principles  | `setPrinciples(list)`, `setPrincipleEnabled(title, enabled)`, `setPrincipleUseful(title, useful)`, `setPrincipleProblems(texts)`                       |
| Appearance  | `setThemeMode`, `setColor(ColorRole, 0xAARRGGBB)`, `resetColors`                                                                                         |
| Queries     | `getMovesToTreeNode(id)` (GUI thread), `getPosition()`, `getCurrentTreeNode()`, `getUserHighlights()`, `getUserArrows()`, `getThemeMode()`, `getColor()`, `getPrinciples()` |

## OUT – GUI → core (Qt signals, emitted in the GUI thread)

| Signal                                          | Meaning                                                                                                                                                                 |
| ----------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `moveAttempted(Move)`                           | User dragged/clicked a move. Not validated – verify it, then answer with `playMove`. Carries the full move (flags) if it was in the legal list, otherwise only from/to. |
| `squareSelected(Square)` / `selectionCleared()` | Piece picked up or dirty-selected / selection ended.                                                                                                                    |
| `userAnnotationsChanged(highlights, arrows)`    | User toggled a highlight/arrow (or cleared them with a left click).                                                                                                     |
| `treeNodeSelected(NodeId, Position)`            | User went to another move (M overlay or ←/→). The board already shows that position.                                                                                     |
| `principleEnabledChanged(title, bool)`        | User switched a principle on or off in the principle drawer (Ctrl+Space). `getPrinciples()` always returns the current choice.                                         |
| `principlesRefreshRequested()`                | User pressed Refresh in the principle drawer: reload the principle file and pass it to `setPrinciples`.                                                                |


## Rules

- Setters never emit signals, so reacting to a signal with setters cannot loop.
- The current tree node always holds the board position. `playMove` follows an existing child with the same move, otherwise it forks a new branch; `setPosition` overwrites the position stored in the current node (use it after `playMove` to send the core's authoritative position).
- `playMove` itself only shifts pieces on screen (incl. castle rook, en passant, promotion).
- `setPosition`/`playMove`/selecting a node clear the legal moves; send the new list afterwards.
- Labels: pass SAN to `playMove`/`addTreeNode`; without a label the tree shows coordinates (`e2e4`).
- `Highlight` and `Arrow` are distinct types; convert with `toSquare()` / `toMove()` (an arrow keeps only from/to bits).
- Node ids are assigned by the Doorman; `ROOT_NODE` (0) is the start position.
- Promotion picker: appears only if the legal list holds several promotion moves for the dropped square.
- New game: `clearTree()` then `setPosition(start)`.
- The move tree is an overlay shown while M is held: the cursor is hidden, mouse movement pans the tree and the node closest to the middle is selected (same as a click). ←/→ step through the tree when the overlay is closed.
- The principle drawer slides up on Ctrl+Space and hides again on Ctrl+Space, Esc or a click above it. The Target Elo slider switches off everything spotted below the target and switches on the rest. Principles come from `data/principles.json` (layout documented in the file, read by `util/PrincipleLoader`), identified by their unique title. `setPrinciples` keeps the enabled/useful state of titles it already knows. The difficulty tag follows from `spotElo`. Principles the engine hasn't marked `useful` show an orange "!".
