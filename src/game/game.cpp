#include "defines.h"
#include "renderer/shared_render_types.h"

uint32_t constexpr MAX_ENTITIES = 100;

struct Entity
{
    Transform transform;
};

struct GameState
{
    uint32_t entityCount;
    Entity entities[MAX_ENTITIES];
};

internal Entity *create_entity(GameState *gameState, Transform transform)
{
    Entity *e = 0;
    if (gameState->entityCount < MAX_ENTITIES)
    {
        e = &gameState->entities[gameState->entityCount++];
        e->transform = transform;
    }
    else
    {
        NB_ASSERT(0, "Reached Maximum amount of entitites!");
    }

    return e;
}

void update_game(GameState *gameState)
{
}

bool init_game(GameState *gameState)
{
    Entity *e = create_entity(gameState, {100.0f, 100.0f, 100.0f, 100.0f});

    return true;
}