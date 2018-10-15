#include <algorithm>
#include <cassert>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"

namespace
{
	// Table used to drive the king towards the edge of the board
	// in KX vs K and KQ vs KR endgames.
	const int PushToEdges[SQUARE_NB] = {
		100, 90, 80, 70, 70, 80, 90, 100,
		90, 70, 60, 50, 50, 60, 70,  90,
		80, 60, 40, 30, 30, 40, 60,  80,
		70, 50, 30, 20, 20, 30, 50,  70,
		70, 50, 30, 20, 20, 30, 50,  70,
		80, 60, 40, 30, 30, 40, 60,  80,
		90, 70, 60, 50, 50, 60, 70,  90,
		100, 90, 80, 70, 70, 80, 90, 100
	};

	// Table used to drive the king towards a corner square of the
	// right color in KBN vs K endgames.
	const int PushToCorners[SQUARE_NB] = {
		200, 190, 180, 170, 160, 150, 140, 130,
		190, 180, 170, 160, 150, 140, 130, 140,
		180, 170, 155, 140, 140, 125, 140, 150,
		170, 160, 140, 120, 110, 140, 150, 160,
		160, 150, 140, 110, 120, 140, 160, 170,
		150, 140, 125, 140, 140, 155, 170, 180,
		140, 130, 140, 150, 160, 170, 180, 190,
		130, 140, 150, 160, 170, 180, 190, 200
	};

	// Tables used to drive a piece towards or away from another piece
	const int PushClose[8] = { 0, 0, 100, 80, 60, 40, 20, 10 };
	const int PushAway[8] = { 0, 5, 20, 40, 60, 80, 90, 100 };

	// Pawn Rank based scaling factors used in KRPPKRP endgame
	const int KRPPKRPScaleFactors[RANK_NB] = { 0, 9, 10, 14, 21, 44, 0, 0 };

	bool verify_material(const Position& pos, Color c, Value npm, int pawnsCnt) 
	{
		return pos.non_pawn_material(c) == npm && pos.count<SOLDIER>(c) == pawnsCnt;
	}

	// Map the square as if strongSide is white and strongSide's only pawn
	// is on the left half of the board.
	Square normalize(const Position& pos, Color strongSide, Square sq) {		

		if (file_of(pos.square<SOLDIER>(strongSide)) >= FILE_E)
			sq = Square(sq ^ 7); // Mirror SQ_H1 -> SQ_A1

		if (strongSide == BLACK)
			sq = ~sq;

		return sq;
	}

	// Get the material key of Position out of the given endgame key code
	// like "KBPKN". The trick here is to first forge an ad-hoc FEN string
	// and then let a Position object do the work for us.
	Key key(const std::string& code, Color c) {

		//assert(code.length() > 0 && code.length() < 8);
		//assert(code[0] == 'K');

		std::string sides[] = { code.substr(code.find('K', 1)),      // Weak
			code.substr(0, code.find('K', 1)) }; // Strong

		std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower);

		std::string fen = sides[0] + char(8 - sides[0].length() + '0') + "/8/8/8/8/8/8/"
			+ sides[1] + char(8 - sides[1].length() + '0') + " w - - 0 10";

		StateInfo st;
		return Position().set(fen, &st, nullptr).material_key();
	}
} // namespace

/// Endgames members definitions

Endgames::Endgames() 
{

	add<KPK>("KPK");
	add<KNNK>("KNNK");
	add<KBNK>("KBNK");
	add<KRKP>("KRKP");
	add<KRKB>("KRKB");
	add<KRKN>("KRKN");
	add<KQKP>("KQKP");
	add<KQKR>("KQKR");

	add<KNPK>("KNPK");
	add<KNPKB>("KNPKB");
	add<KRPKR>("KRPKR");
	add<KRPKB>("KRPKB");
	add<KBPKB>("KBPKB");
	add<KBPKN>("KBPKN");
	add<KBPPKB>("KBPPKB");
	add<KRPPKRP>("KRPPKRP");
}

