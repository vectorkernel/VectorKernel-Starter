// StatefulVectorRenderer.cpp
#include "StatefulVectorRenderer.h"

#include "HersheyTextBuilder.h"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // glm::ortho

void StatefulVectorRenderer::Init()
{
    worldPass.Init();
    hudPass.Init();
}

void StatefulVectorRenderer::SetEntityBook(const EntityBook* book)
{
    entityBook = book;
    dirty = true;
}

void StatefulVectorRenderer::MarkDirty()
{
    dirty = true;
}

void StatefulVectorRenderer::RebuildBatchesIfDirty()
{
    if (!dirty || !entityBook)
        return;

    cachedWorldLines.clear();
    cachedHudLines.clear();

    const auto& entities = entityBook->GetEntities();

    for (const Entity& e : entities)
    {
        if (e.type == EntityType::Line)
        {
            if (e.screenSpace) cachedHudLines.push_back(e.line);
            else               cachedWorldLines.push_back(e.line);
        }
        else if (e.type == EntityType::Text)
        {
            if (e.screenSpace)
                HersheyTextBuilder::BuildLines(e.text, cachedHudLines);
            else
                HersheyTextBuilder::BuildLines(e.text, cachedWorldLines);
        }
    }

    worldPass.BuildStatic(cachedWorldLines);
    hudPass.BuildStatic(cachedHudLines);

    dirty = false;

    //std::cout << "[StatefulVectorRenderer] rebuilt: worldLines=" << cachedWorldLines.size()
              //<< " hudLines=" << cachedHudLines.size() << std::endl;
}

// Extract viewport dimensions from glm::ortho(0, w, h, 0, -1, 1) (Y-down).
static void ExtractViewportWH_FromOrthoYDown(const glm::mat4& proj, float& outW, float& outH)
{
    const float m00 = proj[0][0];
    const float m11 = proj[1][1];

    outW = (m00 != 0.0f) ? (2.0f / m00) : 1.0f;
    outH = (m11 != 0.0f) ? (-2.0f / m11) : 1.0f;
}

void StatefulVectorRenderer::Redraw(const RenderContext& ctx)
{
    // DEBUG / SIMPLICITY: always rebuild so EntityBook mutations (colors) show immediately.
    dirty = true; // <-- ADDED

    RebuildBatchesIfDirty();

    // World pass uses Application model/view/projection
    worldPass.DrawStatic(ctx);

    // HUD pass: identity view/model + Y-up ortho so Hershey text is upright.
    float w = 1.0f, h = 1.0f;
    ExtractViewportWH_FromOrthoYDown(ctx.projection, w, h);

    RenderContext hudCtx = ctx;
    hudCtx.model = glm::mat4(1.0f);
    hudCtx.view = glm::mat4(1.0f);
    hudCtx.projection = glm::ortho(0.0f, w, 0.0f, h, -1.0f, 1.0f);

    hudPass.DrawStatic(hudCtx);
}

