#include <algorithm>
#include <cstring> // For std::memset, std::memcmp
#include <sstream>

#include "position.h"
#include "thread.h"
#include "tt.h"
#include "misc.h"

namespace PSQT 
{
	extern Score psq[PIECE_NB][SQUARE_NB];
}

namespace Zobrist 
{
	Key psq[PIECE_NB][SQUARE_NB];
	Key side;
}

namespace
{

const std::string PieceToChar(" PNBCRAK pnbcrak");

// min_attacker() is a helper function used by see() to locate the least
// valuable attacker for the side to move, remove the attacker we just found
// from the bitboards and scan for new X-ray attacks behind it.

template<int Pt>
PieceType min_attacker(const Bitboard* bb, Square to, Bitboard stmAttackers,
	Bitboard& occupied, Bitboard& attackers) 
{
	Bitboard b = stmAttackers & bb[Pt];
	if (!b)
		return min_attacker<Pt + 1>(bb, to, stmAttackers, occupied, attackers);

	occupied ^= b & ~(b - 1);

	if (Pt == SOLDIER || Pt == CHARIOT)
		attackers |= attacks_bb<CHARIOT>(to, occupied) & bb[CHARIOT];
	if (Pt == CANNON)
		attackers |= attacks_bb<CANNON>(to, occupied) & bb[CANNON];

	attackers &= occupied; // After X-ray that may add already processed pieces
	return (PieceType)Pt;
}

template<>
PieceType min_attacker<GENERAL>(const Bitboard*, Square, Bitboard, Bitboard&, Bitboard&) {
	return GENERAL; // No need to update bitboards: it is the last cycle
}

} // namespace

/// operator<<(Position) returns an ASCII representation of the position

std::ostream& operator<<(std::ostream& os, const Position& pos)
{
	std::string s = "";
	std::string c;
	for (Rank r = RANK_10; r >= RANK_1; --r)
	{		
		for (File f = FILE_A; f <= FILE_I; ++f)
		{
			c = PieceToChar[pos.piece_on(make_square(f, r))];
			s += c != " "
					? f != FILE_I ? c + "---" : c
					: f != FILE_I ? "----" : "-";
		}

		if (r == RANK_6)
			s += "\n|||||||||||||||||||||||||||||||||\n";
		else if (r == RANK_10 || r == RANK_3)
			s += "\n|   |   |   | \\ | / |   |   |   |\n";
		else if (r == RANK_9 || r == RANK_2)
			s += "\n|   |   |   | / | \\ |   |   |   |\n";
		else if (r != RANK_1)
			s += "\n|   |   |   |   |   |   |   |   |\n";
	}

	os << s;

	return os;
}

/// Position::init() initializes at startup the various arrays used to compute
/// hash keys.

void Position::init() 
{
	PRNG rng(1070372);

	for (Piece pc : Pieces)
		for (Square s = PT_A1; s <= PT_I8; ++s)
			Zobrist::psq[pc][s] = rng.rand<Key>();

	Zobrist::side = rng.rand<Key>();
}

/// Position::set() initializes the position object with the given FEN string.
/// This function is not very robust - make sure that input FENs are correct,
/// this is assumed to be the responsibility of the GUI.

Position& Position::set(const std::string& fenStr, StateInfo* si, Thread* th)
{
/*
	A FEN string defines a particular position using only the ASCII character set.

	A FEN string contains six fields separated by a space. The fields are:

	1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 10 and ending with rank 1. Within each rank, the contents of each
      square are described from file A through file I. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("PNBRAKC") whilst Black uses lowercase ("pnbrakc"). Blank points are
      noted using digits 1 through 9 (the number of blank squares), and "/"
      separates ranks.

	 2) Active color. "w" means white moves next, "b" means black.	 

	 3) Fullmove number. The number of the full move. It starts at 1, and is
	 incremented after Black's move.
*/
	unsigned char col, row, token;
	size_t idx;
	Square sq = PT_A10;
	std::istringstream ss(fenStr);

	std::memset(this, 0, sizeof(Position));
	std::memset(si, 0, sizeof(StateInfo));
	std::fill_n(&pieceList[0][0], sizeof(pieceList) / sizeof(Square), PT_NONE);
	st = si;

	ss >> std::noskipws;

	// 1. Piece placement
	while ((ss >> token) && !isspace(token))
	{
		if (isdigit(token))
			sq += Square(token - '0'); // Advance the given number of files
		else if (token == '/')
			sq -= Square(18);
		else if ((idx = PieceToChar.find(token)) != std::string::npos)
		{
			put_piece(Piece(idx), sq);
			++sq;
		}
	}

	// 2. Active color
	ss >> token;
	sideToMove = (token == 'w' ? WHITE : BLACK);
	ss >> token;	

	// 3. Halfmove clock and fullmove number
	ss >> std::skipws >> gamePly;

	thisThread = th;
	set_state(st);

	return *this;
}

