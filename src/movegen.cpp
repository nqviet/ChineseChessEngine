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
				if ((Pt == HORSE || Pt == CHARIOT || Pt == CANNON)
					&& !(PseudoAttacks[Pt][from] & target & pos.check_squares(Pt)))
					continue;

				if (pos.discovered_check_candidates() & from)
					continue;
			}

			Bitboard b;
			if (Pt == CANNON)
			{
				// attack and quiet moves
				b = pos.attacks_from<CANNON>(from) & target & pos.pieces(~pos.side_to_move());
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
		moveList = generate_moves<  CANNON, Checks>(pos, moveList, Us, target);
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

		Bitboard b = pos.attacks_from(Piece(pt), from) & ~pos.pieces();
		if (pt == CANNON)
		{
			b |= pos.attacks_from(Piece(CHARIOT), from) & ~pos.pieces();
		}

		while (b)
			*moveList++ = make_move(from, pop_lsb(&b));
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
		return moveList; // Double check, only a king move can save the day

	// Generate blocking evasions or captures of the checking piece
	Square checksq = lsb(pos.checkers());
	Bitboard target = (between_bb(checksq, ksq) | checksq) & (~pos.pieces(us));

	return us == WHITE ? generate_all<WHITE, EVASIONS>(pos, moveList, target)
		: generate_all<BLACK, EVASIONS>(pos, moveList, target);
}

/// generate<LEGAL> generates all the legal moves in the given position
template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* moveList) 
{	
	Bitboard pinned = pos.pinned_pieces(pos.side_to_move());
	Square ksq = pos.square<GENERAL>(pos.side_to_move());
	Bitboard kingFacedCannons = pos.attacks_from<CHARIOT>(ksq) & pos.pieces(~pos.side_to_move(), CANNON);	
	ExtMove* cur = moveList;

	moveList = pos.checkers() ? generate<EVASIONS>(pos, moveList)
		: generate<NON_EVASIONS>(pos, moveList);

	while (cur != moveList)
		if ((pinned || from_sq(*cur) == ksq || kingFacedCannons || pos.checkers())
			&& !pos.legal(*cur))
			*cur = (--moveList)->move;
		else
			++cur;

	return moveList;
}