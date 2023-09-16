#ifdef NBENGINE
#pragma once
#endif
struct GlobalData
{
    int screenSizeX;
    int screenSizeY;
};

struct Transform
{
    float xPos;
    float yPos;
    float sizeX;
    float sizeY;
};

struct PushData
{
    int transformIdx;
};