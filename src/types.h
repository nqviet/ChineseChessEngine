#ifndef __TYPES_H__
#define __TYPES_H__

#include <cstdint>
#include <string>
#include <nmmintrin.h>	// Intel and Microsoft header for _mm_popcnt_u64()
#include <intrin.h>		// Microsoft header for _BitScanForward64()
#include <xmmintrin.h> // Intel and Microsoft header for _mm_prefetch()

#include <immintrin.h> // Header for _pext_u64() intrinsic
#define pext(b, m) _pext_u64(b, m)

#if defined(_MSC_VER)
// Disable some silly and noisy warning from MSVC compiler
#pragma warning(disable: 4127) // Conditional expression is constant
#pragma warning(disable: 4146) // Unary minus operator applied to unsigned type
#pragma warning(disable: 4800) // Forcing value to bool 'true' or 'false'
#endif

#define MM_SHL_SI128(v, n) \
{ \
    __m128i v1, v2; \
 \
    if ((n) >= 64) \
    { \
        v1 = _mm_slli_si128(v, 8); \
        v1 = _mm_slli_epi64(v1, (n) - 64); \
    } \
    else \
    { \
        v1 = _mm_slli_epi64(v, n); \
        v2 = _mm_slli_si128(v, 8); \
        v2 = _mm_srli_epi64(v2, 64 - (n)); \
        v1 = _mm_or_si128(v1, v2); \
    } \
    v = v1; \
}

#define MM_SHR_SI128(v, n) \
{ \
    __m128i v1, v2; \
 \
    if ((n) >= 64) \
    { \
        v1 = _mm_srli_si128(v, 8); \
        v1 = _mm_srli_epi64(v1, (n) - 64); \
    } \
    else \
    { \
        v1 = _mm_srli_epi64(v, n); \
        v2 = _mm_srli_si128(v, 8); \
        v2 = _mm_slli_epi64(v2, 64 - (n)); \
        v1 = _mm_or_si128(v1, v2); \
    } \
    v = v1; \
}

inline int pext_si128(__m128i src, __m128i mask)
{
	int shift;
	uint64_t low = pext(src.m128i_u64[0], mask.m128i_u64[0]);
	uint64_t high = pext(src.m128i_u64[1], mask.m128i_u64[1]);
	
	shift = (int)_mm_popcnt_u64(mask.m128i_u64[0]);

	return ((high << shift) | low) & 0xFFFFFFFF;
}

struct sPair64
{
	sPair64(uint64_t first, uint64_t second)
	{		
		this->v[0] = first;
		this->v[1] = second;
	}

	sPair64(uint64_t first)
	{
		this->v[0] = first;
		
		if (first & 0x8000000000000000ULL)
			this->v[1] = 0xFFFFFFFFFFFFFFFFULL;
		else
			this->v[1] = 0;
	}

	sPair64()
	{
		this->v[0] = 0;
		this->v[1] = 0;
	}

	union
	{
		/// little big indian
		uint64_t v[2];
		struct { uint64_t lower, upper; };
	};
};

typedef sPair64 pair64;

typedef uint64_t Key;

const int MAX_MOVES = 256;
const int MAX_PLY = 128;

//--------------------------------------------------------------------------------
namespace
{
	const char* SquareToChar[] = {
		"A1", "B1", "C1", "D1", "E1", "F1", "G1", "H1", "I1",
		"A2", "B2", "C2", "D2", "E2", "F2", "G2", "H2", "I2",
		"A3", "B3", "C3", "D3", "E3", "F3", "G3", "H3", "I3",
		"A4", "B4", "C4", "D4", "E4", "F4", "G4", "H4", "I4",
		"A5", "B5", "C5", "D5", "E5", "F5", "G5", "H5", "I5",
		"A6", "B6", "C6", "D6", "E6", "F6", "G6", "H6", "I6",
		"A7", "B7", "C7", "D7", "E7", "F7", "G7", "H7", "I7",
		"A8", "B8", "C8", "D8", "E8", "F8", "G8", "H8", "I8",
		"A9", "B9", "C9", "D9", "E9", "F9", "G9", "H9", "I9",
		"A10", "B10", "C10", "D10", "E10", "F10", "G10", "H10", "I10",
		"NONE"
	};
}

