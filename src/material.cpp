#include "endgame.h"
#include "material.h"
#include "thread.h"

namespace
{
	// Polynomial material imbalance parameters

	const int QuadraticOurs[][PIECE_TYPE_NB] = 
	{	
		//            OUR PIECES
		// pair soldier horse elephant canon chariot advisor
		{1667												}, // pair
		{  40,		2										}, // Soldier
		{  32,	  255,	  -3								}, // Horse			OUR PIECES
		{   0,  104,   4,    0								}, // Elephant
		{ -26,   -2,  47,   105,  -149						}, // Canon
		{ -185,   24, 122,   137,  -134,   0				}, // Chariot
		{													}, // Advisor		
	};

	const int QuadraticTheirs[][PIECE_TYPE_NB] =
	{
		//           THEIR PIECES
		// pair soldier horse elephant canon chariot advisor
		{0													}, // pair
		{ 36,    0											}, // Soldier
		{ 9,   63,   0										}, // Horse      OUR PIECES
		{ 59,   65,  42,     0								}, // Elephant
		{ 46,   39,  24,   -24,    0						}, // Canon
		{ 101,  100, -37,   141,  268,    0					},  // Chariot
		{													}, // Advisor	
	};

	// Endgame evaluation and scaling functions are accessed directly and not through
	// the function maps because they correspond to more than one material hash key.
	Endgame<KXK>    EvaluateKXK[] = { Endgame<KXK>(WHITE),    Endgame<KXK>(BLACK) };

	Endgame<KBPsK>  ScaleKBPsK[] = { Endgame<KBPsK>(WHITE),  Endgame<KBPsK>(BLACK) };
	Endgame<KQKRPs> ScaleKQKRPs[] = { Endgame<KQKRPs>(WHITE), Endgame<KQKRPs>(BLACK) };
	Endgame<KPsK>   ScaleKPsK[] = { Endgame<KPsK>(WHITE),   Endgame<KPsK>(BLACK) };
	Endgame<KPKP>   ScaleKPKP[] = { Endgame<KPKP>(WHITE),   Endgame<KPKP>(BLACK) };

	// Helper used to detect a given material distribution
	bool is_KXK(const Position& pos, Color us) 
	{
		return  !more_than_one(pos.pieces(~us))
			&& pos.non_pawn_material(us) >= ChariotValueMg;
	}

	bool is_KBPsKs(const Position& pos, Color us) 
	{
		return   pos.non_pawn_material(us) == ElephantValueMg
			&& pos.count<ELEPHANT>(us) == 1
			&& pos.count<SOLDIER  >(us) >= 1;
	}

	bool is_KQKRPs(const Position& pos, Color us) 
	{
		return  !pos.count<SOLDIER>(us)
			&& pos.non_pawn_material(us) == ChariotValueMg
			&& pos.count<CHARIOT>(us) == 1
			&& pos.count<CANON>(~us) == 1
			&& pos.count<SOLDIER>(~us) >= 1;
	}

	/// imbalance() calculates the imbalance by comparing the piece count of each
	/// piece type for both colors.
	template<Color Us>
	int imbalance(const int pieceCount[][PIECE_TYPE_NB]) {

		const Color Them = (Us == WHITE ? BLACK : WHITE);

		int bonus = 0;

		// Second-degree polynomial material imbalance by Tord Romstad
		for (int pt1 = NO_PIECE_TYPE; pt1 <= CHARIOT; ++pt1)
		{
			if (!pieceCount[Us][pt1])
				continue;

			int v = 0;

			for (int pt2 = NO_PIECE_TYPE; pt2 <= pt1; ++pt2)
				v += QuadraticOurs[pt1][pt2] * pieceCount[Us][pt2]
				+ QuadraticTheirs[pt1][pt2] * pieceCount[Them][pt2];

			bonus += pieceCount[Us][pt1] * v;
		}

		return bonus;
	}
} // namespace