template<EndgameType E, typename T>
void Endgames::add(const std::string& code) 
{
	map<T>()[key(code, WHITE)] = std::unique_ptr<EndgameBase<T>>(new Endgame<E>(WHITE));
	map<T>()[key(code, BLACK)] = std::unique_ptr<EndgameBase<T>>(new Endgame<E>(BLACK));
}

/// Mate with KX vs K. This function is used to evaluate positions with
/// king and plenty of material vs a lone king. It simply gives the
/// attacking side a bonus for driving the defending king towards the edge
/// of the board, and for keeping the distance between the two kings small.
template<>
Value Endgame<KXK>::operator()(const Position& pos) const
{
	assert(verify_material(pos, weakSide, VALUE_ZERO, 0));
	assert(!pos.checkers()); // Eval is never called when in check

	// Stalemate detection with lone king
	if (pos.side_to_move() == weakSide && !MoveList<LEGAL>(pos).size())
		return VALUE_DRAW;

	Square winnerKSq = pos.square<GENERAL>(strongSide);
	Square loserKSq = pos.square<GENERAL>(weakSide);

	Value result = pos.non_pawn_material(strongSide)
		+ pos.count<SOLDIER>(strongSide) * SoldierValueEg
		+ PushToEdges[loserKSq]
		+ PushClose[distance(winnerKSq, loserKSq)];

	if (pos.count<CANON>(strongSide)
		|| pos.count<CHARIOT>(strongSide)
		|| (pos.count<ELEPHANT>(strongSide) && pos.count<HORSE>(strongSide))
		|| (pos.count<ELEPHANT>(strongSide) > 1 && pos.squares<ELEPHANT>(strongSide)[1]))
		result = static_cast<Value>(std::min(static_cast<int>(result + VALUE_KNOWN_WIN), static_cast<int>(VALUE_MATE_IN_MAX_PLY - 1)));

	return strongSide == pos.side_to_move() ? result : -result;
}

/// Mate with KBN vs K. This is similar to KX vs K, but we have to drive the
/// defending king towards a corner square of the right color.
template<>
Value Endgame<KBNK>::operator()(const Position& pos) const
{
	assert(verify_material(pos, strongSide, HorseValueMg + ElephantValueMg, 0));
	assert(verify_material(pos, weakSide, VALUE_ZERO, 0));

	Square winnerKSq = pos.square<GENERAL>(strongSide);
	Square loserKSq = pos.square<GENERAL>(weakSide);
	Square bishopSq = pos.square<GENERAL>(strongSide);

	Value result = VALUE_KNOWN_WIN
		+ PushClose[distance(winnerKSq, loserKSq)]
		+ PushToCorners[loserKSq];

	return strongSide == pos.side_to_move() ? result : -result;
}

/// KP vs K. This endgame is evaluated with the help of a bitbase.
template<>
Value Endgame<KPK>::operator()(const Position& pos) const
{
	assert(verify_material(pos, strongSide, VALUE_ZERO, 1));
	assert(verify_material(pos, weakSide, VALUE_ZERO, 0));

	// Assume strongSide is white and the pawn is on files A-D
	Square wksq = normalize(pos, strongSide, pos.square<GENERAL>(strongSide));
	Square bksq = normalize(pos, strongSide, pos.square<GENERAL>(weakSide));
	Square psq = normalize(pos, strongSide, pos.square<SOLDIER>(strongSide));

	Color us = strongSide == pos.side_to_move() ? WHITE : BLACK;

	if (!Bitbases::probe(wksq, psq, bksq, us))
		return VALUE_DRAW;

	Value result = VALUE_KNOWN_WIN + SoldierValueEg + Value(rank_of(psq));

	return strongSide == pos.side_to_move() ? result : -result;
}

