#ifndef PAWNS_H_INCLUDED
#define PAWNS_H_INCLUDED

#include "misc.h"
#include "position.h"
#include "types.h"

namespace Pawns
{

/// Pawns::Entry contains various information about a pawn structure. A lookup
/// to the pawn hash table (performed by calling the probe function) returns a
/// pointer to an Entry object.

struct Entry
{
	Score pawns_score() const { return score; }
	Bitboard pawn_attacks(Color c) const { return pawnAttacks[c]; }
	Bitboard passed_pawns(Color c) const { return passedPawns[c]; }
	Bitboard pawn_attacks_span(Color c) const { return pawnAttacksSpan[c]; }
	int pawn_asymmetry() const { return asymmetry; }
	int open_files() const { return openFiles; }

	int semiopen_file(Color c, File f) const
	{
		return semiopenFiles[c] & (1 << f);
	}

	int semiopen_side(Color c, File f, bool leftSide) const
	{
		return semiopenFiles[c] & (leftSide ? (1 << f) - 1 : ~((1 << (f + 1)) - 1));
	}

	int pawns_on_same_color_squares(Color c, Square s) const
	{
		return true;
	}

	template<Color Us>
	Score king_safety(const Position& pos, Square ksq)
	{
		return  kingSquares[Us] == ksq ? kingSafety[Us] : (kingSafety[Us] = do_king_safety<Us>(pos, ksq));
	}

	template<Color Us>
	Score do_king_safety(const Position& pos, Square ksq) { return SCORE_ZERO; }

	template<Color Us>
	Value shelter_storm(const Position& pos, Square ksq) { return VALUE_ZERO; }

	Key key;
	Score score;
	Bitboard passedPawns[COLOR_NB];
	Bitboard pawnAttacks[COLOR_NB];
	Bitboard pawnAttacksSpan[COLOR_NB];
	Square kingSquares[COLOR_NB];
	Score kingSafety[COLOR_NB];
	int castlingRights[COLOR_NB];
	int semiopenFiles[COLOR_NB];
	int pawnsOnSquares[COLOR_NB][COLOR_NB]; // [color][light/dark squares]
	int asymmetry;
	int openFiles;
};

typedef HashTable<Entry, 16384> Table;

void init();
Entry* probe(const Position& pos);

}
#endif
