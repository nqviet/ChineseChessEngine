#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <cassert>

#include "bitboard.h"

int SquareDistance[SQUARE_NB][SQUARE_NB];

Bitboard  ChariotMasks[SQUARE_NB];
Bitboard* ChariotAttacks[SQUARE_NB];

Bitboard  CanonMasks[SQUARE_NB];
Bitboard* CanonAttacks[SQUARE_NB];

Bitboard  HorseMasks[SQUARE_NB];
Bitboard* HorseAttacks[SQUARE_NB];

Bitboard  ElephantMasks[SQUARE_NB];
Bitboard* ElephantAttacks[SQUARE_NB];

Bitboard SquareBB[SQUARE_NB];
Bitboard FileBB[FILE_NB];
Bitboard RankBB[RANK_NB];
Bitboard AdjacentFilesBB[FILE_NB];
Bitboard InFrontBB[COLOR_NB][RANK_NB];
Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard DistanceRingBB[SQUARE_NB][10];
Bitboard ForwardBB[COLOR_NB][SQUARE_NB];
Bitboard PassedPawnMask[COLOR_NB][SQUARE_NB];
Bitboard PawnAttackSpan[COLOR_NB][SQUARE_NB];
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];

namespace
{
	Bitboard ChariotTable[0x108000];	// To store chariot attacks
	Bitboard CanonTable[0x108000/*0xB40000*/];		// To store canon attacks
	Bitboard HorseTable[0x10F00];		// To store horse attacks
	Bitboard ElephantTable[0xB24];		// To store elephant attacks

	template <PieceType Pt>
	void init_magics(Bitboard table[], Bitboard* attacks[],	Bitboard masks[], Square deltas[], int deltasSize);
}

const std::string Bitboards::pretty(Bitboard b)
{
	std::string s = "";

	for (Rank r = RANK_10; r >= RANK_1; --r)
	{
		for (File f = FILE_A; f <= FILE_I; ++f)
			s += b & make_square(f, r)
					?	f != FILE_I ? "X---" : "X"
						:	f != FILE_I ? "----" : "-";
		if (r == RANK_6)
			s += "\n|||||||||||||||||||||||||||||||||\n";
		else if (r == RANK_10 || r == RANK_3)
			s += "\n|   |   |   | \\ | / |   |   |   |\n";
		else if (r == RANK_9 || r == RANK_2)
			s += "\n|   |   |   | / | \\ |   |   |   |\n";
		else if (r != RANK_1)
			s += "\n|   |   |   |   |   |   |   |   |\n";

	}

	return s;
}

/// Bitboards::init() initializes various bitboard tables. It is called at
/// startup and relies on global objects to be already zero-initialized.

