#if !defined(JUMPER_INTRINSICS_H)
#include "math.h"

inline u32
SafeTruncateUInt64(u64 value)
{
    Assert(value <= 0xFFFFFFFF);
    u32 result = (u32)value;
    return(result);
}

inline i32
SignOf(i32 value)
{
    i32 result = (value >= 0) ? 1 : -1;
    return(result);
}

inline r32
SquareRoot(r32 real)
{
    r32 result = sqrtf(real);
    return(result);
}

inline r32
AbsoluteValue(r32 real32)
{
    r32 result = (r32)fabs(real32);
    return(result);
}

inline i32
RoundReal32ToInt32(r32 real32)
{
    i32 result = (i32)roundf(real32);
    return(result);
}

inline r32
Cube(r32 r)
{
    r32 result = r * r * r;
    return(result);
}

inline i32
Cube(i32 i)
{
    i32 result = i * i * i;
    return(result);
}

inline u32 
Cube(u32 u)
{
    u32 result = u * u * u;
    return(result);
}

inline u32
RoundReal32ToUInt32(r32 real32)
{
    u32 result = (u32)roundf(real32);
    return(result);
}

inline i32
FloorReal32ToInt32(r32 real32)
{
    i32 result = (i32)floorf(real32);
    return(result);
}

inline i32
CeilReal32ToInt32(r32 real32)
{
    i32 result = (i32)ceilf(real32);
    return(result);
}

inline i32
TruncateReal32ToInt32(r32 real32)
{
    i32 result = (i32)real32;
    return(result);
}

inline r32
Sin(r32 angle)
{
    r32 result = sinf(angle);
    return(result);
}

inline r32
Cos(r32 angle)
{
    r32 result = cosf(angle);
    return(result);
}

inline r32
ATan2(r32 y, r32 x)
{
    r32 result = atan2f(y, x);
    return(result);
}

struct bit_scan_result
{
    bool32 found;
    u32 index;
};
    
inline bit_scan_result
FindLeastSignificantSetBit(u32 value)
{
    bit_scan_result result = {};
    result.found = false;

#if COMPILER_MSVC
    result.found = _BitScanForward((unsigned long*)&result.index, value);
#else
    for (u32 test = 0; test < 32; ++test)
    {
	if (value & (1 << test))
	{
	    result.index = test;
	    result.found = true;
	    break;
	}
    }
#endif
    return(result);
}
#define JUMPER_INTRINSICS_H
#endif
