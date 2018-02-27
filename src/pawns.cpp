#include "pawns.h"
#include "thread.h"

namespace
{
	#define V Value
	#define S(mg, eg) make_score(mg, eg)


}

namespace Pawns
{

/// Pawns::init() initializes some tables needed by evaluation. Instead of using
/// hard-coded tables, when makes sense, we prefer to calculate them with a formula
/// to reduce independent parameters and to allow easier tuning and better insight.

void init()
{

}

/// Pawns::probe() looks up the current position's pawns configuration in
/// the pawns hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same pawns configuration occurs again.

Entry* probe(const Position& pos) {

	Key key = pos.pawn_key();
	Entry* e = pos.this_thread()->pawnsTable[key];

	if (e->key == key)
		return e;

	e->key = key;
	e->score = make_score(0, 0);//evaluate<WHITE>(pos, e) - evaluate<BLACK>(pos, e);
	e->asymmetry = 0; // popcount(e->semiopenFiles[WHITE] ^ e->semiopenFiles[BLACK]);
	e->openFiles = 0; // popcount(e->semiopenFiles[WHITE] & e->semiopenFiles[BLACK]);
	return e;
}

} // namespace Pawns
