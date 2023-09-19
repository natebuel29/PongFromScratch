#pragma once

#include "my_math.h"
#include "defines.h"

uint32_t constexpr MAX_LABELS = 100;

struct Label
{
    char *text;
    Vec2 pos;
};

struct UIState
{
    uint32_t label;
    Label labels[MAX_LABELS];
};

void do_text(UIState *uistate, Vec2 pos, char *text);