/// Position::set_check_info() sets king attacks to detect if a move gives check

void Position::set_check_info(StateInfo* si) const 
{
	si->blockersForKing[WHITE] = slider_blockers(pieces(BLACK, CHARIOT), square<GENERAL>(WHITE), si->pinnersForKing[WHITE]);
	si->blockersForKing[BLACK] = slider_blockers(pieces(WHITE, CHARIOT), square<GENERAL>(BLACK), si->pinnersForKing[BLACK]);

	si->blockersForKing[WHITE] |= cannon_blockers(pieces(BLACK, CANNON), square<GENERAL>(WHITE), si->pinnersForKing[WHITE]);
	si->blockersForKing[BLACK] |= cannon_blockers(pieces(WHITE, CANNON), square<GENERAL>(BLACK), si->pinnersForKing[BLACK]);

	si->blockersForKing[WHITE] |= horse_blockers(pieces(BLACK, HORSE), square<GENERAL>(WHITE), si->fixedPinnersForKing[WHITE]);
	si->blockersForKing[BLACK] |= horse_blockers(pieces(WHITE, HORSE), square<GENERAL>(BLACK), si->fixedPinnersForKing[BLACK]);

	Square ksq = square<GENERAL>(~sideToMove);

	si->checkSquares[SOLDIER]	= attacks_from<SOLDIER>(ksq, ~sideToMove);
	si->checkSquares[HORSE]		= attacks_from<HORSE>(ksq);
	si->checkSquares[CANNON]	= attacks_from<CANNON>(ksq);
	si->checkSquares[CHARIOT]	= attacks_from<CHARIOT>(ksq);	
	si->checkSquares[GENERAL]	= 0;
}

/// Position::set_state() computes the hash keys of the position, and other
/// data that once computed is updated incrementally as moves are made.
/// The function is only used when a new position is set up, and to verify
/// the correctness of the StateInfo data when running in debug mode.

void Position::set_state(StateInfo* si) const
{
	si->key = si->pawnKey = si->materialKey = 0;
	si->nonPawnMaterial[WHITE] = si->nonPawnMaterial[BLACK] = VALUE_ZERO;
	si->psq = SCORE_ZERO;
	si->checkersBB = attackers_to(square<GENERAL>(sideToMove)) & pieces(~sideToMove);

	set_check_info(si);

	for (Bitboard b = pieces(); b; )
	{
		Square s = pop_lsb(&b);
		Piece pc = piece_on(s);
		si->key ^= Zobrist::psq[pc][s];
		si->psq += PSQT::psq[pc][s];
	}

	if (sideToMove == BLACK)
		si->key ^= Zobrist::side;

	for (Bitboard b = pieces(SOLDIER); b; )
	{
		Square s = pop_lsb(&b);
		si->pawnKey ^= Zobrist::psq[piece_on(s)][s];
	}

	for (Piece pc : Pieces)
	{
		if (type_of(pc) != SOLDIER && type_of(pc) != GENERAL)
			si->nonPawnMaterial[color_of(pc)] += pieceCount[pc] * PieceValue[MG][pc];

		for (int cnt = 0; cnt < pieceCount[pc]; ++cnt)
			si->materialKey ^= Zobrist::psq[pc][cnt];
	}
}