namespace Material
{

/// Material::probe() looks up the current position's material configuration in
/// the material hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same material configuration occurs again.

Entry* probe(const Position& pos)
{
	Key key = pos.material_key();
	Entry* e = pos.this_thread()->materialTable[key];

	if (e->key == key)
		return e;

	std::memset(e, 0, sizeof(Entry));
	e->key = key;
	e->factor[WHITE] = e->factor[BLACK] = (uint8_t)SCALE_FACTOR_NORMAL;
	e->gamePhase = pos.game_phase();

	// Let's look if we have a specialized evaluation function for this particular
	// material configuration. Firstly we look for a fixed configuration one, then
	// for a generic one if the previous search failed.
	if ((e->evaluationFunction = pos.this_thread()->endgames.probe<Value>(key)) != nullptr)
		return e;

	for (Color c = WHITE; c <= BLACK; ++c)
		if (is_KXK(pos, c))
		{
			e->evaluationFunction = &EvaluateKXK[c];
			return e;
		}

	// OK, we didn't find any special evaluation function for the current material
	// configuration. Is there a suitable specialized scaling function?
	EndgameBase<ScaleFactor>* sf;

	if ((sf = pos.this_thread()->endgames.probe<ScaleFactor>(key)) != nullptr)
	{
		e->scalingFunction[sf->strong_side()] = sf; // Only strong color assigned
		return e;
	}

	// We didn't find any specialized scaling function, so fall back on generic
	// ones that refer to more than one material distribution. Note that in this
	// case we don't return after setting the function.
	for (Color c = WHITE; c <= BLACK; ++c)
	{
		if (is_KBPsKs(pos, c))
			e->scalingFunction[c] = &ScaleKBPsK[c];

		else if (is_KQKRPs(pos, c))
			e->scalingFunction[c] = &ScaleKQKRPs[c];
	}

	Value npm_w = pos.non_pawn_material(WHITE);
	Value npm_b = pos.non_pawn_material(BLACK);

	if (npm_w + npm_b == VALUE_ZERO && pos.pieces(SOLDIER)) // Only pawns on the board
	{
		if (!pos.count<SOLDIER>(BLACK))
		{			
			e->scalingFunction[WHITE] = &ScaleKPsK[WHITE];
		}
		else if (!pos.count<SOLDIER>(WHITE))
		{			
			e->scalingFunction[BLACK] = &ScaleKPsK[BLACK];
		}
		else if (pos.count<SOLDIER>(WHITE) == 1 && pos.count<SOLDIER>(BLACK) == 1)
		{
			// This is a special case because we set scaling functions
			// for both colors instead of only one.
			e->scalingFunction[WHITE] = &ScaleKPKP[WHITE];
			e->scalingFunction[BLACK] = &ScaleKPKP[BLACK];
		}
	}

	// Zero or just one pawn makes it difficult to win, even with a small material
	// advantage. This catches some trivial draws like KK, KBK and KNK and gives a
	// drawish scale factor for cases such as KRKBP and KmmKm (except for KBBKN).
	if (!pos.count<SOLDIER>(WHITE) && npm_w - npm_b <= ElephantValueMg)
		e->factor[WHITE] = uint8_t(npm_w <  ChariotValueMg ? SCALE_FACTOR_DRAW :
			npm_b <= ElephantValueMg ? 4 : 14);

	if (!pos.count<SOLDIER>(BLACK) && npm_b - npm_w <= ElephantValueMg)
		e->factor[BLACK] = uint8_t(npm_b <  ChariotValueMg ? SCALE_FACTOR_DRAW :
			npm_w <= ElephantValueMg ? 4 : 14);

	if (pos.count<SOLDIER>(WHITE) == 1 && npm_w - npm_b <= ElephantValueMg)
		e->factor[WHITE] = (uint8_t)SCALE_FACTOR_ONEPAWN;

	if (pos.count<SOLDIER>(BLACK) == 1 && npm_b - npm_w <= ElephantValueMg)
		e->factor[BLACK] = (uint8_t)SCALE_FACTOR_ONEPAWN;

	// Evaluate the material imbalance. We use PIECE_TYPE_NONE as a place holder
	// for the bishop pair "extended piece", which allows us to be more flexible
	// in defining bishop pair bonuses.
	const int PieceCount[COLOR_NB][PIECE_TYPE_NB] = {
		{ pos.count<ELEPHANT>(WHITE) > 1, pos.count<SOLDIER>(WHITE), pos.count<HORSE>(WHITE),
		pos.count<ELEPHANT>(WHITE)    , pos.count<CANON>(WHITE), pos.count<CHARIOT >(WHITE) },
		{ pos.count<ELEPHANT>(BLACK) > 1, pos.count<SOLDIER>(BLACK), pos.count<HORSE>(BLACK),
		pos.count<ELEPHANT>(BLACK)    , pos.count<CANON>(BLACK), pos.count<CHARIOT >(BLACK) } };

	e->value = int16_t((imbalance<WHITE>(PieceCount) - imbalance<BLACK>(PieceCount)) / 16);
	return e;
}
}