/// KR vs KP. This is a somewhat tricky endgame to evaluate precisely without
/// a bitbase. The function below returns drawish scores when the pawn is
/// far advanced with support of the king, while the attacking king is far
/// away.
template<>
Value Endgame<KRKP>::operator()(const Position& pos) const
{
	assert(verify_material(pos, strongSide, ChariotValueMg, 0));
	assert(verify_material(pos, weakSide, VALUE_ZERO, 1));

	Square wksq = relative_square(strongSide, pos.square<GENERAL>(strongSide));
	Square bksq = relative_square(strongSide, pos.square<GENERAL>(weakSide));
	Square rsq = relative_square(strongSide, pos.square<CHARIOT>(strongSide));
	Square psq = relative_square(strongSide, pos.square<SOLDIER>(weakSide));

	Square queeningSq = make_square(file_of(psq), RANK_1);
	Value result;

	// If the stronger side's king is in front of the pawn, it's a win
	if (wksq < psq && file_of(wksq) == file_of(psq))
		result = static_cast<Value>(ChariotValueEg - distance(wksq, psq));

	// If the weaker side's king is too far from the pawn and the rook,
	// it's a win.
	else if (distance(bksq, psq) >= 3 + (pos.side_to_move() == weakSide)
		&& distance(bksq, rsq) >= 3)
		result = static_cast<Value>(ChariotValueEg - distance(wksq, psq));

	// If the pawn is far advanced and supported by the defending king,
	// the position is drawish
	else if (rank_of(bksq) <= RANK_3
		&& distance(bksq, psq) == 1
		&& rank_of(wksq) >= RANK_4
		&& distance(wksq, psq) > 2 + (pos.side_to_move() == strongSide))
		result = static_cast<Value>(Value(80) - 8 * distance(wksq, psq));

	else
		result = static_cast<Value>(Value(200) - 8 * (distance(wksq, psq + SOUTH)
			- distance(bksq, psq + SOUTH)
			- distance(psq, queeningSq)));

	return strongSide == pos.side_to_move() ? result : -result;
}

/// KR vs KB. This is very simple, and always returns drawish scores.  The
/// score is slightly bigger when the defending king is close to the edge.
template<>
Value Endgame<KRKB>::operator()(const Position& pos) const 
{

	assert(verify_material(pos, strongSide, ChariotValueMg, 0));
	assert(verify_material(pos, weakSide, ElephantValueMg, 0));

	Value result = Value(PushToEdges[pos.square<GENERAL>(weakSide)]);
	return strongSide == pos.side_to_move() ? result : -result;
}

/// KR vs KN. The attacking side has slightly better winning chances than
/// in KR vs KB, particularly if the king and the knight are far apart.
template<>
Value Endgame<KRKN>::operator()(const Position& pos) const {

	assert(verify_material(pos, strongSide, ChariotValueMg, 0));
	assert(verify_material(pos, weakSide, HorseValueMg, 0));

	Square bksq = pos.square<GENERAL>(weakSide);
	Square bnsq = pos.square<HORSE>(weakSide);
	Value result = Value(PushToEdges[bksq] + PushAway[distance(bksq, bnsq)]);
	return strongSide == pos.side_to_move() ? result : -result;
}

/// KQ vs KP. In general, this is a win for the stronger side, but there are a
/// few important exceptions. A pawn on 7th rank and on the A,C,F or H files
/// with a king positioned next to it can be a draw, so in that case, we only
/// use the distance between the kings.
template<>
Value Endgame<KQKP>::operator()(const Position& pos) const 
{

	assert(verify_material(pos, strongSide, ChariotValueMg, 0));
	assert(verify_material(pos, weakSide, VALUE_ZERO, 1));

	Square winnerKSq = pos.square<GENERAL>(strongSide);
	Square loserKSq = pos.square<GENERAL>(weakSide);
	Square pawnSq = pos.square<SOLDIER>(weakSide);

	Value result = Value(PushClose[distance(winnerKSq, loserKSq)]);

	if (relative_rank(weakSide, pawnSq) != RANK_7
		|| distance(loserKSq, pawnSq) != 1
		|| !((FileABB | FileCBB | FileFBB | FileHBB) & pawnSq))
		result += ChariotValueEg - SoldierValueEg;

	return strongSide == pos.side_to_move() ? result : -result;
}

