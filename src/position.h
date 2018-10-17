#ifndef __POSITION_H__
#define __POSITION_H__

#include <deque>
#include <memory> // For std::unique_ptr

#include "bitboard.h"
#include "types.h"

/// StateInfo struct stores information needed to restore a Position object to
/// its previous state when we retract a move. Whenever a move is made on the
/// board (by calling Position::do_move), a StateInfo object must be passed.

struct StateInfo
{
	// Copied when making a move
	Key    pawnKey;
	Key    materialKey;
	Value  nonPawnMaterial[COLOR_NB];

	int    pliesFromNull;
	Score  psq;

	// Not copied when making a move (will be recomputed anyhow)
	Key        key;
	Bitboard   checkersBB;
	Piece      capturedPiece;
	StateInfo* previous;
	Bitboard   blockersForKing[COLOR_NB];
	Bitboard   pinnersForKing[COLOR_NB];
	Bitboard   fixedPinnersForKing[COLOR_NB];
	Bitboard   checkSquares[PIECE_TYPE_NB];
};

// In a std::deque references to elements are unaffected upon resizing
typedef std::unique_ptr<std::deque<StateInfo>> StateListPtr;

/// Position class stores information regarding the board representation as
/// pieces, side to move, hash keys, castling info, etc. Important methods are
/// do_move() and undo_move(), used by the search to update node info when
/// traversing the search tree.
class Thread;

class Position
{
public:
	static void init();

	Position() {}
	Position(const Position&) = delete;
	Position& operator=(const Position&) = delete;

	// FEN string input/output
	Position& set(const std::string& fenStr, StateInfo* si, Thread* th);
	const std::string fen() const;

	// Position representation
	Bitboard pieces() const;
	Bitboard pieces(PieceType pt) const;
	Bitboard pieces(PieceType pt1, PieceType pt2) const;
	Bitboard pieces(Color c) const;
	Bitboard pieces(Color c, PieceType pt) const;
	Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const;
	Piece piece_on(Square s) const;
	bool empty(Square s) const;
	template<PieceType Pt> int count(Color c) const;
	template<PieceType Pt> const Square* squares(Color c) const;
	template<PieceType Pt> Square square(Color c) const;

	// Checking
	Bitboard checkers() const;
	Bitboard discovered_check_candidates() const;
	Bitboard pinned_pieces(Color c) const;
	Bitboard fixedPinned_pieces(Color c) const;
	Bitboard check_squares(PieceType pt) const;

	// Properties of moves
	bool capture(Move m) const;
	bool advanced_pawn_push(Move m) const;
	Piece moved_piece(Move m) const;
	Piece captured_piece() const;

	// Piece specific
	bool pawn_passed(Color c, Square s) const;

	// Attacks to/from a given square
	Bitboard attackers_to(Square s) const;
	Bitboard attackers_to(Square s, Bitboard occupied) const;
	Bitboard horses_to(Square s, Bitboard occupied) const;
	Bitboard horses_to(Square s) const;
	Bitboard horseSq_to(Square s) const;
	Bitboard soldierSq_to(Square s, Color c) const;
	Bitboard attacks_from(Piece pc, Square s) const;
	template<PieceType> Bitboard attacks_from(Square s) const;
	template<PieceType> Bitboard attacks_from(Square s, Color c) const;
	Bitboard slider_blockers(Bitboard sliders, Square s, Bitboard& pinners) const;
	Bitboard canon_blockers(Bitboard sliders, Square s, Bitboard& pinners) const;
	Bitboard horse_blockers(Bitboard sliders, Square s, Bitboard& pinners) const;

	// Properties of moves
	bool legal(Move m) const;
	bool pseudo_legal(const Move m) const;
	bool gives_check(Move m) const;
	bool gives_canon_check(Move m) const;
	bool receives_canon_check(Move m) const;

	// Doing and undoing moves
	void do_move(Move m, StateInfo& st, bool givesCheck);
	void undo_move(Move m);
	void do_null_move(StateInfo& st);
	void undo_null_move();

	// Static Exchange Evaluation
	bool see_ge(Move m, Value value) const;

	// Accessing hash keys
	Key key() const;
	Key key_after(Move m) const;
	Key pawn_key() const;
	Key material_key() const;

	// Other properties of the position
	Phase game_phase() const;
	int game_ply() const;
	Color side_to_move() const;
	Score psq_score() const;
	Value non_pawn_material(Color c) const;
	Thread* this_thread() const;
	uint64_t nodes_searched() const;
	bool is_draw() const;

private:
	// Initialization helpers (used while setting up a position)
	void set_state(StateInfo* si) const;
	void set_check_info(StateInfo* si) const;

	// Other helpers
	void put_piece(Piece pc, Square s);
	void remove_piece(Piece pc, Square s);
	void move_piece(Piece pc, Square from, Square to);

	// Data members
	Piece board[SQUARE_NB];
	Bitboard byTypeBB[PIECE_TYPE_NB];
	Bitboard byColorBB[COLOR_NB];
	int pieceCount[PIECE_NB];
	Square pieceList[PIECE_NB][16];
	int index[SQUARE_NB];
	uint64_t nodes;
	int gamePly;
	Color sideToMove;
	Thread* thisThread;
	StateInfo* st;
};

extern std::ostream& operator<<(std::ostream& os, const Position& pos);

inline Color Position::side_to_move() const
{
	return sideToMove;
}

inline bool Position::empty(Square s) const
{
	return board[s] == NO_PIECE;
}

inline Piece Position::piece_on(Square s) const
{
	return board[s];
}

inline Piece Position::moved_piece(Move m) const
{
	return board[from_sq(m)];
}