/// Position::fen() returns a FEN representation of the position.
/// This is mainly a debugging function.
const std::string Position::fen() const
{
	int emptyCnt;
	std::ostringstream ss;

	for (Rank r = RANK_10; r >= RANK_1; --r)
	{
		for (File f = FILE_A; f <= FILE_I; ++f)
		{
			for (emptyCnt = 0; f <= FILE_I && empty(make_square(f, r)); ++f)
				++emptyCnt;
			if (emptyCnt)
				ss << emptyCnt;
			if (f <= FILE_I)
				ss << PieceToChar[piece_on(make_square(f, r))];
		}

		if (r > RANK_1)
			ss << '/';
	}

	ss << (sideToMove == WHITE ? " w " : " b ");

	ss << "- - 0 1";

	return ss.str();
}

/// Position::game_phase() calculates the game phase interpolating total non-pawn
/// material between endgame and midgame limits.

Phase Position::game_phase() const 
{

	Value npm = st->nonPawnMaterial[WHITE] + st->nonPawnMaterial[BLACK];

	npm = std::max(EndgameLimit, std::min(npm, MidgameLimit));

	return Phase(((npm - EndgameLimit) * PHASE_MIDGAME) / (MidgameLimit - EndgameLimit));
}

/// Position::slider_blockers() returns a bitboard of all the pieces (both colors)
/// that are blocking attacks on the square 's' from 'sliders'. A piece blocks a
/// slider if removing that piece from the board would result in a position where
/// square 's' is attacked. For example, a king-attack blocking piece can be either
/// a pinned or a discovered check piece, according if its color is the opposite
/// or the same of the color of the slider.

Bitboard Position::slider_blockers(Bitboard sliders, Square s, Bitboard& pinners) const 
{
	Bitboard result = 0;
	pinners = 0;

	// Snipers are chariots that attack 's' when a piece is removed
	Bitboard snipers = (PseudoAttacks[CHARIOT][s]) & sliders;

	while (snipers)
	{
		Square sniperSq = pop_lsb(&snipers);
		Bitboard b = between_bb(s, sniperSq) & pieces();

		if (!more_than_one(b))
		{
			result |= b;
			if (b & pieces(color_of(piece_on(s))))
				pinners |= sniperSq;
		}
	}
	return result;
}

Bitboard Position::cannon_blockers(Bitboard sliders, Square s, Bitboard& pinners) const
{
	Bitboard result = 0;
	pinners = 0;

	// Snipers are cannons that attack 's' when a piece is removed
	Bitboard snipers = (PseudoAttacks[CHARIOT][s]) & sliders;

	while (snipers)
	{
		Square sniperSq = pop_lsb(&snipers);
		Bitboard b = between_bb(s, sniperSq) & pieces();

		int pcnt = popcount(b);
		if (pcnt == 2)
		{
			result |= b;			
			
			while (b)
			{
				Square c = pop_lsb(&b);
				if (SquareBB[c] & pieces(color_of(piece_on(s))))
					pinners |= c;
			}			
		}
	}
	return result;
}

Bitboard Position::horse_blockers(Bitboard sliders, Square s, Bitboard& pinners) const
{
	Bitboard result = 0;
	pinners = 0;

	// Snipers are horses that attack 's' when a piece is removed
	Bitboard snipers = (PseudoAttacks[HORSE][s]) & sliders;

	while (snipers)
	{
		Square sniperSq = pop_lsb(&snipers);

		Square dir = DIR_NONE;
		if (rank_of(s) == rank_of(sniperSq) + 2) dir = NORTH;
		else if (rank_of(s) == rank_of(sniperSq) - 2) dir = SOUTH;
		else if (file_of(s) == file_of(sniperSq) + 2) dir = EAST;
		else if (file_of(s) == file_of(sniperSq) - 2) dir = WEST;

		Bitboard b = SquareBB[sniperSq + dir] & pieces();
		if (b)
		{
			result |= b;
			if (b & pieces(color_of(piece_on(s))))
				pinners |= sniperSq;
		}
	}

	return result;
}

/// Position::attackers_to() computes a bitboard of all pieces which attack a
/// given square. Slider attacks use the occupied bitboard to indicate occupancy.