/// KQ vs KR.  This is almost identical to KX vs K:  We give the attacking
/// king a bonus for having the kings close together, and for forcing the
/// defending king towards the edge. If we also take care to avoid null move for
/// the defending side in the search, this is usually sufficient to win KQ vs KR.
template<>
Value Endgame<KQKR>::operator()(const Position& pos) const 
{

	assert(verify_material(pos, strongSide, ChariotValueMg, 0));
	assert(verify_material(pos, weakSide, CanonValueMg, 0));

	Square winnerKSq = pos.square<GENERAL>(strongSide);
	Square loserKSq = pos.square<GENERAL>(weakSide);

	Value result = ChariotValueEg
		- CanonValueEg
		+ PushToEdges[loserKSq]
		+ PushClose[distance(winnerKSq, loserKSq)];

	return strongSide == pos.side_to_move() ? result : -result;
}

/// Some cases of trivial draws
template<> Value Endgame<KNNK>::operator()(const Position&) const { return VALUE_DRAW; }

/// KB and one or more pawns vs K. It checks for draws with rook pawns and
/// a bishop of the wrong color. If such a draw is detected, SCALE_FACTOR_DRAW
/// is returned. If not, the return value is SCALE_FACTOR_NONE, i.e. no scaling
/// will be used.
template<>
ScaleFactor Endgame<KBPsK>::operator()(const Position& pos) const
{
	assert(pos.non_pawn_material(strongSide) == ElephantValueMg);
	assert(pos.count<SOLDIER>(strongSide) >= 1);

	// No assertions about the material of weakSide, because we want draws to
	// be detected even when the weaker side has some pawns.

	Bitboard pawns = pos.pieces(strongSide, SOLDIER);
	File pawnsFile = file_of(lsb(pawns));

	// If all the pawns are on the same B or G file, then it's potentially a draw
	if ((pawnsFile == FILE_B || pawnsFile == FILE_G)
		&& !(pos.pieces(SOLDIER) & ~file_bb(pawnsFile))
		&& pos.non_pawn_material(weakSide) == 0
		&& pos.count<SOLDIER>(weakSide) >= 1)
	{
		// Get weakSide pawn that is closest to the home rank
		Square weakPawnSq = backmost_sq(weakSide, pos.pieces(weakSide, SOLDIER));

		Square strongKingSq = pos.square<GENERAL>(strongSide);
		Square weakKingSq = pos.square<GENERAL>(weakSide);
		Square bishopSq = pos.square<ELEPHANT>(strongSide);

		// There's potential for a draw if our pawn is blocked on the 7th rank,
		// the bishop cannot attack it or they only have one pawn left
		if (relative_rank(strongSide, weakPawnSq) == RANK_7
			&& (pos.pieces(strongSide, SOLDIER) & (weakPawnSq + pawn_push(weakSide)))
			&& pos.count<SOLDIER>(strongSide) == 1)
		{
			int strongKingDist = distance(weakPawnSq, strongKingSq);
			int weakKingDist = distance(weakPawnSq, weakKingSq);

			// It's a draw if the weak king is on its back two ranks, within 2
			// squares of the blocking pawn and the strong king is not
			// closer. (I think this rule only fails in practically
			// unreachable positions such as 5k1K/6p1/6P1/8/8/3B4/8/8 w
			// and positions where qsearch will immediately correct the
			// problem such as 8/4k1p1/6P1/1K6/3B4/8/8/8 w)
			if (relative_rank(strongSide, weakKingSq) >= RANK_7
				&& weakKingDist <= 2
				&& weakKingDist <= strongKingDist)
				return SCALE_FACTOR_DRAW;
		}
	}

	return SCALE_FACTOR_NONE;
}

