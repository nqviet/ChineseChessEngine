#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

#include <chrono>
#include <vector>

#include "types.h"

const std::string engine_info(bool to_uci = false);
void prefetch(void* addr);
void start_logger(const std::string& fname);

typedef std::chrono::milliseconds::rep TimePoint; // A value in milliseconds

inline TimePoint now() 
{
	return std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::steady_clock::now().time_since_epoch()).count();
}

template<class Entry, int Size>
struct HashTable 
{
	Entry* operator[](Key key) { return &table[(uint32_t)key & (Size - 1)]; }

private:
	std::vector<Entry> table = std::vector<Entry>(Size);
};

enum SyncCout { IO_LOCK, IO_UNLOCK };
std::ostream& operator<<(std::ostream&, SyncCout);

#define sync_cout std::cout << IO_LOCK
#define sync_endl std::endl << IO_UNLOCK


/// xorshift64star Pseudo-Random Number Generator
/// This class is based on original code written and dedicated
/// to the public domain by Sebastiano Vigna (2014).
/// It has the following characteristics:
///
///  -  Outputs 64-bit numbers
///  -  Passes Dieharder and SmallCrush test batteries
///  -  Does not require warm-up, no zeroland to escape
///  -  Internal state is a single 64-bit integer
///  -  Period is 2^64 - 1
///  -  Speed: 1.60 ns/call (Core i7 @3.40GHz)
///
/// For further analysis see
///   <http://vigna.di.unimi.it/ftp/papers/xorshift.pdf>

class PRNG {

	uint64_t s;

	uint64_t rand64() {

		s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
		return s * 2685821657736338717LL;
	}

public:
	PRNG(uint64_t seed) : s(seed) { }

	template<typename T> T rand() { return T(rand64()); }

	/// Special generator used to fast init magic numbers.
	/// Output values only have 1/8th of their bits set on average.
	template<typename T> T sparse_rand()
	{
		return T(rand64() & rand64() & rand64());
	}
};

#endif
