#include <cassert>

#include "movegen.h"
#include "position.h"

namespace
{
	template<PieceType Pt, bool Checks>
	ExtMove* generate_moves(const Position& pos, ExtMove* moveList, Color us,
		Bitboard target) {

		assert(Pt != GENERAL);

		const Square* pl = pos.squares<Pt>(us);

		for (Square from = *pl; from != PT_NONE; from = *++pl)
		{
			if (Checks)
			{
				if ((Pt == HORSE || Pt == CHARIOT)
					&& !(PseudoAttacks[Pt][from] & target & pos.check_squares(Pt)))
					continue;

				if (Pt == CANON
					&& !(PseudoAttacks[CHARIOT][from] & target & pos.check_squares(Pt)))
					continue;

				if (pos.discovered_check_candidates() & from)
					continue;

				// check if a piece at 'from' is a canon which faces to the King
				if (pos.attacks_from<CHARIOT>(pos.square<GENERAL>(~us)) & pos.pieces(us, CANON) & from)
					continue;
			}

			Bitboard b;
			if (Pt == CANON)
			{
				// attack and quiet moves
				b = pos.attacks_from<CANON>(from) & target & pos.pieces(~pos.side_to_move());
				b |= pos.attacks_from<CHARIOT>(from) & target & (~pos.pieces());
			}
			else if (Pt == SOLDIER)
				b = pos.attacks_from<SOLDIER>(from, us) & target;
			else
				b = pos.attacks_from<Pt>(from) & target;

			if (Checks)
				b &= pos.check_squares(Pt);

			while (b)
				*moveList++ = make_move(from, pop_lsb(&b));
		}

		return moveList;
	}

	template<Color Us, GenType Type>
	ExtMove* generate_all(const Position& pos, ExtMove* moveList, Bitboard target)
	{
		const bool Checks = Type == QUIET_CHECKS;

		ExtMove* pTmp = moveList;

		moveList = generate_moves< SOLDIER, Checks>(pos, moveList, Us, target);
		moveList = generate_moves<ELEPHANT, Checks>(pos, moveList, Us, target);
		moveList = generate_moves< ADVISOR, Checks>(pos, moveList, Us, target);
		moveList = generate_moves<   HORSE, Checks>(pos, moveList, Us, target);
		moveList = generate_moves<  CANON, Checks>(pos, moveList, Us, target);
		moveList = generate_moves< CHARIOT, Checks>(pos, moveList, Us, target);

		if (Type != QUIET_CHECKS && Type != EVASIONS)
		{
			Square ksq = pos.square<GENERAL>(Us);
			Bitboard b = pos.attacks_from<GENERAL>(ksq) & target;
			while (b)
				*moveList++ = make_move(ksq, pop_lsb(&b));
		}

		return moveList;
	}

} // namespace

/// generate<CAPTURES> generates all pseudo-legal captures.
/// Returns a pointer to the end of the move list.
///
/// generate<QUIETS> generates all pseudo-legal non-captures.
/// Returns a pointer to the end of the move list.
///
/// generate<NON_EVASIONS> generates all pseudo-legal captures and
/// non-captures. Returns a pointer to the end of the move list.

template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList)
{
	Color us = pos.side_to_move();

	Bitboard target = Type == CAPTURES ? pos.pieces(~us)
		: Type == QUIETS ? ~pos.pieces()
		: Type == NON_EVASIONS ? ~pos.pieces(us) : 0;

	return us == WHITE ? generate_all<WHITE, Type>(pos, moveList, target)
		: generate_all<BLACK, Type>(pos, moveList, target);
}

// Explicit template instantiations
template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<NON_EVASIONS>(const Position&, ExtMove*);