/// KQ vs KR and one or more pawns. It tests for fortress draws with a rook on
/// the third rank defended by a pawn.
template<>
ScaleFactor Endgame<KQKRPs>::operator()(const Position& pos) const 
{

	assert(verify_material(pos, strongSide, ChariotValueMg, 0));
	assert(pos.count<CHARIOT>(weakSide) == 1);
	assert(pos.count<SOLDIER>(weakSide) >= 1);

	Square kingSq = pos.square<GENERAL>(weakSide);
	Square rsq = pos.square<GENERAL>(weakSide);

	if (relative_rank(weakSide, kingSq) <= RANK_2
		&&  relative_rank(weakSide, pos.square<GENERAL>(strongSide)) >= RANK_4
		&&  relative_rank(weakSide, rsq) == RANK_3
		&& (pos.pieces(weakSide, SOLDIER)
			& pos.attacks_from<GENERAL>(kingSq)
			& pos.attacks_from<SOLDIER>(rsq, strongSide)))
		return SCALE_FACTOR_DRAW;

	return SCALE_FACTOR_NONE;
}

/// KRP vs KR. This function knows a handful of the most important classes of
/// drawn positions, but is far from perfect. It would probably be a good idea
/// to add more knowledge in the future.
///
/// It would also be nice to rewrite the actual code for this function,
/// which is mostly copied from Glaurung 1.x, and isn't very pretty.
template<>
ScaleFactor Endgame<KRPKR>::operator()(const Position& pos) const
{
	assert(verify_material(pos, strongSide, ChariotValueMg, 1));
	assert(verify_material(pos, weakSide, ChariotValueMg, 0));

	// Assume strongSide is white and the pawn is on files A-D
	Square wksq = normalize(pos, strongSide, pos.square<GENERAL>(strongSide));
	Square bksq = normalize(pos, strongSide, pos.square<GENERAL>(weakSide));
	Square wrsq = normalize(pos, strongSide, pos.square<CHARIOT>(strongSide));
	Square wpsq = normalize(pos, strongSide, pos.square<SOLDIER>(strongSide));
	Square brsq = normalize(pos, strongSide, pos.square<CHARIOT>(weakSide));

	File f = file_of(wpsq);
	Rank r = rank_of(wpsq);
	Square queeningSq = make_square(f, RANK_8);
	int tempo = (pos.side_to_move() == strongSide);

	// If the pawn is not too far advanced and the defending king defends the
	// queening square, use the third-rank defence.
	if (r <= RANK_5
		&& distance(bksq, queeningSq) <= 1
		&& wksq <= PT_H5
		&& (rank_of(brsq) == RANK_6 || (r <= RANK_3 && rank_of(wrsq) != RANK_6)))
		return SCALE_FACTOR_DRAW;

	// The defending side saves a draw by checking from behind in case the pawn
	// has advanced to the 6th rank with the king behind.
	if (r == RANK_6
		&& distance(bksq, queeningSq) <= 1
		&& rank_of(wksq) + tempo <= RANK_6
		&& (rank_of(brsq) == RANK_1 || (!tempo && distance<File>(brsq, wpsq) >= 3)))
		return SCALE_FACTOR_DRAW;

	if (r >= RANK_6
		&& bksq == queeningSq
		&& rank_of(brsq) == RANK_1
		&& (!tempo || distance(wksq, wpsq) >= 2))
		return SCALE_FACTOR_DRAW;

	// White pawn on a7 and rook on a8 is a draw if black's king is on g7 or h7
	// and the black rook is behind the pawn.
	if (wpsq == PT_A7
		&& wrsq == PT_A8
		&& (bksq == PT_H7 || bksq == PT_G7)
		&& file_of(brsq) == FILE_A
		&& (rank_of(brsq) <= RANK_3 || file_of(wksq) >= FILE_D || rank_of(wksq) <= RANK_5))
		return SCALE_FACTOR_DRAW;

	// If the defending king blocks the pawn and the attacking king is too far
	// away, it's a draw.
	if (r <= RANK_5
		&& bksq == wpsq + NORTH
		&& distance(wksq, wpsq) - tempo >= 2
		&& distance(wksq, brsq) - tempo >= 2)
		return SCALE_FACTOR_DRAW;

	// Pawn on the 7th rank supported by the rook from behind usually wins if the
	// attacking king is closer to the queening square than the defending king,
	// and the defending king cannot gain tempi by threatening the attacking rook.
	if (r == RANK_7
		&& f != FILE_A
		&& file_of(wrsq) == f
		&& wrsq != queeningSq
		&& (distance(wksq, queeningSq) < distance(bksq, queeningSq) - 2 + tempo)
		&& (distance(wksq, queeningSq) < distance(bksq, wrsq) + tempo))
		return ScaleFactor(SCALE_FACTOR_MAX - 2 * distance(wksq, queeningSq));

	// Similar to the above, but with the pawn further back
	if (f != FILE_A
		&& file_of(wrsq) == f
		&& wrsq < wpsq
		&& (distance(wksq, queeningSq) < distance(bksq, queeningSq) - 2 + tempo)
		&& (distance(wksq, wpsq + NORTH) < distance(bksq, wpsq + NORTH) - 2 + tempo)
		&& (distance(bksq, wrsq) + tempo >= 3
			|| (distance(wksq, queeningSq) < distance(bksq, wrsq) + tempo
				&& (distance(wksq, wpsq + NORTH) < distance(bksq, wrsq) + tempo))))
		return ScaleFactor(SCALE_FACTOR_MAX
			- 8 * distance(wpsq, queeningSq)
			- 2 * distance(wksq, queeningSq));

	// If the pawn is not far advanced and the defending king is somewhere in
	// the pawn's path, it's probably a draw.
	if (r <= RANK_4 && bksq > wpsq)
	{
		if (file_of(bksq) == file_of(wpsq))
			return ScaleFactor(10);
		if (distance<File>(bksq, wpsq) == 1
			&& distance(wksq, bksq) > 2)
			return ScaleFactor(24 - 2 * distance(wksq, bksq));
	}
	return SCALE_FACTOR_NONE;
}

