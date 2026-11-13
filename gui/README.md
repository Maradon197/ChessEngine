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
| Review      | `addReviewEntry(id, title, markdownText)`, `removeReviewEntry`, `clearReviewEntries`, `focusReviewEntry`                                                 |
| Move tree   | `setTreeOrigin`, `addTreeNode(parent, move, position, label)` → id, `removeTreeNode`, `clearTree`, `setCurrentTreeNode`                                  |
| Appearance  | `setThemeMode`, `setColor(ColorRole, 0xAARRGGBB)`, `resetColors`                                                                                         |
| Queries     | `getMovesToTreeNode(id)` (GUI thread), `getPosition()`, `getCurrentTreeNode()`, `getUserHighlights()`, `getUserArrows()`, `getThemeMode()`, `getColor()` |

## OUT – GUI → core (Qt signals, emitted in the GUI thread)

| Signal                                          | Meaning                                                                                                                                                                 |
| ----------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `moveAttempted(Move)`                           | User dragged/clicked a move. Not validated – verify it, then answer with `playMove`. Carries the full move (flags) if it was in the legal list, otherwise only from/to. |
| `squareSelected(Square)` / `selectionCleared()` | Piece picked up or dirty-selected / selection ended.                                                                                                                    |
| `userAnnotationsChanged(highlights, arrows)`    | User toggled a highlight/arrow (or cleared them with a left click).                                                                                                     |
| `treeNodeSelected(NodeId, Position)`            | User went to another move (click or ←/→). The board already shows that position.                                                                                        |
| `reviewEntryActivated(ReviewEntryId)`           | User clicked a review entry.                                                                                                                                            |


## Rules

- Setters never emit signals, so reacting to a signal with setters cannot loop.
- The current tree node always holds the board position. `playMove` follows an existing child with the same move, otherwise it forks a new branch; `setPosition` overwrites the position stored in the current node (use it after `playMove` to send the core's authoritative position).
- `playMove` itself only shifts pieces on screen (incl. castle rook, en passant, promotion).
- `setPosition`/`playMove`/selecting a node clear the legal moves; send the new list afterwards.
- Labels: pass SAN to `playMove`/`addTreeNode`; without a label the tree shows coordinates (`e2e4`).
- `Highlight` and `Arrow` are distinct types; convert with `toSquare()` / `toMove()` (an arrow keeps only from/to bits).
- Node ids are assigned by the Doorman; `ROOT_NODE` (0) is the start position. Review ids come from your core.
- Promotion picker: appears only if the legal list holds several promotion moves for the dropped square.
- New game: `clearTree()` then `setPosition(start)`.
