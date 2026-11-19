# Principles
id: free enum value
title: This principle is about ...
spotElo: chess.com rapid
description: We do this because...
```cpp
enum class GamePart { OPENING, MIDGAME, ATTACKING_MIDGAME, ENDGAME };
enum class PrincipleFocus { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, SPACE, STRATEGY, COORDINATION };
``` 

## A. Opening

- Play theory → opening database
- Contest the center → attacks on central squares
	- id: CONTEST_THE_CENTER
	- title: "Contesting the Center"
	- spotElo: 300
	- description: "Controlling central space eases development"
	- gamePart: OPENING, MIDGAME
	- focus: SPACE
- Develop knights before bishops → if both knights are still on their starting squares
	- id: DEVELOP_KNIGHTS_BEFORE_BISHOPS
	- title: "Developing knights before bishops"
	- spotElo: 1000
	- description: "Bishops are more valuable down the line"
	- gamePart: OPENING
	- focus: COORDINATION
- Don't move the same piece twice → list of pieces already moved in the opening
	- id: DONT_MOVE_SAME_PIECE_TWICE
	- title: "Dont't move the same piece twice"
	- spotElo: 650
	- description: "Developing other pieces has more value"
	- gamePart: OPENING
	- focus: COORDINATION
- Kick out enemy pieces → pawn attacks on pieces on ranks
	- id: KICK_OUT_PIECES
	- title: "Kick out enemy pieces"
	- spotElo: 400
	- description: "Forcing back opponent's pieces gains space"
	- gamePart: OPENING, MIDGAME
	- focus: SPACE
- Don't bring the queen out too early → queen's share of attacks in the opponent's half, queen moves in early plies
	- id: NO_EARLY_QUEEN_MOVES
	- title: "Don't bring the queen out too early"
	- spotElo: 850
	- description: "Developing pieces gains space more sustainably"
	- gamePart: OPENING
	- focus: QUEEN
- Castle often and early → castle if possible
	- id: CASTLE_OFTEN_EARLY
	- title: "Castle often, castle early"
	- spotElo: 400
	- description: "Castling gets your king to safety"
	- gamePart: OPENING, MIDGAME
	- focus: KING
- Centralize rooks → rooks see each other; more points towards the d/e-files
	- id: CENTRALIZE_ROOKS
	- title: "Centralize your rooks"
	- spotElo: 1100
	- description: "Centralizing your rooks supports your position"
	- gamePart: OPENING, MIDGAME
	- focus: ROOK
- Use bishops to unpin knights → unmake pinned knight
	- id: UNPIN_KNIGHTS_WITH_BISHOPS
	- title: "Use bishops to unpin knights"
	- spotElo: 350
	- description: "Remove pressure from your knights"
	- gamePart: OPENING
	- focus: BISHOP

## B. Middlegame

- If you're up material, trade → prefer equal trades if ahead in absolute (unweighted) material
	- id: TRADE_WHEN_UP_MATERIAL
	- title: "Trade if you're up material"
	- spotElo: 600
	- description: "It's the simplest way to convert a winning position"
	- gamePart: MIDGAME, ENDGAME
	- focus: STRATEGY
- Know when to attack / defend (Rule of +2) → count attackers / defenders of the squares around the king
	- id: RULE_OF_PLUS_TWO
	- title: "Know when to attack (Rule of +2)"
	- spotElo: 1250
	- description: "A successful attack usually requires at least 2 more attackers than defenders"
	- gamePart: MIDGAME
	- focus: COORDINATION

### B1. Attacking

- Create threats to the king, pieces and position → MVV/LVA principle: count and weight threats
	- id: CREATE_THREATS
	- title: "Create threats to the enemy"
	- spotElo: 200
	- description: "Creating threats to the enemy king, pieces or position is the only way to win"
	- gamePart: OPENING, MIDGAME, END_GAME
	- focus: STRATEGY
- Keep attackers on the board → bonus for moves that keep king attackers on the board
	- id: KEEP_ATTACKERS
	- title: "Keep vital attackers on the board"
	- spotElo: 1350
	- description: "Without sufficient material presence, your attack can be stopped easily"
	- gamePart: ATTACKING_MIDGAME
	- focus: STRATEGY
