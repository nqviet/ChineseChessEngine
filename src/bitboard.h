#ifndef __BITBOARD_H__
#define __BITBOARD_H__

#include <string>
#include <stdio.h>

#include "types.h"

namespace Bitbases 
{
	void init();
	bool probe(Square wksq, Square wpsq, Square bksq, Color us);
}

class Bitboard;
namespace Bitboards
{
	void init();
	const std::string pretty(Bitboard b);
}

class Bitboard
{
public:
	Bitboard() 
	{ 
		v = _mm_setzero_si128();
	}

	Bitboard(__m128i v)
	{
		this->v = v;
	}
	
	Bitboard(pair64 c)
	{
		__m128i *p = (__m128i*)  c.v;
		v = _mm_load_si128(p);		
	}

	Bitboard(uint64_t n)
	{		
		v = Bitboard(pair64(n)).v;
	}
	
	Bitboard(const Bitboard& b)
	{
		v = b.v;
	}

	Bitboard& operator=(const pair64& c)
	{
		__m128i *p = (__m128i*)  c.v;
		v = _mm_load_si128(p);
		return *this;
	}

	Bitboard& operator=(uint64_t n)
	{
		*this = pair64(n);
		return *this;
	}

	Bitboard& operator=(const Bitboard& b)
	{
		v = b.v;
		return *this;
	}

	operator const bool() const
	{		
		return v.m128i_i64[0] || v.m128i_i64[1];
	}
	
	operator pair64()
	{
		pair64 bb;
		__m128i *p = (__m128i*)  bb.v;
		_mm_store_si128(p, v);
		return bb;
	}

	operator __m128i()
	{
		return v;
	}

	void reset()
	{
		v = _mm_setzero_si128();
	}

	std::string str()
	{
		char str[64];

		sprintf_s(str, "0x%llx-0x%llx", v.m128i_u64[0], v.m128i_u64[1]);
		return std::string(std::move(str));
	}
public:
	__m128i v;
};

inline Bitboard operator << (Bitboard b, int n)
{
	__m128i tmp = b.v;
	MM_SHL_SI128(tmp, n);
	return tmp;
}

inline Bitboard operator >> (Bitboard b, int n)
{
	__m128i tmp = b.v;
	MM_SHR_SI128(tmp, n);
	return tmp;
}

const Bitboard one_epi64 = pair64(1, 1);
const Bitboard one_si128 = 1;
const Bitboard carryone_si128 = pair64(0, 1);
const Bitboard setone_si128 = pair64(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL);

inline bool overflow_l_epi64(__m128i a, __m128i b)
{	
	__m128i x, y;	
	x = _mm_or_si128(a, b);
	x = _mm_xor_si128(x, setone_si128.v);
	y = _mm_and_si128(a, b);
	
	return (x.m128i_u64[0] == 0ULL && y.m128i_u64[0] > 0ULL) || (x.m128i_u64[0] != 0ULL && x.m128i_u64[0] < y.m128i_u64[0]);
}


const Bitboard FileABB = pair64(0x8040201008040201ULL, 0x20100ULL);
const Bitboard FileBBB = FileABB << 1;
const Bitboard FileCBB = FileABB << 2;
const Bitboard FileDBB = FileABB << 3;
const Bitboard FileEBB = FileABB << 4;
const Bitboard FileFBB = FileABB << 5;
const Bitboard FileGBB = FileABB << 6;
const Bitboard FileHBB = FileABB << 7;
const Bitboard FileIBB = FileABB << 8;

const Bitboard Rank1BB = 0x1FF;
const Bitboard Rank2BB = Rank1BB << (9 * 1);
const Bitboard Rank3BB = Rank1BB << (9 * 2);
const Bitboard Rank4BB = Rank1BB << (9 * 3);
const Bitboard Rank5BB = Rank1BB << (9 * 4);
const Bitboard Rank6BB = Rank1BB << (9 * 5);
const Bitboard Rank7BB = Rank1BB << (9 * 6);
const Bitboard Rank8BB = Rank1BB << (9 * 7);
const Bitboard Rank9BB = Rank1BB << (9 * 8);
const Bitboard Rank10BB = Rank1BB << (9 * 9);

extern int SquareDistance[SQUARE_NB][SQUARE_NB];

extern Bitboard SquareBB[SQUARE_NB];
extern Bitboard FileBB[FILE_NB];
extern Bitboard RankBB[RANK_NB];
extern Bitboard AdjacentFilesBB[FILE_NB];
extern Bitboard InFrontBB[COLOR_NB][RANK_NB];
extern Bitboard StepAttacksBB[PIECE_NB][SQUARE_NB];
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];
extern Bitboard DistanceRingBB[SQUARE_NB][10];
extern Bitboard ForwardBB[COLOR_NB][SQUARE_NB];
extern Bitboard PassedPawnMask[COLOR_NB][SQUARE_NB];
extern Bitboard PawnAttackSpan[COLOR_NB][SQUARE_NB];
extern Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];