void Bitboards::init()
{
	for (Square s = PT_A1; s <= PT_I10; ++s)
		SquareBB[s] = Bitboard(1ULL) << s;

	for (File f = FILE_A; f <= FILE_I; ++f)
		FileBB[f] = f > FILE_A ? FileBB[f - 1] << 1 : FileABB;

	for (Rank r = RANK_1; r <= RANK_10; ++r)
		RankBB[r] = r > RANK_1 ? RankBB[r - 1] << 9 : Rank1BB;

	for (File f = FILE_A; f <= FILE_I; ++f)
		AdjacentFilesBB[f] = (f > FILE_A ? FileBB[f - 1] : 0) | (f < FILE_I ? FileBB[f + 1] : 0);

	for (Rank r = RANK_1; r < RANK_10; ++r)
		InFrontBB[WHITE][r] = ~(InFrontBB[BLACK][r + 1] = InFrontBB[BLACK][r] | RankBB[r]);

	for (Color c = WHITE; c <= BLACK; ++c)
		for (Square s = PT_A1; s <= PT_I10; ++s)
		{
			ForwardBB[c][s] = InFrontBB[c][rank_of(s)] & FileBB[file_of(s)];
			PawnAttackSpan[c][s] = (relative_rank(c, s) > RANK_5 ? RankBB[rank_of(s)] : 0) | ForwardBB[c][s];
			PassedPawnMask[c][s] = PawnAttackSpan[c][s];
		}

	for (Square s1 = PT_A1; s1 <= PT_I10; ++s1)
		for (Square s2 = PT_A1; s2 <= PT_I10; ++s2)
			if (s1 != s2)
			{
				SquareDistance[s1][s2] = std::max(distance<File>(s1, s2), distance<Rank>(s1, s2));
				DistanceRingBB[s1][SquareDistance[s1][s2] - 1] |= s2;
			}

	int steps[][9] = { {}, { -1, 9, 1 }, {}, {}, {}, {}, { 10, 8, -8, -10}, {9, 1, -1, -9 } };

	for (Color c = WHITE; c <= BLACK; ++c)
		for (PieceType pt = SOLDIER; pt <= GENERAL; ++pt)
			for (Square s = PT_A1; s <= PT_I10; ++s)
				for (int i = 0; steps[pt][i]; ++i)
				{
					Square to = s + Square(c == WHITE ? steps[pt][i] : -steps[pt][i]);

					if (is_ok(to) && distance(s, to) < 3)
					{
						if (pt == SOLDIER && relative_rank(c, s) <= RANK_5 && file_of(s) != file_of(to))
							continue;
						
						if ((pt == ADVISOR || pt == GENERAL) &&
							(file_of(to) == FILE_C || file_of(to) == FILE_G || relative_rank(c, to) > RANK_3))
							continue;

						if (pt == ADVISOR && (distance<File>(to, s) == 0 || distance<Rank>(to, s) == 0))
							continue;

						StepAttacksBB[make_piece(c, pt)][s] |= to;
					}
				}

	Square ChariotDeltas[]	= { NORTH,  EAST,  SOUTH,  WEST };
	Square CanonDeltas[]	= { NORTH,  EAST,  SOUTH,  WEST };
	Square HorseDeltas[]	= {	NORTH + NORTH + EAST, NORTH + NORTH + WEST,
								SOUTH + SOUTH + EAST, SOUTH + SOUTH + WEST,
								EAST + EAST + NORTH, EAST + EAST + SOUTH,
								WEST + WEST + NORTH, WEST + WEST + SOUTH,
								NORTH,  EAST,  SOUTH,  WEST };
	Square ElephantDeltas[] = {  NORTH_EAST + NORTH_EAST, NORTH_WEST + NORTH_WEST,
								SOUTH_EAST + SOUTH_EAST, SOUTH_WEST + SOUTH_WEST,
								NORTH_EAST,  NORTH_WEST, SOUTH_WEST,  SOUTH_WEST };
#if _DEBUG
	std::chrono::time_point<std::chrono::system_clock> startt, endt;
	std::chrono::duration<double> elapsed;

#	define STARTT	\
	startt = std::chrono::system_clock::now();
#	define ENDT	\
	endt = std::chrono::system_clock::now(); \
	elapsed = endt - startt; \
	std::cout << "initialize the attack table: " << elapsed.count() << "(s)" << std::endl;
#else
#	define STARTT
#	define ENDT
#endif

	STARTT
	init_magics<CHARIOT>(ChariotTable, ChariotAttacks, ChariotMasks, ChariotDeltas, 4);
	ENDT

	STARTT
	init_magics<CANON>(CanonTable, CanonAttacks, CanonMasks, CanonDeltas, 4);
	ENDT

	STARTT
	init_magics<HORSE>(HorseTable, HorseAttacks, HorseMasks, HorseDeltas, 12);
	ENDT

	STARTT
	init_magics<ELEPHANT>(ElephantTable, ElephantAttacks, ElephantMasks, ElephantDeltas, 8);
	ENDT

	Bitboard surroundingBB;
	for (Square s1 = PT_A1; s1 <= PT_I10; ++s1)
	{
		PseudoAttacks[CHARIOT][s1]	= attacks_bb<CHARIOT>(s1, 0);
		PseudoAttacks[HORSE][s1]	= attacks_bb<HORSE>(s1, 0);
		PseudoAttacks[ELEPHANT][s1] = attacks_bb<ELEPHANT>(s1, 0);

		surroundingBB.reset();
		surroundingBB |= shift<NORTH>(SquareBB[s1]);
		surroundingBB |= shift<EAST >(SquareBB[s1]);
		surroundingBB |= shift<SOUTH>(SquareBB[s1]);
		surroundingBB |= shift<WEST >(SquareBB[s1]);

		PseudoAttacks[CANON][s1] = PseudoAttacks[CHARIOT][s1] & ~surroundingBB;

		for (Square s2 = PT_A1; s2 <= PT_I10; ++s2)
		{
			if (!(PseudoAttacks[CHARIOT][s1] & s2))
				continue;

			LineBB[s1][s2] = (attacks_bb(W_CHARIOT, s1, 0) & attacks_bb(W_CHARIOT, s2, 0)) | s1 | s2;
			BetweenBB[s1][s2] = attacks_bb(W_CHARIOT, s1, SquareBB[s2]) & attacks_bb(W_CHARIOT, s2, SquareBB[s1]);
		}
	}
}