template<>
ScaleFactor Endgame<KRPKB>::operator()(const Position& pos) const
{
	assert(verify_material(pos, strongSide, ChariotValueMg, 1));
	assert(verify_material(pos, weakSide, ElephantValueMg, 0));

	// Test for a rook pawn
	if (pos.pieces(SOLDIER) & (FileABB | FileHBB))
	{
		Square ksq = pos.square<GENERAL>(weakSide);
		Square bsq = pos.square<ELEPHANT>(weakSide);
		Square psq = pos.square<SOLDIER>(strongSide);
		Rank rk = relative_rank(strongSide, psq);
		Square push = pawn_push(strongSide);

		// If the pawn is on the 5th rank and the pawn (currently) is on
		// the same color square as the bishop then there is a chance of
		// a fortress. Depending on the king position give a moderate
		// reduction or a stronger one if the defending king is near the
		// corner but not trapped there.
		if (rk == RANK_5)
		{
			int d = distance(psq + 3 * push, ksq);

			if (d <= 2 && !(d == 0 && ksq == pos.square<GENERAL>(strongSide) + 2 * push))
				return ScaleFactor(24);
			else
				return ScaleFactor(48);
		}

		// When the pawn has moved to the 6th rank we can be fairly sure
		// it's drawn if the bishop attacks the square in front of the
		// pawn from a reasonable distance and the defending king is near
		// the corner
		if (rk == RANK_6
			&& distance(psq + 2 * push, ksq) <= 1
			&& (PseudoAttacks[ELEPHANT][bsq] & (psq + push))
			&& distance<File>(bsq, psq) >= 2)
			return ScaleFactor(8);
	}
	return SCALE_FACTOR_NONE;
}

