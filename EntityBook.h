// EntityBook.h
#pragma once
#include <vector>
#include "Entity.h"

class EntityBook
{
public:
    EntityBook() = default;

    Entity& AddEntity(const Entity& e);

    template <typename Pred>
    void RemoveIf(Pred&& pred)
    {
        entities.erase(
            std::remove_if(entities.begin(), entities.end(), std::forward<Pred>(pred)),
            entities.end()
        );
    }

    void Clear();

    const std::vector<Entity>& GetEntities() const;
    std::vector<Entity>& GetEntitiesMutable();

    void SortByDrawOrder();

private:
    std::vector<Entity> entities;
};
