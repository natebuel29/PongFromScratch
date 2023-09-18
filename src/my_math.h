#pragma once

struct Vec2
{

    float x;
    float y;

    Vec2 operator/(float scalar)
    {
        return {x / scalar, y / scalar};
    }

    Vec2 operator*(float scalar)
    {
        return {x * scalar, y * scalar};
    }
};

struct Rect
{
    union
    {
        Vec2 pos;
        Vec2 offset;
    };
    Vec2 size;
};

struct Vec4
{
    union
    {

        struct
        {
            float r;
            float g;
            float b;
            float a;
        };
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
    };

    bool operator==(Vec4 other)
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};

float clamp(float value, float a, float b)
{
    if (value < a)
    {
        return a;
    }
    else if (value > b)
    {
        return b;
    }
    else
    {
        return value;
    }
}