enum Move : int 
{
	MOVE_NONE,
	MOVE_NULL = 91
};

enum MoveType 
{
	NORMAL,
	RESERVE0	= 1 << 14,
	RESERVE1	= 2 << 14,
	RESERVE2	= 3 << 14
};

enum Color 
{
	WHITE, BLACK, NO_COLOR, COLOR_NB = 2
};

enum ScaleFactor 
{
	SCALE_FACTOR_DRAW = 0,
	SCALE_FACTOR_ONEPAWN = 48,
	SCALE_FACTOR_NORMAL = 64,
	SCALE_FACTOR_MAX = 128,
	SCALE_FACTOR_NONE = 255
};

enum Phase 
{
	PHASE_ENDGAME,
	PHASE_MIDGAME = 128,
	MG = 0, EG = 1, PHASE_NB = 2
};

enum Bound 
{
	BOUND_NONE,
	BOUND_UPPER,
	BOUND_LOWER,
	BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

enum Value : int 
{
	VALUE_ZERO = 0,
	VALUE_DRAW = 0,
	VALUE_KNOWN_WIN = 10000,
	VALUE_MATE = 32000,
	VALUE_INFINITE = 32001,
	VALUE_NONE = 32002,

	VALUE_MATE_IN_MAX_PLY = /*VALUE_MATE*/32000 - 2 * MAX_PLY,
	VALUE_MATED_IN_MAX_PLY = -/*VALUE_MATE*/32000 + 2 * MAX_PLY,

	AdvisorValueMg = 188, AdvisorValueEg = 248,
	SoldierValueMg = 188, SoldierValueEg = 248,
	HorseValueMg = 753, HorseValueEg = 832,
	ElephantValueMg = 826, ElephantValueEg = 897,
	CannonValueMg = 1285, CannonValueEg = 1371,
	ChariotValueMg = 2513, ChariotValueEg = 2650,

	MidgameLimit = 15258, EndgameLimit = 3915
};

enum PieceType 
{
	NO_PIECE_TYPE, SOLDIER, HORSE, ELEPHANT, CANNON, CHARIOT, ADVISOR, GENERAL,
	ALL_PIECES = 0,
	PIECE_TYPE_NB = 9
};

enum Piece 
{
	NO_PIECE,
	W_SOLDIER = 1, W_HORSE, W_ELEPHANT, W_CANNON, W_CHARIOT, W_ADVISOR, W_GENERAL,
	B_SOLDIER = 9, B_HORSE, B_ELEPHANT, B_CANNON, B_CHARIOT, B_ADVISOR, B_GENERAL,
	PIECE_NB = 16
};

const Piece Pieces[] = 
{
	W_SOLDIER, W_HORSE, W_ELEPHANT, W_CANNON, W_CHARIOT, W_ADVISOR, W_GENERAL,
	B_SOLDIER, B_HORSE, B_ELEPHANT, B_CANNON, B_CHARIOT, B_ADVISOR, B_GENERAL
};
extern Value PieceValue[PHASE_NB][PIECE_NB];

enum Depth 
{

	ONE_PLY = 1,

	DEPTH_ZERO = 0 * /*ONE_PLY*/1,
	DEPTH_QS_CHECKS = 0 * /*ONE_PLY*/1,
	DEPTH_QS_NO_CHECKS = -1 * /*ONE_PLY*/1,
	DEPTH_QS_RECAPTURES = -5 * /*ONE_PLY*/1,