Bitboard Position::attackers_to(Square s, Bitboard occupied) const 
{

	return  (attacks_from<SOLDIER>(s, BLACK)    & pieces(WHITE, SOLDIER))
		| (attacks_from<SOLDIER	>(s, WHITE)		& pieces(BLACK, SOLDIER))
		| (attacks_bb<HORSE		>(s, occupied)	& pieces(HORSE))
		| (attacks_bb<CHARIOT	>(s, occupied)	& pieces(CHARIOT))
		| (attacks_bb<CANNON	>(s, occupied)	& pieces(CANNON))
		| (attacks_bb<ELEPHANT  >(s, occupied)	& pieces(ELEPHANT))
		| (attacks_from<ADVISOR	>(s)			& pieces(ADVISOR))
		| (attacks_from<GENERAL	>(s)			& pieces(GENERAL));
}

/// Position::legal() tests whether a pseudo-legal move is legal

bool Position::legal(Move m) const
{
	Color us = sideToMove;
	Square from = from_sq(m);
	Square to = to_sq(m);	

	// If the moving piece is a king, check whether the destination
	// square is attacked by the opponent.
	if (type_of(piece_on(from)) == GENERAL)
		return !(attackers_to(to) & pieces(~us));

	Square ksq = square<GENERAL>(us);
	Bitboard kingFacedCannons = attacks_from<CHARIOT>(ksq) & pieces(~us, CANNON);
	while (kingFacedCannons)
	{
		Square cannonSq = pop_lsb(&kingFacedCannons);
		if (between_bb(cannonSq, ksq) & to)
			return false;
	}

	if (checkers() && type_of(piece_on(lsb(checkers()))) == CANNON 
		&& (!((between_bb(lsb(checkers()), square<GENERAL>(us)) | checkers()) & to) || aligned(from, to, lsb(checkers()))))
		return false;

	if (fixedPinned_pieces(us) & from)
		return false;

	// A non-king move is legal if and only if it is not pinned or it
	// is moving along the ray towards or away from the king.	
	if((pinned_pieces(us) & from) && !aligned(from, to, square<GENERAL>(us)))
		return false;

	return !receives_cannon_check(m);

	//std::cout << "legal: " << ret << " checker: " << popcount(checkers())
	//								<< " cannon: " << bool( type_of(piece_on(lsb(checkers()))) == CANNON)
	//								<< " between-to: " << bool((between_bb(lsb(checkers()), square<GENERAL>(us)) | checkers()) & to)
	//								<< " align: " << bool (aligned(from, to, lsb(checkers())))
	//								<< std::endl;
}

/// Position::pseudo_legal() takes a random move and tests whether the move is
/// pseudo legal. It is used to validate moves from TT that can be corrupted
/// due to SMP concurrent access or hash position key aliasing.

bool Position::pseudo_legal(const Move m) const
{
	Color us = sideToMove;
	Square from = from_sq(m);
	Square to = to_sq(m);
	Piece pc = moved_piece(m);

	// If the 'from' square is not occupied by a piece belonging to the side to
	// move, the move is obviously not legal.
	if (pc == NO_PIECE || color_of(pc) != us)
		return false;

	// The destination square cannot be occupied by a friendly piece
	if (pieces(us) & to)
		return false;

	if (!(attacks_from(pc, from) & to))
		return false;	

	// Evasions generator already takes care to avoid some kind of illegal moves
	// and legal() relies on this. We therefore have to take care that the same
	// kind of moves are filtered out here.
	if (checkers())
	{
		if (type_of(pc) != GENERAL)
		{
			// Double check? In this case a king move is required
			if (more_than_one(checkers()))
				return false;

			// Our move must be a blocking evasion or a capture of the checking piece
			if (!((between_bb(lsb(checkers()), square<GENERAL>(us)) | checkers()) & to))				
				return false;
		}
		// In case of king moves under check we have to remove king so as to catch
		// invalid moves like b1a1 when opposite queen is on c1.
		else if (attackers_to(to, pieces() ^ from) & pieces(~us))
			return false;
	}

	return true;
}

/// Position::gives_check() tests whether a pseudo-legal move gives a check

bool Position::gives_check(Move m) const 
{
	Square from = from_sq(m);
	Square to = to_sq(m);

	// Is there a direct check?
	if (type_of(piece_on(from)) != CANNON && (st->checkSquares[type_of(piece_on(from))] & to))
		return true;
	
	if (gives_cannon_check(m))
		return true;

	// Is there a discovered check?
	else if ((discovered_check_candidates() & from)
		&& !aligned(from, to, square<GENERAL>(~sideToMove)))
		return true;

	return false;
}

