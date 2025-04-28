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

#define JUMPER_INTRINSICS_H
#endif