inline Bitboard Position::pieces() const
{
	return byTypeBB[ALL_PIECES];
}

inline Bitboard Position::pieces(PieceType pt) const
{
	return byTypeBB[pt];
}

inline Bitboard Position::pieces(PieceType pt1, PieceType pt2) const
{
	return byTypeBB[pt1] | byTypeBB[pt2];
}

inline Bitboard Position::pieces(Color c) const
{
	return byColorBB[c];
}

inline Bitboard Position::pieces(Color c, PieceType pt) const
{
	return byColorBB[c] & byTypeBB[pt];
}

inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2) const
{
	return byColorBB[c] & (byTypeBB[pt1] | byTypeBB[pt2]);
}

template<PieceType Pt> inline int Position::count(Color c) const
{
	return pieceCount[make_piece(c, Pt)];
}

template<PieceType Pt> inline const Square* Position::squares(Color c) const
{
	return pieceList[make_piece(c, Pt)];
}

template<PieceType Pt> inline Square Position::square(Color c) const
{
	return pieceList[make_piece(c, Pt)][0];
}

template<PieceType Pt>
inline Bitboard Position::attacks_from(Square s) const
{
	return  Pt == CANON || Pt == CHARIOT || Pt == HORSE || Pt == ELEPHANT ? attacks_bb<Pt>(s, byTypeBB[ALL_PIECES])
		: StepAttacksBB[Pt][s];
}

template<>
inline Bitboard Position::attacks_from<SOLDIER>(Square s, Color c) const
{
	return StepAttacksBB[make_piece(c, SOLDIER)][s];
}

inline Bitboard Position::attacks_from(Piece pc, Square s) const
{
	return attacks_bb(pc, s, byTypeBB[ALL_PIECES]);
}

inline Bitboard Position::attackers_to(Square s) const
{
	return attackers_to(s, byTypeBB[ALL_PIECES]);
}

inline Bitboard Position::horses_to(Square s) const
{
  return horses_to(s, byTypeBB[ALL_PIECES]);
}

inline Bitboard Position::checkers() const
{
	return st->checkersBB;
}

inline Bitboard Position::discovered_check_candidates() const
{
	return st->blockersForKing[~sideToMove] & pieces(sideToMove);
}

inline Bitboard Position::pinned_pieces(Color c) const
{
	return st->blockersForKing[c] & pieces(c);
}

inline Bitboard Position::fixedPinned_pieces(Color c) const
{
	return st->fixedPinnersForKing[c];
}

inline Bitboard Position::check_squares(PieceType pt) const
{
	return st->checkSquares[pt];
}

inline bool Position::pawn_passed(Color c, Square s) const
{
	return !(pieces(~c, SOLDIER) & passed_pawn_mask(c, s));
}

inline bool Position::advanced_pawn_push(Move m) const
{
	return   type_of(moved_piece(m)) == SOLDIER
		&& relative_rank(sideToMove, from_sq(m)) > RANK_4;
}

inline Key Position::key() const
{
	return st->key;
}

inline Key Position::pawn_key() const
{
	return st->pawnKey;
}

inline Key Position::material_key() const
{
	return st->materialKey;
}

inline Score Position::psq_score() const
{
	return st->psq;
}

inline Value Position::non_pawn_material(Color c) const
{
	return st->nonPawnMaterial[c];
}

inline int Position::game_ply() const
{
	return gamePly;
}

inline uint64_t Position::nodes_searched() const
{
	return nodes;
}

inline bool Position::capture(Move m) const
{
	return !empty(to_sq(m));
}

inline Piece Position::captured_piece() const
{
	return st->capturedPiece;
}

inline Thread* Position::this_thread() const
{
	return thisThread;
}

inline void Position::put_piece(Piece pc, Square s)
{
	board[s] = pc;
	byTypeBB[ALL_PIECES] |= s;
	byTypeBB[type_of(pc)] |= s;
	byColorBB[color_of(pc)] |= s;
	index[s] = pieceCount[pc]++;
	pieceList[pc][index[s]] = s;
	pieceCount[make_piece(color_of(pc), ALL_PIECES)]++;
}

inline void Position::remove_piece(Piece pc, Square s) {

	// WARNING: This is not a reversible operation. If we remove a piece in
	// do_move() and then replace it in undo_move() we will put it at the end of
	// the list and not in its original place, it means index[] and pieceList[]
	// are not invariant to a do_move() + undo_move() sequence.
	byTypeBB[ALL_PIECES] ^= s;
	byTypeBB[type_of(pc)] ^= s;
	byColorBB[color_of(pc)] ^= s;
	/* board[s] = NO_PIECE;  Not needed, overwritten by the capturing one */
	Square lastSquare = pieceList[pc][--pieceCount[pc]];
	index[lastSquare] = index[s];
	pieceList[pc][index[lastSquare]] = lastSquare;
	pieceList[pc][pieceCount[pc]] = PT_NONE;
	pieceCount[make_piece(color_of(pc), ALL_PIECES)]--;
}

inline void Position::move_piece(Piece pc, Square from, Square to)
{

	// index[from] is not updated and becomes stale. This works as long as index[]
	// is accessed just by known occupied squares.
	Bitboard from_to_bb = SquareBB[from] ^ SquareBB[to];
	byTypeBB[ALL_PIECES] ^= from_to_bb;
	byTypeBB[type_of(pc)] ^= from_to_bb;
	byColorBB[color_of(pc)] ^= from_to_bb;
	board[from] = NO_PIECE;
	board[to] = pc;
	index[to] = index[from];
	pieceList[pc][index[to]] = to;
}

#endif
