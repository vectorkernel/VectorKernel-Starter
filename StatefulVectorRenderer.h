#pragma once

#include "EntityBook.h"
#include "LinePass.h"
#include "RenderContext.h"

#include <vector>

class StatefulVectorRenderer
{
public:
    void Init();

    void SetEntityBook(const EntityBook* book);
    void MarkDirty();

    // Draw cached/static batches
    void Redraw(const RenderContext& ctx);

private:
    void RebuildBatchesIfDirty();

private:
    const EntityBook* entityBook = nullptr;
    bool dirty = true;

    // We batch everything into lines for now (lines + text -> line segments)
    std::vector<LineEntity> cachedWorldLines;
    std::vector<LineEntity> cachedHudLines;

    LinePass worldPass;
    LinePass hudPass;
};

