// EntityType.h
#pragma once

enum class EntityType
{
    Line,
    Text
};

enum class EntityTag
{
    Grid,   // background
    Scene,  // main world entities
    Cursor, // overlay cursor/picker
    Hud     // top overlay
};
