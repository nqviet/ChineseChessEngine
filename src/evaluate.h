#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include <string>

#include "types.h"

class Position;

namespace Eval 
{

	const Value Tempo = Value(20); // Must be visible to search

	std::string trace(const Position& pos);

	template<bool DoTrace = false>
	Value evaluate(const Position& pos);
}


#endif