	DEPTH_NONE = -6 * /*ONE_PLY*/1,
	DEPTH_MAX = MAX_PLY * /*ONE_PLY*/1
};

enum Square
{
	PT_A1, PT_B1, PT_C1, PT_D1, PT_E1, PT_F1, PT_G1, PT_H1, PT_I1,
	PT_A2, PT_B2, PT_C2, PT_D2, PT_E2, PT_F2, PT_G2, PT_H2, PT_I2,
	PT_A3, PT_B3, PT_C3, PT_D3, PT_E3, PT_F3, PT_G3, PT_H3, PT_I3,
	PT_A4, PT_B4, PT_C4, PT_D4, PT_E4, PT_F4, PT_G4, PT_H4, PT_I4,
	PT_A5, PT_B5, PT_C5, PT_D5, PT_E5, PT_F5, PT_G5, PT_H5, PT_I5,
	PT_A6, PT_B6, PT_C6, PT_D6, PT_E6, PT_F6, PT_G6, PT_H6, PT_I6,
	PT_A7, PT_B7, PT_C7, PT_D7, PT_E7, PT_F7, PT_G7, PT_H7, PT_I7,
	PT_A8, PT_B8, PT_C8, PT_D8, PT_E8, PT_F8, PT_G8, PT_H8, PT_I8,
	PT_A9, PT_B9, PT_C9, PT_D9, PT_E9, PT_F9, PT_G9, PT_H9, PT_I9,
	PT_A10, PT_B10, PT_C10, PT_D10, PT_E10, PT_F10, PT_G10, PT_H10, PT_I10,
	PT_NONE,

	POINT_NB = 90,
	SQUARE_NB = POINT_NB,

	DIR_NONE = 0,

	NORTH = 9,
	EAST = 1,
	SOUTH = -9,
	WEST = -1,	