/// Position::gives_check() tests whether a pseudo-legal cannon move gives a check

bool Position::gives_cannon_check(Move m) const
{
	Square from = from_sq(m);
	Square to = to_sq(m);
	Square ksq = square<GENERAL>(~sideToMove);

	Bitboard occupied = byTypeBB[ALL_PIECES] ^ from | to;
	Bitboard attackers = attacks_bb<CANNON>(ksq, occupied);
	Bitboard cannons = pieces(sideToMove, CANNON);
	if (type_of(piece_on(from)) == CANNON)
		cannons = cannons ^ from | to;

	return attackers & cannons;
}

/// Position::gives_check() tests whether a pseudo-legal cannon move receives a check

bool Position::receives_cannon_check(Move m) const
{
	Square from = from_sq(m);
	Square to = to_sq(m);
	Square ksq = square<GENERAL>(sideToMove);

	Bitboard occupied = byTypeBB[ALL_PIECES] ^ from | to;
	Bitboard attackers = attacks_bb<CANNON>(ksq, occupied);
	Bitboard cannons = pieces(~sideToMove, CANNON);
	if (type_of(piece_on(to)) == CANNON)
		cannons = cannons ^ to;

	return attackers & cannons;
}

/// Position::do_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
/// moves should be filtered out before this function is called.

void Position::do_move(Move m, StateInfo& newSt, bool givesCheck)
{
	//std::cout << "do_move: " << PieceToChar[piece_on(from_sq(m))] << "." << sqToStr(m) 
	//		<< " givesCheck: " << givesCheck
	//		<< std::endl;
	//std::cout << fen() << std::endl;
	//std::cout << *this << std::endl;

	++nodes;
	Key k = st->key ^ Zobrist::side;

	// Copy some fields of the old state to our new StateInfo object except the
	// ones which are going to be recalculated from scratch anyway and then switch
	// our state pointer to point to the new (ready to be updated) state.
	std::memcpy(&newSt, st, offsetof(StateInfo, key));
	newSt.previous = st;
	st = &newSt;

	// Increment ply counters. In particular, rule50 will be reset to zero later on
	// in case of a capture or a pawn move.
	++gamePly;	
	++st->pliesFromNull;

	Color us = sideToMove;
	Color them = ~us;
	Square from = from_sq(m);
	Square to = to_sq(m);
	Piece pc = piece_on(from);
	Piece captured = piece_on(to);

	if (captured)
	{
		Square capsq = to;

		// If the captured piece is a pawn, update pawn hash key, otherwise
		// update non-pawn material.
		if (type_of(captured) == SOLDIER)
			st->pawnKey ^= Zobrist::psq[captured][capsq];
		else
			st->nonPawnMaterial[them] -= PieceValue[MG][captured];

		// Update board and piece lists
		remove_piece(captured, capsq);

		// Update material hash key and prefetch access to materialTable
		k ^= Zobrist::psq[captured][capsq];
		st->materialKey ^= Zobrist::psq[captured][pieceCount[captured]];
#if 0
		prefetch(thisThread->materialTable[st->materialKey]);
#endif

		// Update incremental scores
		st->psq -= PSQT::psq[captured][capsq];
	}

	// Update hash key
	k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

	// Move the piece.
	move_piece(pc, from, to);

	// If the moving piece is a pawn do some special extra work
	if (type_of(pc) == SOLDIER)
	{
		// Update pawn hash key and prefetch access to pawnsTable
		st->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
#if 0
		prefetch(thisThread->pawnsTable[st->pawnKey]);
#endif
	}

	// Update incremental scores
	st->psq += PSQT::psq[pc][to] - PSQT::psq[pc][from];

	// Set capture piece
	st->capturedPiece = captured;

	// Update the key with the final value
	st->key = k;

	// Calculate checkers bitboard (if move gives check)
	st->checkersBB = givesCheck ? attackers_to(square<GENERAL>(them)) & pieces(us) : 0;

#ifdef _DEBUG
	if (square<GENERAL>(them) == PT_NONE)
	{
		std::cout << "ERROR" << std::endl;
		std::cout << *this << std::endl;
		assert(false);
	}
#endif

	sideToMove = ~sideToMove;

	// Update king attacks used for fast check detection
	set_check_info(st);
}