/// bitwise and
inline Bitboard operator&(Bitboard b, Square s)
{
	return _mm_and_si128(b.v, SquareBB[s].v);
}

inline Bitboard operator&(Bitboard b, Bitboard d)
{
	return _mm_and_si128(b.v, d.v);
}

/// bitwise or
inline Bitboard operator|(Bitboard b, Square s) 
{
	return _mm_or_si128(b.v, SquareBB[s].v);
}

inline Bitboard operator|(Bitboard b, Bitboard d)
{
	return _mm_or_si128(b.v, d.v);
}

/// bitwise xor
inline Bitboard operator^(Bitboard b, Square s) 
{
	return _mm_xor_si128(b.v, SquareBB[s].v);
}

inline Bitboard operator^(Bitboard b, Bitboard d)
{
	return _mm_xor_si128(b.v, d.v);
}

/// bitwise not
inline Bitboard operator~(Bitboard b)
{
	return _mm_xor_si128(b.v, setone_si128.v);
}

inline Bitboard& operator&=(Bitboard& b, Square s)
{
	b.v = _mm_and_si128(b.v, SquareBB[s].v);
	return b;
}

inline Bitboard& operator&=(Bitboard& b, Bitboard d)
{
	b.v = _mm_and_si128(b.v, d.v);
	return b;
}

inline Bitboard& operator|=(Bitboard& b, Square s) 
{
	b.v = _mm_or_si128(b.v, SquareBB[s].v);
	return b;
}

inline Bitboard& operator|=(Bitboard& b, Bitboard d)
{
	b.v = _mm_or_si128(b.v, d.v);
	return b;
}

inline Bitboard& operator^=(Bitboard& b, Square s) 
{
	b.v = _mm_xor_si128(b.v, SquareBB[s].v);
	return b;
}

inline Bitboard& operator^=(Bitboard& b, Bitboard d)
{
	b.v = _mm_xor_si128(b.v, d.v);
	return b;
}

/// arithmetic substract
inline Bitboard operator-(Bitboard b, Bitboard d)
{
	__m128i x = _mm_xor_si128(d.v, setone_si128.v);
	if (overflow_l_epi64(x, one_si128.v))
		x = _mm_add_epi64(x, one_epi64.v);
	else
		x = _mm_add_epi64(x, one_si128.v);

	if (overflow_l_epi64(b.v, x))
	{
		x = _mm_add_epi64(b.v, x);
		x = _mm_add_epi64(x, carryone_si128.v);
	}
	else
		x = _mm_add_epi64(b.v, x);

	return x;
}

inline Bitboard operator-(Bitboard b, int n)
{	
	return b - Bitboard(n);
}

inline bool more_than_one(Bitboard b) 
{
	return b & (b - 1);
}

/// rank_bb() and file_bb() return a bitboard representing all the points on
/// the given file or rank.

inline Bitboard rank_bb(Rank r) 
{
	return RankBB[r];
}

inline Bitboard rank_bb(Square s) 
{
	return RankBB[rank_of(s)];
}

inline Bitboard file_bb(File f) 
{
	return FileBB[f];
}

inline Bitboard file_bb(Square s) 
{
	return FileBB[file_of(s)];
}

/// shift() moves a bitboard one step along direction D. Mainly for soldiers
template<Square D>
inline Bitboard shift(Bitboard b) 
{
	return	D == NORTH ? b << 9 : D == SOUTH ? b >> 9
		: D == EAST ? (b & ~FileIBB) << 1 : D == WEST ? (b & ~FileABB) >> 1
		: 0;
}

/// adjacent_files_bb() returns a bitboard representing all the squares on the
/// adjacent files of the given one.
inline Bitboard adjacent_files_bb(File f) 
{
	return AdjacentFilesBB[f];
}

/// between_bb() returns a bitboard representing all the points between the two
/// given ones. For instance, between_bb(PT_C4, PT_F7) returns a bitboard with
/// the bits for points d5 and e6 set. If s1 and s2 are not on the same rank, file
/// or diagonal, 0 is returned.
inline Bitboard between_bb(Square s1, Square s2) 
{
	return BetweenBB[s1][s2];
}

/// in_front_bb() returns a bitboard representing all the points on all the ranks
/// in front of the given one, from the point of view of the given color. For
/// instance, in_front_bb(BLACK, RANK_3) will return the points on ranks 1 and 2.
inline Bitboard in_front_bb(Color c, Rank r) 
{
	return InFrontBB[c][r];
}

/// forward_bb() returns a bitboard representing all the points along the line
/// in front of the given one, from the point of view of the given color:
///        ForwardBB[c][s] = in_front_bb(c, s) & file_bb(s)
inline Bitboard forward_bb(Color c, Square s) 
{
	return ForwardBB[c][s];
}

