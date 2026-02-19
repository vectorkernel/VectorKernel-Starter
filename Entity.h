// Entity.h
#pragma once

#include <cstddef>
#include "EntityType.h"
#include "LineEntity.h"
#include "TextEntity.h"

// A single drawable thing in the scene.
// We store both payloads and select which is active via EntityType.
struct Entity
{
    std::size_t id = 0;

    EntityType type = EntityType::Line;
    EntityTag  tag = EntityTag::Scene;

    // If true, this entity is interpreted in screen-space (UI overlay).
    // Kept as a simple flag because several call sites reference it directly.
    bool screenSpace = false;

    int drawOrder = 0;

    LineEntity line{};
    TextEntity text{};
};