/// generate<QUIET_CHECKS> generates all pseudo-legal non-captures.
/// Returns a pointer to the end of the move list.
template<>
ExtMove* generate<QUIET_CHECKS>(const Position& pos, ExtMove* moveList)
{
	Color us = pos.side_to_move();
	Bitboard dc = pos.discovered_check_candidates();

	while (dc)
	{
		Square from = pop_lsb(&dc);
		PieceType pt = type_of(pos.piece_on(from));

		Bitboard b;
		if (pt == SOLDIER)
			b = pos.attacks_from<SOLDIER>(from, us)  & ~pos.pieces();
		else if (pt == CANON)
			b = pos.attacks_from(Piece(CHARIOT), from) & ~pos.pieces();
		else
			b = pos.attacks_from(Piece(pt), from) & ~pos.pieces();

		while (b)
		{
			Square to = pop_lsb(&b);
			if (pt == CHARIOT || pt == CANON || pt == SOLDIER)
			{
				if (!aligned(from, to, pos.square<GENERAL>(~us)))
					*moveList++ = make_move(from, to);
			}
			else
				*moveList++ = make_move(from, to);
		}
	}

	Bitboard canonFacingToKing = pos.attacks_from<CHARIOT>(pos.square<GENERAL>(~us)) & pos.pieces(us, CANON);
	if (canonFacingToKing)
	{
		Square canonFacingToKingSq = lsb(canonFacingToKing);
		Bitboard pieces = pos.pieces(us) ^ canonFacingToKingSq;
		while (pieces)
		{
			Square from = pop_lsb(&pieces);
			PieceType pt = type_of(pos.piece_on(from));

			Bitboard b;
			if (pt == SOLDIER)
				b = pos.attacks_from<SOLDIER>(from, us)  & ~pos.pieces();
			else if (pt == CANON)
				b = pos.attacks_from(Piece(CHARIOT), from) & ~pos.pieces();
			else
				b = pos.attacks_from(Piece(pt), from) & ~pos.pieces();

			while (b)
			{
				Square to = pop_lsb(&b);
				if (aligned(pos.square<GENERAL>(~us), canonFacingToKingSq, to))
					*moveList++ = make_move(from, to);
			}
		}
	}

	return us == WHITE ? generate_all<WHITE, QUIET_CHECKS>(pos, moveList, ~pos.pieces())
		: generate_all<BLACK, QUIET_CHECKS>(pos, moveList, ~pos.pieces());
}

/// generate<EVASIONS> generates all pseudo-legal check evasions when the side
/// to move is in check. Returns a pointer to the end of the move list.
template<>
ExtMove* generate<EVASIONS>(const Position& pos, ExtMove* moveList)
{

	Color us = pos.side_to_move();
	Square ksq = pos.square<GENERAL>(us);
	Bitboard sliderAttacks = 0;
	Bitboard sliders = pos.checkers() & ~pos.pieces(HORSE, SOLDIER);

	// Find all the squares attacked by slider checkers. We will remove them from
	// the king evasions in order to skip known illegal moves, which avoids any
	// useless legality checks later on.
	while (sliders)
	{
		Square checksq = pop_lsb(&sliders);
		sliderAttacks |= LineBB[checksq][ksq] ^ checksq;
	}

	// Generate evasions for king, capture and non capture moves
	Bitboard b = pos.attacks_from<GENERAL>(ksq) & ~pos.pieces(us) & ~sliderAttacks;
	while (b)
		*moveList++ = make_move(ksq, pop_lsb(&b));

	if (more_than_one(pos.checkers()))
	{
		Bitboard ch = pos.checkers();
		Square first = pop_lsb(&ch), second = pop_lsb(&ch);
		if (!aligned(first, second, pos.square<GENERAL>(us)))
			return moveList; // Double check, only a king move can save the day
	}

	// Generate blocking evasions or captures of the checking piece
	Square checksq = lsb(pos.checkers());
	Bitboard target = (between_bb(checksq, ksq) | checksq) & (~pos.pieces(us));

	// Generate blocking evasions of horse
	if (type_of(pos.piece_on(checksq)) == HORSE)
	{
		Square blockSq;
		if (distance<Rank>(checksq, ksq) == 2)
			blockSq = make_square(file_of(checksq), (rank_of(checksq) + rank_of(ksq)) / 2);
		else
			blockSq = make_square((file_of(checksq) + file_of(ksq)) / 2, rank_of(checksq));
		target |= blockSq;
	}
	// Generate blocking evasions of no trad of canon
	else if (type_of(pos.piece_on(checksq)) == CANON)
	{
		Bitboard trad = between_bb(checksq, ksq) & pos.pieces(us);
		Square tradSq = pop_lsb(&trad);
		Bitboard b = pos.attacks_from(pos.piece_on(tradSq), tradSq) & ~pos.pieces(us);
		while (b)
			*moveList++ = make_move(tradSq, pop_lsb(&b));
	}

	return us == WHITE ? generate_all<WHITE, EVASIONS>(pos, moveList, target)
		: generate_all<BLACK, EVASIONS>(pos, moveList, target);
}

/// generate<LEGAL> generates all the legal moves in the given position
template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* moveList)
{
	Color us = pos.side_to_move();
	Bitboard pinned = pos.pinned_pieces(us);
	Square ksq = pos.square<GENERAL>(us);
	Square theirKsq = pos.square<GENERAL>(~us);
	bool canonsFacingKing = pos.attacks_from<CHARIOT>(ksq) & pos.pieces(~us, CANON);
	bool flyingKingCandidate = popcount(between_bb(ksq, theirKsq) & pos.pieces()) == 1;
	ExtMove* cur = moveList;

	moveList = pos.checkers() ? generate<EVASIONS>(pos, moveList)
		: generate<NON_EVASIONS>(pos, moveList);

	while (cur != moveList)
		if ((pinned || from_sq(*cur) == ksq || canonsFacingKing || flyingKingCandidate || pos.checkers())
			&& !pos.legal(*cur))
			*cur = (--moveList)->move;
		else
			++cur;

	return moveList;
}