/// pawn_attack_span() returns a bitboard representing all the points that can be
/// attacked by a pawn of the given color when it moves along its file, starting
/// from the given point:
///       PawnAttackSpan[c][s] = in_front_bb(c, s) & adjacent_files_bb(s);
inline Bitboard pawn_attack_span(Color c, Square s) 
{
	return PawnAttackSpan[c][s];
}

/// passed_pawn_mask() returns a bitboard mask which can be used to test if a
/// pawn of the given color and on the given point is a passed pawn:
///       PassedPawnMask[c][s] = pawn_attack_span(c, s) | forward_bb(c, s)
inline Bitboard passed_pawn_mask(Color c, Square s) 
{
	return PassedPawnMask[c][s];
}

/// aligned() returns true if the squares s1, s2 and s3 are aligned either on a
/// straight or on a diagonal line.
inline bool aligned(Square s1, Square s2, Square s3) 
{
	return LineBB[s1][s2] & s3;
}

/// distance() functions return the distance between x and y, defined as the
/// number of steps for a king in x to reach y. Works with squares, ranks, files.

template<typename T> inline int distance(T x, T y) { return x < y ? y - x : x - y; }
template<> inline int distance(Square x, Square y) { return SquareDistance[x][y]; }

template<typename T1, typename T2> inline int distance(T2 x, T2 y) { return 0; };
template<> inline int distance<File>(Square x, Square y) { return distance(file_of(x), file_of(y)); }
template<> inline int distance<Rank>(Square x, Square y) { return distance(rank_of(x), rank_of(y)); }

/// attacks_bb() returns a bitboard representing all the squares attacked by a
/// piece of type Pt (bishop or rook) placed on 's'. The helper magic_index()
/// looks up the index using the 'magic bitboards' approach.
template<PieceType Pt>
inline unsigned magic_index(Square s, Bitboard occupied)
{
	extern Bitboard ChariotMasks[SQUARE_NB];
	extern Bitboard CannonMasks[SQUARE_NB];
	extern Bitboard HorseMasks[SQUARE_NB];
	extern Bitboard ElephantMasks[SQUARE_NB];

	Bitboard* const Masks = Pt == CHARIOT ? ChariotMasks : 
							Pt == CANNON ? CannonMasks : 
							Pt == HORSE ? HorseMasks : ElephantMasks;

	return unsigned(pext_si128(occupied.v, Masks[s].v));
}

template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occupied) 
{

	extern Bitboard* ChariotAttacks[SQUARE_NB];
	extern Bitboard* CannonAttacks[SQUARE_NB];
	extern Bitboard* HorseAttacks[SQUARE_NB];
	extern Bitboard* ElephantAttacks[SQUARE_NB];

	unsigned idx = magic_index<Pt>(s, occupied);
	
	return (Pt == CHARIOT ? ChariotAttacks : 
			Pt == CANNON ? CannonAttacks : 
			Pt == HORSE ? HorseAttacks : ElephantAttacks)[s][idx];
}

inline Bitboard attacks_bb(Piece pc, Square s, Bitboard occupied) 
{

	switch (type_of(pc))
	{
	case CANNON: return attacks_bb<CANNON>(s, occupied);
	case CHARIOT: return attacks_bb<CHARIOT>(s, occupied);
	case HORSE: return attacks_bb<HORSE>(s, occupied);
	case ELEPHANT: return attacks_bb<ELEPHANT>(s, occupied);
	default: return StepAttacksBB[pc][s];
	}
}

/// popcount() counts the number of non-zero bits in a bitboard
inline int popcount(Bitboard b) 
{	
	return (int)_mm_popcnt_u64(b.v.m128i_u64[1]) + (int)_mm_popcnt_u64(b.v.m128i_u64[0]);
}

/// lsb() and msb() return the least/most significant bit in a non-zero bitboard
inline Square lsb(Bitboard b) 
{	
	unsigned long idx1, idx2;
		
	if (b.v.m128i_u64[0])
	{
		_BitScanForward64(&idx1, b.v.m128i_u64[0]);
		return (Square)idx1;
	}

	_BitScanForward64(&idx2, b.v.m128i_u64[1]);
	return (Square)(idx2 + 64);
}

inline Square msb(Bitboard b) 
{
	unsigned long idx1, idx2;
		
	if (b.v.m128i_u64[1])
	{
		_BitScanReverse64(&idx1, b.v.m128i_u64[1]);
		return (Square)(idx1 + 64);
	}

	_BitScanReverse64(&idx2, b.v.m128i_u64[0]);
	return (Square) idx2;
}

/// pop_lsb() finds and clears the least significant bit in a non-zero bitboard
inline Square pop_lsb(Bitboard* b) 
{
	const Square s = lsb(*b);
	*b = *b & (*b - 1);
	return s;
}

/// frontmost_sq() and backmost_sq() return the square corresponding to the
/// most/least advanced bit relative to the given color.
inline Square frontmost_sq(Color c, Bitboard b) { return c == WHITE ? msb(b) : lsb(b); }
inline Square  backmost_sq(Color c, Bitboard b) { return c == WHITE ? lsb(b) : msb(b); }

#endif