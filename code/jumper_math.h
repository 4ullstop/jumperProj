#if !defined (JUMPER_MATH_H)

//define our vector 2 overloaded operations

struct v2
{
    union
    {
	struct
	{
	    r32 x, y;
	};
	r32 e[2];
    };

    inline v2 &operator*=(r32 a);

    inline v2 &operator+=(v2 a);

};

inline v2
V2(r32 x, r32 y)
{
    v2 result;
    result.x = x;
    result.y = y;

    return(result);
}

inline v2
operator*(r32 a, v2 b)
{
    v2 result;
    result.x = a * b.x;
    result.y = a * b.y;    

    return(result); 
}

inline v2
operator*(v2 b, r32 a)
{
    v2 result = a * b;

    return(result); 
}

inline v2 &v2::
operator*=(r32 a)
{
    *this = a * *this;
    return(*this);
}

inline v2
operator-(v2 a)
{
    v2 result;
    result.x = -a.x;
    result.y = -a.y;    

    return(result);
}

inline v2
operator+(v2 a, v2 b)
{
    v2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return(result);
}

inline v2 &v2::
operator+=(v2 a)
{
    *this = *this + a;

    return(*this);
}

inline v2
operator-(v2 a, v2 b)
{
    v2 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return(result);
}

inline r32
Square(r32 a)
{
    r32 result = a * a;

    return(result);
}

inline r32
Inner(v2 a, v2 b)
{
    r32 result = a.x * b.x + a.y * b.y;
    return(result);
}

inline r32
LengthSq(v2 a)
{
    r32 result = Inner(a, a);

    return(result);
}

#define JUMPER_MATH_H
#endif
