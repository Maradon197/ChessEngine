# Principles

## A. Opening

- Play theory → opening database
- Contest the center → attacks on central squares
- Develop knights before bishops → if both knights are still on their starting squares
- Don't move the same piece twice → list of pieces already moved in the opening
- Kick out enemy pieces → pawn attacks on pieces on ranks
- Don't bring the queen out too early → queen's share of attacks in the opponent's half
- Castle often and early → castle if possible
- Centralize rooks → rooks see each other; more points towards the d/e-files
- Use bishops to unpin knights → unmake pinned knight

## B. Middlegame

- If you're up material, trade → prefer equal trades if ahead in absolute (unweighted) material
- Know when to attack / defend (Rule of +2) → count attackers / defenders of the squares around the king

### B1. Attacking

- Create threats to the king, pieces and position → MVV/LVA principle: count and weight threats
- Keep attackers on the board → bonus for moves that keep king attackers on the board
- Bring / coordinate your pieces → bonus for attacking the same squares
- Play tempo-winning moves → define tempo (tempo eval?)
- Avoid counterplay → define tempo (tempo eval?)
- Vs. same-side castling: lock down the center → silent moves + high occupancy + few effective attacks in the center
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
