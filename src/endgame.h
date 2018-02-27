#ifndef ENDGAME_H_INCLUDED
#define ENDGAME_H_INCLUDED

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "position.h"
#include "types.h"

/// EndgameType lists all supported endgames

enum EndgameType {

	// Evaluation functions

	KNNK,  // KNN vs K
	KXK,   // Generic "mate lone king" eval
	KBNK,  // KBN vs K
	KPK,   // KP vs K
	KRKP,  // KR vs KP
	KRKB,  // KR vs KB
	KRKN,  // KR vs KN
	KQKP,  // KQ vs KP
	KQKR,  // KQ vs KR


	// Scaling functions
	SCALING_FUNCTIONS,

	KBPsK,   // KB and pawns vs K
	KQKRPs,  // KQ vs KR and pawns
	KRPKR,   // KRP vs KR
	KRPKB,   // KRP vs KB
	KRPPKRP, // KRPP vs KRP
	KPsK,    // K and pawns vs K
	KBPKB,   // KBP vs KB
	KBPPKB,  // KBPP vs KB
	KBPKN,   // KBP vs KN
	KNPK,    // KNP vs K
	KNPKB,   // KNP vs KB
	KPKP     // KP vs KP
};


/// Endgame functions can be of two types depending on whether they return a
/// Value or a ScaleFactor.
template<EndgameType E> using
eg_type = typename std::conditional<(E < SCALING_FUNCTIONS), Value, ScaleFactor>::type;

/// Base and derived templates for endgame evaluation and scaling functions
template<typename T>
struct EndgameBase 
{

	virtual ~EndgameBase() = default;
	virtual Color strong_side() const = 0;
	virtual T operator()(const Position&) const = 0;
};

template<EndgameType E, typename T = eg_type<E>>
struct Endgame : public EndgameBase<T> 
{

	explicit Endgame(Color c) : strongSide(c), weakSide(~c) {}
	Color strong_side() const { return strongSide; }
	T operator()(const Position&) const;

private:
	Color strongSide, weakSide;
};

/// The Endgames class stores the pointers to endgame evaluation and scaling
/// base objects in two std::map. We use polymorphism to invoke the actual
/// endgame function by calling its virtual operator().
class Endgames 
{

	template<typename T> using Map = std::map<Key, std::unique_ptr<EndgameBase<T>>>;

	template<EndgameType E, typename T = eg_type<E>>
	void add(const std::string& code);

	template<typename T>
	Map<T>& map() {
		return std::get<std::is_same<T, ScaleFactor>::value>(maps);
	}

	std::pair<Map<Value>, Map<ScaleFactor>> maps;

public:
	Endgames();

	template<typename T>
	EndgameBase<T>* probe(Key key) {
		return map<T>().count(key) ? map<T>()[key].get() : nullptr;
	}
};

#endif