/// KRPP vs KRP. There is just a single rule: if the stronger side has no passed
/// pawns and the defending king is actively placed, the position is drawish.
template<>
ScaleFactor Endgame<KRPPKRP>::operator()(const Position& pos) const
{
	assert(verify_material(pos, strongSide, ChariotValueMg, 2));
	assert(verify_material(pos, weakSide, ChariotValueMg, 1));

	Square wpsq1 = pos.squares<SOLDIER>(strongSide)[0];
	Square wpsq2 = pos.squares<SOLDIER>(strongSide)[1];
	Square bksq = pos.square<GENERAL>(weakSide);

	// Does the stronger side have a passed pawn?
	if (pos.pawn_passed(strongSide, wpsq1) || pos.pawn_passed(strongSide, wpsq2))
		return SCALE_FACTOR_NONE;

	Rank r = std::max(relative_rank(strongSide, wpsq1), relative_rank(strongSide, wpsq2));

	if (distance<File>(bksq, wpsq1) <= 1
		&& distance<File>(bksq, wpsq2) <= 1
		&& relative_rank(strongSide, bksq) > r)
	{
		assert(r > RANK_1 && r < RANK_7);
		return ScaleFactor(KRPPKRPScaleFactors[r]);
	}
	return SCALE_FACTOR_NONE;
}

/// K and two or more pawns vs K. There is just a single rule here: If all pawns
/// are on the same rook file and are blocked by the defending king, it's a draw.
template<>
ScaleFactor Endgame<KPsK>::operator()(const Position& pos) const 
{

	assert(pos.non_pawn_material(strongSide) == VALUE_ZERO);
	assert(pos.count<SOLDIER>(strongSide) >= 2);
	assert(verify_material(pos, weakSide, VALUE_ZERO, 0));

	Square ksq = pos.square<GENERAL>(weakSide);
	Bitboard pawns = pos.pieces(strongSide, GENERAL);

	// If all pawns are ahead of the king, on a single rook file and
	// the king is within one file of the pawns, it's a draw.
	if (!(pawns & ~in_front_bb(weakSide, rank_of(ksq)))
		&& !((pawns & ~FileABB) && (pawns & ~FileHBB))
		&& distance<File>(ksq, lsb(pawns)) <= 1)
		return SCALE_FACTOR_DRAW;

	return SCALE_FACTOR_NONE;
}

/// KBP vs KB. There are two rules: if the defending king is somewhere along the
/// path of the pawn, and the square of the king is not of the same color as the
/// stronger side's bishop, it's a draw. If the two bishops have opposite color,
/// it's almost always a draw.
template<>
ScaleFactor Endgame<KBPKB>::operator()(const Position& pos) const {

	assert(verify_material(pos, strongSide, ElephantValueMg, 1));
	assert(verify_material(pos, weakSide, ElephantValueMg, 0));

	Square pawnSq = pos.square<SOLDIER>(strongSide);
	Square strongBishopSq = pos.square<ELEPHANT>(strongSide);
	Square weakBishopSq = pos.square<ELEPHANT>(weakSide);
	Square weakKingSq = pos.square<GENERAL>(weakSide);

	// Case 1: Defending king blocks the pawn, and cannot be driven away
	if (file_of(weakKingSq) == file_of(pawnSq)
		&& relative_rank(strongSide, pawnSq) < relative_rank(strongSide, weakKingSq)
		&& (relative_rank(strongSide, weakKingSq) <= RANK_6))
		return SCALE_FACTOR_DRAW;
	
	return SCALE_FACTOR_NONE;
}

/// KBPP vs KB. It detects a few basic draws with opposite-colored bishops
template<>
ScaleFactor Endgame<KBPPKB>::operator()(const Position& pos) const {

	return SCALE_FACTOR_NONE;	
}