namespace
{
	Bitboard sliding_attack(Square deltas[], int deltasSize, Square sq, Bitboard occupied, PieceType pt = NO_PIECE_TYPE)
	{
		Bitboard attack = 0;

		for (int i = 0; i < deltasSize; ++i)
			for (Square s = sq + deltas[i];
				is_ok(s) && distance(s, s - deltas[i]) == 1;
				s += deltas[i])
			{
				if (pt == CANON)
				{
					if (occupied)
					{
						Bitboard bb;
						for (Square t = sq + deltas[i]; t != s; t += deltas[i])
						{
							bb |= t;
						}
						bb |= s;

						int pcnt = popcount(bb & occupied);
						if (pcnt == 1)
						{
							if (!(occupied & s))
								attack |= s;
						}
						else if (pcnt > 1)
						{
							attack |= s;
							break;
						}
					}
				}
				else
				{
					attack |= s;

					if (occupied & s)
						break;
				}
			}

		return attack;
	}

	Bitboard step_attack(Square deltas[], int deltasSize, Square sq, Bitboard occupied, PieceType pt = NO_PIECE_TYPE)
	{
		Bitboard attack = 0;

		for (int i = 0; i < deltasSize; ++i)
		{
			Square s = sq + deltas[i];
			Square dir = deltas[i];

			Square dir2 = DIR_NONE;
			if (dir == NORTH + NORTH + EAST || dir == NORTH + NORTH + WEST) dir2 = NORTH;
			else if (dir == SOUTH + SOUTH + EAST || dir == SOUTH + SOUTH + WEST) dir2 = SOUTH;
			else if (dir == EAST + EAST + NORTH || dir == EAST + EAST + SOUTH) dir2 = EAST;
			else if (dir == WEST + WEST + NORTH || dir == WEST + WEST + SOUTH) dir2 = WEST;

			if ((file_of(sq) <= FILE_B && dir2 == WEST) || (file_of(sq) >= FILE_I && dir2 == EAST))
				continue;

			if (is_ok(s))
			{
				if (pt == HORSE)
				{
					if (!(dir == NORTH || dir == EAST || dir == SOUTH || dir == WEST))
					{
						if (occupied)
						{
							if ((occupied & (sq + dir2)))
								continue;

							attack |= s;
						}
						else
							attack |= s;
					}
				}
				else if (pt == ELEPHANT)
				{
					Square dir = deltas[i];
					if (!(dir == NORTH_EAST || dir == NORTH_WEST || dir == SOUTH_EAST || dir == SOUTH_WEST))
					{
						if (s == PT_C1 || s == PT_G1 ||
							s == PT_A3 || s == PT_E3 || s == PT_I3 ||
							s == PT_C5 || s == PT_G5 ||
							s == PT_C6 || s == PT_G6 ||
							s == PT_A8 || s == PT_E8 || s == PT_I8 ||
							s == PT_C10 || s == PT_G10)
						{
							if (occupied)
							{
								Square dir2 = deltas[i] / 2;
								if (occupied & (sq + dir2))
									continue;
								if (occupied & (sq + dir))
									continue;

								attack |= s;
							}
							else
								attack |= s;
						}
					}
				}
				else
					attack |= s;
			}
		}

		return attack;
	}

	/// init_magics() computes all chariot and canon attacks at startup.

	template <PieceType Pt>
	void init_magics(Bitboard table[], Bitboard* attacks[], Bitboard masks[], Square deltas[], int deltasSize)
	{
		Bitboard edges, b, ring;
		int size;
		auto attack = (Pt == CANON || Pt == CHARIOT) ? sliding_attack : step_attack;

		// attacks[s] is a pointer to the beginning of the attacks table for square 's'
		attacks[PT_A1] = table;

		for (Square s = PT_A1; s <= PT_I10; ++s)
		{
			// Board edges are not considered in the relevant occupancies
			edges = ((Rank1BB | Rank10BB) & ~rank_bb(s)) | ((FileABB | FileIBB) & ~file_bb(s));

			// Given a square 's', the mask is the bitboard of sliding attacks from
			// 's' computed on an empty board. The index must be big enough to contain
			// all the attacks for each possible subset of the mask and so is 2 power
			// the number of 1s of the mask. Hence we deduce the size of the shift to
			// apply to the 64 or 32 bits word to get the index.
			if (Pt == CANON)
				masks[s] = attack(deltas, deltasSize, s, 0, NO_PIECE_TYPE) & ~edges;
			else
				masks[s] = attack(deltas, deltasSize, s, 0, NO_PIECE_TYPE) & ~edges;

			// Use Carry-Rippler trick to enumerate all subsets of masks[s] and
			// store the corresponding sliding attack bitboard in reference[].
			b = size = 0;

			do
			{
				attacks[s][pext_si128(b.v, masks[s].v)] = attack(deltas, deltasSize, s, b, Pt);
				size++;
				b = (b - masks[s]) & masks[s];
			} while (b);

			// Set the offset for the table of the next point.
			if (s < PT_I10)
				attacks[s + 1] = attacks[s] + size;      
		}    
	}
}
