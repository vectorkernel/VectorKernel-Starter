#include "RenderLoopRenderer.h"
#include "EntityType.h"
#include "HersheyTextBuilder.h"

void RenderLoopRenderer::Init()
{
    linePass.Init();
}

void RenderLoopRenderer::BeginFrame()
{
    linePass.BeginFrame();
}

void RenderLoopRenderer::Submit(const Entity& e)
{
    switch (e.type)
    {
    case EntityType::Line:
        linePass.Submit(e.line);
        break;

    case EntityType::Text:
    {
        std::vector<LineEntity> textLines;
        textLines.reserve(e.text.text.size() * 8);
        HersheyTextBuilder::BuildLines(e.text, textLines);

        for (const auto& l : textLines)
            linePass.Submit(l);
    }
    break;

    default:
        break;
    }
}

void RenderLoopRenderer::Draw(const RenderContext& ctx)
{
    linePass.DrawImmediate(ctx);
}