/// Position::undo_move() unmakes a move. When it returns, the position should
/// be restored to exactly the same state as before the move was made.

void Position::undo_move(Move m)
{
	sideToMove = ~sideToMove;

	Color us = sideToMove;
	Square from = from_sq(m);
	Square to = to_sq(m);
	Piece pc = piece_on(to);

	move_piece(pc, to, from); // Put the piece back at the source square

	if (st->capturedPiece)
	{
		Square capsq = to;
		put_piece(st->capturedPiece, capsq); // Restore the captured piece
	}

	// Finally point our state pointer back to the previous state
	st = st->previous;
	--gamePly;
}

/// Position::do(undo)_null_move() is used to do(undo) a "null move": It flips
/// the side to move without executing any move on the board.

void Position::do_null_move(StateInfo& newSt)
{
	std::memcpy(&newSt, st, sizeof(StateInfo));
	newSt.previous = st;
	st = &newSt;

	st->key ^= Zobrist::side;
	prefetch(TT.first_entry(st->key));

	st->pliesFromNull = 0;

	sideToMove = ~sideToMove;

	set_check_info(st);
}

void Position::undo_null_move()
{
	st = st->previous;
	sideToMove = ~sideToMove;
}

/// Position::key_after() computes the new hash key after the given move. Needed
/// for speculative prefetch. It doesn't recognize special moves like castling,
/// en-passant and promotions.

Key Position::key_after(Move m) const
{
	Square from = from_sq(m);
	Square to = to_sq(m);
	Piece pc = piece_on(from);
	Piece captured = piece_on(to);
	Key k = st->key ^ Zobrist::side;

	if (captured)
		k ^= Zobrist::psq[captured][to];

	return k ^ Zobrist::psq[pc][to] ^ Zobrist::psq[pc][from];
}

/// Position::see_ge (Static Exchange Evaluation Greater or Equal) tests if the
/// SEE value of move is greater or equal to the given value. We'll use an
/// algorithm similar to alpha-beta pruning with a null window.

bool Position::see_ge(Move m, Value v) const
{
	Square from = from_sq(m), to = to_sq(m);
	PieceType nextVictim = type_of(piece_on(from));
	Color stm = ~color_of(piece_on(from)); // First consider opponent's move
	Value balance; // Values of the pieces taken by us minus opponent's ones
	Bitboard occupied, stmAttackers;

	balance = PieceValue[MG][piece_on(to)];
	occupied = 0;

	if (balance < v)
		return false;

	if (nextVictim == GENERAL)
		return true;

	balance -= PieceValue[MG][nextVictim];

	if (balance >= v)
		return true;

	bool relativeStm = true; // True if the opponent is to move
	occupied ^= pieces() ^ from ^ to;

	// Find all attackers to the destination square, with the moving piece removed,
	// but possibly an X-ray attacker added behind it.
	Bitboard attackers = attackers_to(to, occupied) & occupied;

	while (true)
	{
		stmAttackers = attackers & pieces(stm);

		// Don't allow pinned pieces to attack pieces except the king as long all
		// pinners are on their original square.
		if (!(st->pinnersForKing[stm] & ~occupied))
			stmAttackers &= ~st->blockersForKing[stm];

		if (!stmAttackers)
			return relativeStm;

		// Locate and remove the next least valuable attacker
		nextVictim = min_attacker<SOLDIER>(byTypeBB, to, stmAttackers, occupied, attackers);

		if (nextVictim == GENERAL)
			return relativeStm == bool(attackers & pieces(~stm));

		balance += relativeStm ? PieceValue[MG][nextVictim]
			: -PieceValue[MG][nextVictim];

		relativeStm = !relativeStm;

		if (relativeStm == (balance >= v))
			return relativeStm;

		stm = ~stm;
	}
}

/// Position::is_draw() tests whether the position is drawn by repetition.
/// It does not detect stalemates.

bool Position::is_draw() const
{
	StateInfo* stp = st;
	for (int i = 2, e = st->pliesFromNull; i <= e; i += 2)
	{
		stp = stp->previous->previous;

		if (stp->key == st->key)
			return true; // Draw at first repetition
	}

	return false;
}