- Bring / coordinate your pieces → bonus for attacking the same squares
	- id: COORDINATE_ATTACKERS
	- title: "Coordinate your attack"
	- spotElo: 1400
	- description: "Bring and coordinate your attackers to overwhelm the enemy position"
	- gamePart: ATTACKING_MIDGAME
	- focus: COORDINATION
- Play tempo-winning moves → define tempo (tempo eval?)
	- id: TEMPO_WINNING_MOVES
	- title: "Play tempo-winning moves"
	- spotElo: 1000
	- description: "Momentum allows you to take control over the game"
	- gamePart: MIDGAME, ENDGAME
	- focus: STRATEGY
- Vs. same-side castling: lock down the center → silent moves + high occupancy + few effective attacks in the center
	- id: LOCK_DOWN_CENTER
	- title: "Attack the same-side castled king"
	- spotElo: 1400
	- description: "Lock down the center to avoid enemy counterplay"
	- gamePart: ATTACKING_MIDGAME
	- focus: 
- Vs. opposite-side castling: open the center + pawn storm → pawn pushes + captures in the center, low occupancy
- Vs. uncastled king: castle, bring pieces
- Create rook lifts → see rooks; prefer 3rd rank if attacking

### B2. Defending

- ~~Acknowledge and deal with threats~~ → search + weighted material
- ~~Trade off crucial attackers~~ → inverse of attacking
- ~~Win back tempo~~ → tempo eval?
- ~~Create counterplay~~ → same as tempo

### B3. General position-improving moves

#### 1) Pawns

- Take space from the opponent's pieces → prefer attacks on squares the opponent attacks
- Don't weaken pawn chains → pawn structure strength eval?
- ~~Avoid (uninfluential) doubled pawns~~ → pawn structure strength
- Create pawn breaks → identify break squares
- Attack enemy backward pawns → backward pawn: neighbouring pawns are ahead, not on the a/h-file
- Stave off enemy isolated pawns → prefer less movement on isolated pawns
- ~~Avoid and reinforce backward pawns~~ → see isolated pawns
- ~~Keep play dynamic around isolated pawns~~ → see isolated pawns
- Create space for the king → 3 pawns in front of the king: bad
- c- and d-pawns go together → prefer d4 together with c4
- Keep files to your king closed → prefer a+b / g+h pawns
- ~~Open up files to the opponent's king~~ → see above
- ~~Create fianchetto squares~~
- ~~Create and reinforce outpost squares~~ → ~~outpost square: no enemy pawns on the adjacent files, attacked by own pawn~~
- Create pawn storms with the h- and f-pawns → prefer h4–h5 or f4 when the opponent has castled kingside

#### 2) Knights

- ~~Develop knights to the center~~ → piece-weighted values
- ~~Prefer knights over bishops in closed positions~~ → piece-weighted values (positional eval?)
- Use outpost squares → no enemy pawns, attacked by own pawn
- Reroute knights that are stuck → stuck knight: heavy enemy attacks / pawn attacks on the squares it attacks
- ~~Prefer bishops over knights by default~~ (positional eval?)
- Use blockade squares → blockade square: in front of a pawn + can't be kicked out in 1 move

#### 3) Bishops

- ~~Place bishops on strong diagonals (few / weak pawns)~~ → weighted material
- Use fianchetto squares → fianchetto square: b2/g2 + undeveloped bishop
- Create queen batteries → same diagonal as the queen
- ~~Place bishops on diagonals with valuable victims~~ → weighted material
- Create pins → pin: 1 enemy piece in front of the MVV
- ~~Preserve the bishop pair~~ → weighted material
- Use blockade squares a knight can't take → blockade square + no knight attacks

#### 4) Rooks

- Place rooks on open files → no enemy pawn occupancy
- ~~Don't block in rooks~~ → weighted material
- Place rooks on semi-open files with valuable victims → max. 1 pawn in total, king / queen as well

## C. Endgame

- If you're up material, trade → prefer equal trades if pure value (no weights) is higher
- Bring your king in iff the queens are off the board → key squares good iff no queens
- Create passed pawns → no pawns ahead
- Avoid doubled pawns → 2 pawns on the same file
- Prefer pawns of the opponent's bad bishop
- ~~Take key squares~~
- Push passed pawns
- Rooks behind passed pawns
- King off the back rank
