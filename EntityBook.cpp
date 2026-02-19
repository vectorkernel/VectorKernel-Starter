// EntityBook.cpp
#include "EntityBook.h"
#include <algorithm>

Entity& EntityBook::AddEntity(const Entity& e)
{
    entities.push_back(e);
    return entities.back();
}

void EntityBook::Clear()
{
    entities.clear();
}

const std::vector<Entity>& EntityBook::GetEntities() const
{
    return entities;
}

std::vector<Entity>& EntityBook::GetEntitiesMutable()
{
    return entities;
}

// Stable sort so insertion order is preserved within a drawOrder.
void EntityBook::SortByDrawOrder()
{
    std::stable_sort(entities.begin(), entities.end(),
        [](const Entity& a, const Entity& b)
        {
            if (a.drawOrder != b.drawOrder) return a.drawOrder < b.drawOrder;

            auto layer = [](EntityTag t)
                {
                    switch (t)
                    {
                    case EntityTag::Grid:   return 0;
                    case EntityTag::Scene:  return 1;
                    case EntityTag::Cursor: return 2;
                    case EntityTag::Hud:    return 3;
                    default:                return 1;
                    }
                };

            int la = layer(a.tag);
            int lb = layer(b.tag);
            if (la != lb) return la < lb;

            return a.id < b.id;
        });
}