/// KBP vs KN. There is a single rule: If the defending king is somewhere along
/// the path of the pawn, and the square of the king is not of the same color as
/// the stronger side's bishop, it's a draw.
template<>
ScaleFactor Endgame<KBPKN>::operator()(const Position& pos) const {

	assert(verify_material(pos, strongSide, ElephantValueMg, 1));
	assert(verify_material(pos, weakSide, HorseValueMg, 0));

	Square pawnSq = pos.square<SOLDIER>(strongSide);
	Square strongBishopSq = pos.square<ELEPHANT>(strongSide);
	Square weakKingSq = pos.square<GENERAL>(weakSide);

	if (file_of(weakKingSq) == file_of(pawnSq)
		&& relative_rank(strongSide, pawnSq) < relative_rank(strongSide, weakKingSq)
		&& (relative_rank(strongSide, weakKingSq) <= RANK_6))
		return SCALE_FACTOR_DRAW;

	return SCALE_FACTOR_NONE;
}

/// KNP vs K. There is a single rule: if the pawn is a rook pawn on the 7th rank
/// and the defending king prevents the pawn from advancing, the position is drawn.
template<>
ScaleFactor Endgame<KNPK>::operator()(const Position& pos) const {

	assert(verify_material(pos, strongSide, HorseValueMg, 1));
	assert(verify_material(pos, weakSide, VALUE_ZERO, 0));

	// Assume strongSide is white and the pawn is on files A-D
	Square pawnSq = normalize(pos, strongSide, pos.square<SOLDIER>(strongSide));
	Square weakKingSq = normalize(pos, strongSide, pos.square<GENERAL>(weakSide));

	if (pawnSq == PT_A7 && distance(PT_A8, weakKingSq) <= 1)
		return SCALE_FACTOR_DRAW;

	return SCALE_FACTOR_NONE;
}

/// KNP vs KB. If knight can block bishop from taking pawn, it's a win.
/// Otherwise the position is drawn.
template<>
ScaleFactor Endgame<KNPKB>::operator()(const Position& pos) const {

	Square pawnSq = pos.square<SOLDIER>(strongSide);
	Square bishopSq = pos.square<ELEPHANT>(weakSide);
	Square weakKingSq = pos.square<GENERAL>(weakSide);

	// King needs to get close to promoting pawn to prevent knight from blocking.
	// Rules for this are very tricky, so just approximate.
	if (forward_bb(strongSide, pawnSq) & pos.attacks_from<ELEPHANT>(bishopSq))
		return ScaleFactor(distance(weakKingSq, pawnSq));

	return SCALE_FACTOR_NONE;
}

/// KP vs KP. This is done by removing the weakest side's pawn and probing the
/// KP vs K bitbase: If the weakest side has a draw without the pawn, it probably
/// has at least a draw with the pawn as well. The exception is when the stronger
/// side's pawn is far advanced and not on a rook file; in this case it is often
/// possible to win (e.g. 8/4k3/3p4/3P4/6K1/8/8/8 w - - 0 1).
template<>
ScaleFactor Endgame<KPKP>::operator()(const Position& pos) const {

	assert(verify_material(pos, strongSide, VALUE_ZERO, 1));
	assert(verify_material(pos, weakSide, VALUE_ZERO, 1));

	// Assume strongSide is white and the pawn is on files A-D
	Square wksq = normalize(pos, strongSide, pos.square<GENERAL>(strongSide));
	Square bksq = normalize(pos, strongSide, pos.square<GENERAL>(weakSide));
	Square psq = normalize(pos, strongSide, pos.square<SOLDIER>(strongSide));

	Color us = strongSide == pos.side_to_move() ? WHITE : BLACK;

	// If the pawn has advanced to the fifth rank or further, and is not a
	// rook pawn, it's too dangerous to assume that it's at least a draw.
	if (rank_of(psq) >= RANK_5 && file_of(psq) != FILE_A)
		return SCALE_FACTOR_NONE;

	// Probe the KPK bitbase with the weakest side's pawn removed. If it's a draw,
	// it's probably at least a draw even with the pawn.
	return Bitbases::probe(wksq, psq, bksq, us) ? SCALE_FACTOR_NONE : SCALE_FACTOR_DRAW;
}
