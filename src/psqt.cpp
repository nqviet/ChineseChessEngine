#include <algorithm>

#include "types.h"

Value PieceValue[PHASE_NB][PIECE_NB] = {
	{ VALUE_ZERO, SoldierValueMg, HorseValueMg, ElephantValueMg, CannonValueMg, ChariotValueMg },
	{ VALUE_ZERO, SoldierValueEg, HorseValueEg, ElephantValueEg, CannonValueEg, ChariotValueEg }
};

namespace PSQT
{
#define S(mg, eg) make_score(mg, eg)

// Bonus[PieceType][Square / 2] contains Piece-Square scores. For each piece
// type on a given square a (middlegame, endgame) score pair is assigned. Table
// is defined for files A..D and white side: it is symmetric for black side and
// second half of the files.
const Score Bonus[][RANK_NB][int(FILE_NB) / 2 + 1] = 
{
	{ },
	{ // Soldier
		{ S(0, 0),	 S(0, 0),	S(0, 0),	S(0, 0),   S(0, 0)},
		{ S(-16, 7), S(1,-4),	S(7, 8),	S(3,-2),   S(0, 0) },
		{ S(-23,-4), S(-7,-5),	S(19, 5),	S(24, 4),  S(0, 0) },
		{ S(-22, 3), S(-14, 3), S(20,-8),	S(35,-3),  S(0, 0) },
		{ S(-11, 8), S(0, 9),	S(3, 7),	S(21,-6),  S(0, 0) },
		{ S(-11, 8), S(-13,-5), S(-6, 2),	S(-2, 4),  S(0, 0) },
		{ S(-9, 3),  S(15,-9),	S(-8, 1),	S(-4,18),  S(0, 0) },

		{ S(-9, 3),  S(15,-9),	S(-8, 1),	S(-4,18),  S(0, 0) },
		{ S(-9, 3),  S(15,-9),	S(-8, 1),	S(-4,18),  S(0, 0) },
		{ S(-9, 3),  S(15,-9),	S(-8, 1),	S(-4,18),  S(0, 0) }
	},
	{ // Horse
		{ S(-143, -97), S(-96,-82), S(-80,-46), S(-73,-14),  S(0, 0) },
		{ S(-83, -69),	S(-43,-55), S(-21,-17), S(-10,  9),  S(0, 0) },
		{ S(-71, -50),	S(-22,-39), S(0, -8),	S(9, 28),    S(0, 0) },
		{ S(-25, -41),	S(18,-25),	S(43,  7),	S(47, 38),   S(0, 0) },
		{ S(-26, -46),	S(16,-25),	S(38,  2),	S(50, 41),   S(0, 0) },
		{ S(-11, -55),	S(37,-38),	S(56, -8),	S(71, 27),   S(0, 0) },
		{ S(-62, -64),	S(-17,-50), S(5,-24),	S(14, 13),   S(0, 0) },
		{ S(-195,-110), S(-66,-90), S(-42,-50), S(-29,-13),  S(0, 0) },

		{ S(-195,-110), S(-66,-90), S(-42,-50), S(-29,-13),  S(0, 0) },
		{ S(-195,-110), S(-66,-90), S(-42,-50), S(-29,-13),  S(0, 0) }
	},
	{ // Elephant
	},
	{ // Cannon
		{ S(-54,-68), S(-23,-40), S(-35,-46), S(-44,-28),	S(0, 0) },
		{ S(-30,-43), S(10,-17),  S(2,-23),   S(-9, -5),	S(0, 0) },
		{ S(-19,-32), S(17, -9),  S(11,-13),  S(1,  8),		S(0, 0) },
		{ S(-21,-36), S(18,-13),  S(11,-15),  S(0,  7),		S(0, 0) },
		{ S(-21,-36), S(14,-14),  S(6,-17),   S(-1,  3),	S(0, 0) },
		{ S(-27,-35), S(6,-13),   S(2,-10),   S(-8,  1),	S(0, 0) },
		{ S(-33,-44), S(7,-21),   S(-4,-22),  S(-12, -4),	S(0, 0) },
		{ S(-45,-65), S(-21,-42), S(-29,-46), S(-39,-27),	S(0, 0) },

		{ S(-45,-65), S(-21,-42), S(-29,-46), S(-39,-27),	S(0, 0) },
		{ S(-45,-65), S(-21,-42), S(-29,-46), S(-39,-27),	S(0, 0) }
	},
	{ // Chariot
		{ S(-25, 0), S(-16, 0), S(-16, 0),	S(-9, 0),	S(0, 0) },
		{ S(-21, 0), S(-8, 0),  S(-3, 0),	S(0, 0),	S(0, 0) },
		{ S(-21, 0), S(-9, 0),  S(-4, 0),	S(2, 0),	S(0, 0) },
		{ S(-22, 0), S(-6, 0),  S(-1, 0),	S(2, 0),	S(0, 0) },
		{ S(-22, 0), S(-7, 0),  S(0, 0),	S(1, 0),	S(0, 0) },
		{ S(-21, 0), S(-7, 0),  S(0, 0),	S(2, 0),	S(0, 0) },
		{ S(-12, 0), S(4, 0),   S(8, 0),	S(12, 0),	S(0, 0) },
		{ S(-23, 0), S(-15, 0), S(-11, 0),	S(-5, 0),	S(0, 0) },

		{ S(-23, 0), S(-15, 0), S(-11, 0),	S(-5, 0),	S(0, 0) },
		{ S(-23, 0), S(-15, 0), S(-11, 0),	S(-5, 0),	S(0, 0) }
	},
	{ // Advisor
	},
	{ // General
	}
};

#undef S

Score psq[PIECE_NB][SQUARE_NB];

// init() initializes piece-square tables: the white halves of the tables are
// copied from Bonus[] adding the piece value, then the black halves of the
// tables are initialized by flipping and changing the sign of the white scores.
void init() {

	for (Piece pc = W_SOLDIER; pc <= W_GENERAL; ++pc)
	{
		PieceValue[MG][~pc] = PieceValue[MG][pc];
		PieceValue[EG][~pc] = PieceValue[EG][pc];

		Score v = make_score(PieceValue[MG][pc], PieceValue[EG][pc]);

		for (Square s = PT_A1; s <= PT_I10; ++s)
		{
			File f = std::min(file_of(s), FILE_I - file_of(s));
			psq[pc][s] = v + Bonus[pc][rank_of(s)][f];
			psq[~pc][~s] = -psq[pc][s];
		}
	}
}

} // namespace PSQT