	NORTH_EAST = /*NORTH*/9 + /*EAST*/1,
	SOUTH_EAST = /*SOUTH*/-9 + /*EAST*/1,
	SOUTH_WEST = /*SOUTH*/-9 + /*WEST*/-1,
	NORTH_WEST = /*NORTH*/9 + /*WEST*/-1,	
};

enum File : int 
{
	FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_I, FILE_NB
};

enum Rank : int 
{
	RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9, RANK_10, RANK_NB
};

/// Score enum stores a middlegame and an endgame value in a single integer
/// (enum). The least significant 16 bits are used to store the endgame value
/// and the upper 16 bits are used to store the middlegame value. Take some
/// care to avoid left-shifting a signed int to avoid undefined behavior.
enum Score : int { SCORE_ZERO };

inline Score make_score(int mg, int eg) 
{
	return Score((int)((unsigned int)eg << 16) + mg);
}

/// Extracting the signed lower and upper 16 bits is not so trivial because
/// according to the standard a simple cast to short is implementation defined
/// and so is a right shift of a signed integer.
inline Value eg_value(Score s) 
{

	union { uint16_t u; int16_t s; } eg = { uint16_t(unsigned(s + 0x8000) >> 16) };
	return Value(eg.s);
}

inline Value mg_value(Score s) 
{

	union { uint16_t u; int16_t s; } mg = { uint16_t(unsigned(s)) };
	return Value(mg.s);
}

#define ENABLE_BASE_OPERATORS_ON(T)                             \
inline T operator+(T d1, T d2) { return T(int(d1) + int(d2)); } \
inline T operator+(T d1, int d2) { return T(int(d1) + d2); }	\
inline T operator-(T d1, T d2) { return T(int(d1) - int(d2)); } \
inline T operator-(T d1, int d2) { return T(int(d1) - d2); }	\
inline T operator*(int i, T d) { return T(i * int(d)); }        \
inline T operator*(T d1, T d2) { return T(int(d1) * int(d2)); } \
inline T operator*(T d, int i) { return T(int(d) * i); }        \
inline T operator-(T d) { return T(-int(d)); }                  \
inline T operator^(T d1, T d2) { return T(int(d1) ^ int(d2)); }	\
inline T operator&(T d1, T d2) { return T(int(d1) & int(d2)); }	\
inline T operator|(T d1, T d2) { return T(int(d1) | int(d2)); }	\
inline T& operator+=(T& d1, T d2) { return d1 = d1 + d2; }      \
inline T& operator-=(T& d1, T d2) { return d1 = d1 - d2; }      \
inline T& operator*=(T& d, int i) { return d = T(int(d) * i); } 

#define ENABLE_FULL_OPERATORS_ON(T)                             \
ENABLE_BASE_OPERATORS_ON(T)                                     \
inline T& operator++(T& d) { return d = T(int(d) + 1); }        \
inline T& operator--(T& d) { return d = T(int(d) - 1); }        \
inline T operator/(T d, int i) { return T(int(d) / i); }        \
inline int operator/(T d1, T d2) { return int(d1) / int(d2); }  \
inline T& operator/=(T& d, int i) { return d = T(int(d) / i); }

ENABLE_FULL_OPERATORS_ON(Value)
ENABLE_FULL_OPERATORS_ON(PieceType)
ENABLE_FULL_OPERATORS_ON(Piece)
ENABLE_FULL_OPERATORS_ON(Color)
ENABLE_FULL_OPERATORS_ON(Depth)
ENABLE_FULL_OPERATORS_ON(Square)
ENABLE_FULL_OPERATORS_ON(File)
ENABLE_FULL_OPERATORS_ON(Rank)

ENABLE_BASE_OPERATORS_ON(Score)

#undef ENABLE_FULL_OPERATORS_ON
#undef ENABLE_BASE_OPERATORS_ON

inline Color operator~(Color c) 
{
	return Color(c ^ BLACK); // Toggle color
}

inline Square operator~(Square s)
{
	return Square(int(RANK_NB - s / FILE_NB - 1) * FILE_NB + (s % FILE_NB)); // vertical flip
}

inline Piece operator~(Piece pc) 
{
	return Piece(pc ^ 8); // Swap color of piece B_KNIGHT -> W_KNIGHT
}

inline Value mate_in(int ply) 
{
	return VALUE_MATE - Value(ply);
}

inline Value mated_in(int ply) 
{
	return -VALUE_MATE + ply;
}

inline Square make_square(File f, Rank r) 
{
	return Square(((r << 3) + r) + f);
}

inline Piece make_piece(Color c, PieceType pt) 
{
	return Piece((c << 3 ) + pt);
}

inline PieceType type_of(Piece pc) 
{
	return PieceType(pc & 7);
}

inline Color color_of(Piece pc) 
{	
	return Color(pc >> 3);
}

inline bool is_ok(Square s) 
{
	return s >= PT_A1 && s <= PT_I10;
}

inline File file_of(Square s) 
{
	return File(s % int(FILE_NB));
}

inline Rank rank_of(Square s) 
{
	return Rank(s / int(FILE_NB));
}

inline Square relative_square(Color c, Square s) 
{
	return Square((1 - c) * s + c * int((FILE_NB - s / FILE_NB) * FILE_NB + (s % FILE_NB)));
}

inline Rank relative_rank(Color c, Rank r) 
{
	return Rank((r ^ (c * 15)) + c * -6);
}

inline Rank relative_rank(Color c, Square s) 
{
	return relative_rank(c, rank_of(s));
}

inline Square pawn_push(Color c) 
{
	return c == WHITE ? NORTH : SOUTH;
}

inline Square from_sq(Move m) 
{
	return Square((m >> 7) & 0x7F);
}

inline Square to_sq(Move m) 
{
	return Square(m & 0x7F);
}

inline MoveType type_of(Move m) 
{
	return MoveType(0);
}

inline Move make_move(Square from, Square to) 
{
	return Move((from << 7) + to);
}

template<MoveType T>
inline Move make(Square from, Square to, PieceType pt = HORSE)
{
	return Move(T + ((pt - HORSE) << 14) + (from << 7) + to);
}

inline bool is_ok(Move m) 
{
	return from_sq(m) != to_sq(m); // Catch MOVE_NULL and MOVE_NONE
}

inline const std::string sqToStr(Move m)
{
	std::string s;
	
	s.append(SquareToChar[from_sq(m)]);
	s.append("-");
	s.append(SquareToChar[to_sq(m)]);

	return std::move(s);
}

#endif