// Application.cpp
#include "Application.h"
#include "DragonCurve.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <random>

#include <glm/gtc/matrix_transform.hpp>

#include "hersheyfont.h"

// NOTE: Your project uses the C-style hershey font handle.
static hershey_font* g_hersheyFont = nullptr;

// ------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------
static Entity* FindEntityById(EntityBook& book, std::size_t id)
{
    auto& ents = book.GetEntitiesMutable();
    for (auto& e : ents)
        if (e.id == id)
            return &e;
    return nullptr;
}

static Entity MakeLine(uint32_t id,
    EntityTag tag,
    int drawOrder,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec4& color,
    float thickness,
    bool screenSpace)
{
    Entity e;
    e.id = id;
    e.tag = tag;
    e.type = EntityType::Line;
    e.drawOrder = drawOrder;
    e.screenSpace = screenSpace;

    e.line.start = a;
    e.line.end = b;
    e.line.color = color;
    e.line.thickness = thickness;
    return e;
}

static Entity MakeText(uint32_t id,
    EntityTag tag,
    int drawOrder,
    const std::string& text,
    const glm::vec3& pos,
    float boxW,
    float boxH,
    bool wrap,
    TextHAlign align,
    float scale,
    const glm::vec4& color,
    float strokeWidth,
    bool screenSpace)
{
    Entity e;
    e.id = id;
    e.tag = tag;
    e.type = EntityType::Text;
    e.drawOrder = drawOrder;
    e.screenSpace = screenSpace;

    e.text.text = text;
    e.text.position = pos;
    e.text.boxWidth = boxW;
    e.text.boxHeight = boxH;
    e.text.wordWrapEnabled = wrap;
    e.text.hAlign = align;
    e.text.scale = scale;
    e.text.color = color;
    e.text.strokeWidth = strokeWidth;

    // Critical: project expects the raw hershey_font* handle here.
    e.text.font = g_hersheyFont;

    return e;
}

// ------------------------------------------------------------

Application::Application()
{
}

void Application::Init(int windowWidth, int windowHeight)
{
    clientWidth = std::max(1, windowWidth);
    clientHeight = std::max(1, windowHeight);

    // Load font once (if available).
    if (!g_hersheyFont)
    {
        // hersheyfont.h provides hershey_font_load(fontname) which loads:
        //   %HERSHEY_FONTS_DIR%/<fontname>.jhf
        // and defaults to C:\ProgramData\hershey-fonts if the env var isn't set.
        // Example names: "futural", "rowmans", etc.
        g_hersheyFont = hershey_font_load("futural");

#if _DEBUG
    std::printf("[Pick LMB Down] mouseClient=(%d,%d) mouseWorld=(%.3f,%.3f)\n", mouseClient.x, mouseClient.y, mouseWorld.x, mouseWorld.y);
#endif
    }

    // Start with a default zoom.
    zoom = 1.0f;

    // Center WORLD origin (0,0) in the client window.
    // world = client/zoom + panPixels
    // want world(0,0) at client(center) => panPixels = -center/zoom
    const float invZoom = 1.0f / std::max(0.0001f, zoom);
    panPixels = glm::vec2(
        -(clientWidth * 0.5f) * invZoom,
        -(clientHeight * 0.5f) * invZoom);

    MarkAllDirty();
    EnsureCursorEntities();
    OnResize(clientWidth, clientHeight);
}



void Application::UpdateCameraMatrices()
{
    // world->client: Scale(zoom) * Translate(-panPixels)
    view = glm::mat4(1.0f);
    view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));
    view = glm::translate(view, glm::vec3(-panPixels, 0.0f));
}


void Application::OnResize(int w, int h)
{
    clientWidth = std::max(1, w);
    clientHeight = std::max(1, h);

    // Ortho in client pixel units, origin top-left, Y down.
    projection = glm::ortho(0.0f, (float)clientWidth, (float)clientHeight, 0.0f, -1.0f, 1.0f);

    // Apply current pan/zoom to the view matrix (instead of forcing identity).
    UpdateCameraMatrices();

    model = glm::mat4(1.0f);

    MarkAllDirty();
}


void Application::MarkAllDirty()
{
    dirtyScene = true;
    dirtyPickTree = true;
}

void Application::Update(float /*deltaTime*/)
{
    OnMouseMove();

    if (dirtyScene)
    {
        // Remove old grid/scene, rebuild.
        entityBook.RemoveIf([](const Entity& e)
            {
                return e.tag == EntityTag::Grid || e.tag == EntityTag::Scene || e.tag == EntityTag::Hud;
            });

        RebuildGrid();
        RebuildScene();

        // Cursor entities stay; make sure ordering is consistent.
        entityBook.SortByDrawOrder();

        dirtyScene = false;
        dirtyPickTree = true;
    }

    // Cursor overlay updated every frame (box always, crosshair only when selection inactive).
    EnsureCursorEntities();
    UpdateCursorEntities();

    // Hover only when selection mode is active and we are NOT doing a marquee drag.
    if (selectionMode && !marqueeActive)
    {
        EnsurePickTree();
        UpdateHover();
    }
    else
    {
        ClearHover();
    }
}

void Application::OnMouseMove()
{
    mouseWorld = ClientToWorld(mouseClient);
}

glm::vec2 Application::ClientToWorld(const glm::ivec2& c) const
{
    // World is "pixel-like" with pan/zoom applied.
    // client -> world = (client / zoom) + panPixels
    const float fx = static_cast<float>(c.x);
    const float fy = static_cast<float>(c.y);
    return glm::vec2(fx / std::max(0.0001f, zoom), fy / std::max(0.0001f, zoom)) + panPixels;
}

glm::vec2 Application::WorldToClient(const glm::vec2& w) const
{
    // world -> client = (world - panPixels) * zoom
    const float safeZoom = std::max(0.0001f, zoom);
    return (w - panPixels) * safeZoom;
}


// ------------------------------------------------------------
// Right-mouse panning
// ------------------------------------------------------------
void Application::BeginMousePan(int clientX, int clientY)
{
    mousePanning = true;
    mousePanLastClient = { clientX, clientY };
}

void Application::UpdateMousePan(int clientX, int clientY)
{
    if (!mousePanning)
        return;

    const glm::ivec2 now(clientX, clientY);
    const glm::ivec2 delta = now - mousePanLastClient;
    mousePanLastClient = now;

    // Convert client pixel delta into world delta.
    const float invZoom = 1.0f / std::max(0.0001f, zoom);
    panPixels -= glm::vec2(delta.x * invZoom, delta.y * invZoom);

    UpdateCameraMatrices();
    MarkAllDirty();
}


void Application::EndMousePan()
{
    mousePanning = false;
}

// ------------------------------------------------------------
// Zoom/pan helpers
// ------------------------------------------------------------
void Application::ZoomAtClient(int cx, int cy, float zoomFactor)
{
    const float oldZoom = zoom;
    zoom = std::clamp(zoom * zoomFactor, 0.02f, 200.0f);

    // Keep the point under the cursor stable while zooming:
    const glm::vec2 client((float)cx, (float)cy);
    const glm::vec2 worldUnder = client / std::max(0.0001f, oldZoom) + panPixels;
    panPixels = worldUnder - client / std::max(0.0001f, zoom);

    UpdateCameraMatrices();
    MarkAllDirty();
}


void Application::PanByPixels(int dx, int dy)
{
    const float invZoom = 1.0f / std::max(0.0001f, zoom);
    panPixels += glm::vec2(dx * invZoom, dy * invZoom);

    UpdateCameraMatrices();
    MarkAllDirty();
}


// ------------------------------------------------------------
// Modes
// ------------------------------------------------------------
void Application::ToggleSelectionMode()
{
    selectionMode = !selectionMode;

    // When leaving selection mode, clear hover highlight (crosshair will return).
    if (!selectionMode)
        ClearHover();

    MarkAllDirty();
}

void Application::ToggleGrid()
{
    gridEnabled = !gridEnabled;
    MarkAllDirty();
}

void Application::ToggleWipeout()
{
    wipeoutEnabled = !wipeoutEnabled;
    MarkAllDirty();
}

// ------------------------------------------------------------
// Picking / selection
// ------------------------------------------------------------
void Application::OnLeftClick(HWND /*hwnd*/)
{
    // Backwards-compat wrapper (existing call sites).
    // This is treated as "LMB down".
    OnLeftDown(nullptr);
}

void Application::OnLeftClick()
{
    OnLeftDown(nullptr);
}

void Application::OnLeftDown(HWND /*hwnd*/)
{
    if (!selectionMode)
        return;

    EnsurePickTree();

    // Picker square size is defined in client pixels, converted to world units.
    const float halfSize = (0.5f * static_cast<float>(SELECTION_BOX_SIZE_PX)) / std::max(0.0001f, zoom);
    const BoundingBox box(
        mouseWorld.x - halfSize, mouseWorld.y - halfSize, -1.0f,
        mouseWorld.x + halfSize, mouseWorld.y + halfSize, 1.0f);

#if _DEBUG
    std::printf("[Pick LMB Down] mouseClient=(%d,%d) mouseWorld=(%.3f,%.3f)\n", mouseClient.x, mouseClient.y, mouseWorld.x, mouseWorld.y);
#endif

    const auto hit = pickTree.QueryFirstIntersect(box);
    if (hit.has_value())
    {
        // Single entity select
        const std::size_t idx = *hit;
        ApplySelection(std::vector<std::size_t>{ idx });

        // Prevent hover from immediately undoing selection
        if (hoveredIndex.has_value() && *hoveredIndex == idx)
            hoveredIndex.reset();

        return;
    }

    // No hit => start a marquee selection rectangle.
    BeginMarquee();
}

void Application::OnLeftUp(HWND /*hwnd*/)
{
    if (!marqueeActive)
        return;

    FinishMarqueeSelect();
}

void Application::UpdateMarqueeDrag(int clientX, int clientY)
{
    if (!marqueeActive)
        return;

    marqueeEndClient = { clientX, clientY };
    marqueeEndWorld = ClientToWorld(marqueeEndClient);
}

void Application::ClearSelection()
{
    auto& ents = entityBook.GetEntitiesMutable();

    for (const auto& kv : selectedPrevColors)
    {
        const std::size_t idx = kv.first;
        if (idx < ents.size() && ents[idx].type == EntityType::Line)
            ents[idx].line.color = kv.second;
    }

    selectedPrevColors.clear();
    selectedIndices.clear();
    selectedIndex.reset();
}

void Application::ApplySelection(const std::vector<std::size_t>& indices)
{
    ClearSelection();

    auto& ents = entityBook.GetEntitiesMutable();
    selectedIndices = indices;
    if (!selectedIndices.empty())
        selectedIndex = selectedIndices.front();

    for (std::size_t idx : selectedIndices)
    {
        if (idx >= ents.size())
            continue;
        Entity& e = ents[idx];
        if (e.type != EntityType::Line)
            continue;

        selectedPrevColors[idx] = e.line.color;
        e.line.color = glm::vec4(1, 1, 1, 1); // SELECTED = white
    }
}

void Application::BeginMarquee()
{
    marqueeActive = true;
    marqueeStartClient = mouseClient;
    marqueeEndClient = mouseClient;

    marqueeStartWorld = mouseWorld;
    marqueeEndWorld = mouseWorld;
}

void Application::FinishMarqueeSelect()
{
    // Determine drag direction (right = bounds/inside, left = crossing).
    const int dx = marqueeEndClient.x - marqueeStartClient.x;
    const int dy = marqueeEndClient.y - marqueeStartClient.y;

    // Tiny drags just cancel.
    if (std::abs(dx) < 2 && std::abs(dy) < 2)
    {
        marqueeActive = false;
        return;
    }

    const bool crossing = (dx < 0); // drag-left = crossing
    const glm::vec2 a = marqueeStartWorld;
    const glm::vec2 b = marqueeEndWorld;

    const float minX = std::min(a.x, b.x);
    const float maxX = std::max(a.x, b.x);
    const float minY = std::min(a.y, b.y);
    const float maxY = std::max(a.y, b.y);

    std::vector<std::size_t> hits;
    const auto& ents = entityBook.GetEntities();

    for (std::size_t i = 0; i < ents.size(); ++i)
    {
        const Entity& e = ents[i];
        if (e.tag != EntityTag::Scene)
            continue;
        if (e.type != EntityType::Line)
            continue;

        const glm::vec3 p0 = e.line.start;
        const glm::vec3 p1 = e.line.end;

        const float eMinX = std::min(p0.x, p1.x);
        const float eMaxX = std::max(p0.x, p1.x);
        const float eMinY = std::min(p0.y, p1.y);
        const float eMaxY = std::max(p0.y, p1.y);

        if (crossing)
        {
            const bool intersects =
                !(eMaxX < minX || eMinX > maxX || eMaxY < minY || eMinY > maxY);

            if (intersects)
                hits.push_back(i);
        }
        else
        {
            const bool inside =
                (eMinX >= minX && eMaxX <= maxX && eMinY >= minY && eMaxY <= maxY);

            if (inside)
                hits.push_back(i);
        }
    }

    ApplySelection(hits);

    marqueeActive = false;
}

void Application::UpdateMarqueeOverlay()
{
    // Hide marquee when not active by collapsing it to cursor center.
    if (!marqueeActive)
    {
        const float cx = static_cast<float>(mouseClient.x);
        const float cy = static_cast<float>(clientHeight - 1 - mouseClient.y);
        const glm::vec3 c(cx, cy, 0.0f);

        for (int i = 0; i < 4; ++i)
        {
            if (auto* l = FindEntityById(entityBook, marqueeBoxId[i]))
            {
                l->screenSpace = true;
                l->line.start = c;
                l->line.end = c;
            }
        }
        return;
    }

    // Use client coords, but convert to renderer's Y-up screen space.
    const float sx = static_cast<float>(marqueeStartClient.x);
    const float sy = static_cast<float>(clientHeight - 1 - marqueeStartClient.y);
    const float ex = static_cast<float>(marqueeEndClient.x);
    const float ey = static_cast<float>(clientHeight - 1 - marqueeEndClient.y);

    const float x0 = std::min(sx, ex);
    const float x1 = std::max(sx, ex);
    const float y0 = std::min(sy, ey);
    const float y1 = std::max(sy, ey);

    const glm::vec3 r00(x0, y0, 0.0f);
    const glm::vec3 r10(x1, y0, 0.0f);
    const glm::vec3 r11(x1, y1, 0.0f);
    const glm::vec3 r01(x0, y1, 0.0f);

    if (auto* b0 = FindEntityById(entityBook, marqueeBoxId[0])) { b0->screenSpace = true; b0->line.start = r00; b0->line.end = r10; }
    if (auto* b1 = FindEntityById(entityBook, marqueeBoxId[1])) { b1->screenSpace = true; b1->line.start = r10; b1->line.end = r11; }
    if (auto* b2 = FindEntityById(entityBook, marqueeBoxId[2])) { b2->screenSpace = true; b2->line.start = r11; b2->line.end = r01; }
    if (auto* b3 = FindEntityById(entityBook, marqueeBoxId[3])) { b3->screenSpace = true; b3->line.start = r01; b3->line.end = r00; }
}

void Application::EnsurePickTree()
{
    if (!dirtyPickTree)
        return;

    BuildPickTree();
    dirtyPickTree = false;
}

void Application::BuildPickTree()
{
    pickTree.Clear();

    const auto& ents = entityBook.GetEntities();
    std::vector<std::pair<BoundingBox, std::size_t>> items;
    items.reserve(ents.size());

    // Small pad so thin lines are still hittable. Uses selection box size.
    const float pad = (0.5f * static_cast<float>(SELECTION_BOX_SIZE_PX)) / std::max(0.0001f, zoom);

    for (std::size_t i = 0; i < ents.size(); ++i)
    {
        const Entity& e = ents[i];
        if (e.tag != EntityTag::Scene)
            continue;

        if (e.type != EntityType::Line)
            continue;

        const glm::vec3 a = e.line.start;
        const glm::vec3 b = e.line.end;

        const float minX = std::min(a.x, b.x) - pad;
        const float minY = std::min(a.y, b.y) - pad;
        const float maxX = std::max(a.x, b.x) + pad;
        const float maxY = std::max(a.y, b.y) + pad;

        items.emplace_back(BoundingBox(minX, minY, -1.0f, maxX, maxY, 1.0f), i);
    }

    if (!items.empty())
        pickTree.Build(items);
}

void Application::ClearHover()
{
    if (!hoveredIndex.has_value())
        return;

    const std::size_t idx = *hoveredIndex;

    // Don't restore color if this entity is currently selected.
    if (selectedPrevColors.find(idx) != selectedPrevColors.end())
    {
        hoveredIndex.reset();
        return;
    }

    auto& ents = entityBook.GetEntitiesMutable();
    if (idx < ents.size() && ents[idx].type == EntityType::Line)
        ents[idx].line.color = hoveredPrevColor;

    hoveredIndex.reset();
}

void Application::UpdateHover()
{
    // Clear previous hover (restores color).
    ClearHover();

    // Query a small box around mouse, sized from SELECTION_BOX_SIZE_PX.
    const float halfSize = (0.5f * static_cast<float>(SELECTION_BOX_SIZE_PX)) / std::max(0.0001f, zoom);
    const BoundingBox box(
        mouseWorld.x - halfSize, mouseWorld.y - halfSize, -1.0f,
        mouseWorld.x + halfSize, mouseWorld.y + halfSize, 1.0f);

    const auto hit = pickTree.QueryFirstIntersect(box);
    if (!hit.has_value())
        return;

    const std::size_t idx = *hit;

    // If this is selected, don't treat it as hover-highlight.
    if (selectedPrevColors.find(idx) != selectedPrevColors.end())
        return;

    auto& ents = entityBook.GetEntitiesMutable();
    if (idx >= ents.size())
        return;

    Entity& e = ents[idx];
    if (e.type != EntityType::Line)
        return;

    hoveredPrevColor = e.line.color;
    e.line.color = glm::vec4(1, 1, 1, 1); // hover highlight (white)
    hoveredIndex = idx;
}

// ------------------------------------------------------------
// Cursor entities (screen space)
// ------------------------------------------------------------
void Application::EnsureCursorEntities()
{
    if (cursorEntitiesValid)
        return;

    const glm::vec4 white(1, 1, 1, 1);
    const int order = 900;

    // Crosshair: 6 lines (we only use 2, but keep array stable)
    for (int i = 0; i < 6; ++i)
    {
        cursorCrossId[i] = nextId++;
        entityBook.AddEntity(MakeLine(cursorCrossId[i], EntityTag::Cursor, order,
            glm::vec3(0, 0, 0), glm::vec3(0, 0, 0),
            white, 1.0f, true));
    }

    // Box: 4 lines (always drawn; used as selection box OR crosshair center box)
    for (int i = 0; i < 4; ++i)
    {
        cursorBoxId[i] = nextId++;
        entityBook.AddEntity(MakeLine(cursorBoxId[i], EntityTag::Cursor, order,
            glm::vec3(0, 0, 0), glm::vec3(0, 0, 0),
            white, 1.0f, true));
    }

    // Marquee selection rectangle: 4 lines (only shown while dragging)
    for (int i = 0; i < 4; ++i)
    {
        marqueeBoxId[i] = nextId++;
        entityBook.AddEntity(MakeLine(marqueeBoxId[i], EntityTag::Cursor, order,
            glm::vec3(0, 0, 0), glm::vec3(0, 0, 0),
            white, 1.0f, true));
    }

    cursorEntitiesValid = true;

    // Reorders vector => pickTree indices become invalid.
    entityBook.SortByDrawOrder();
    dirtyPickTree = true;
}

void Application::UpdateCursorEntities()
{
    // Cursor overlay is expressed in client-space pixels.
    // Our renderer’s screen-space path expects Y-up (origin bottom-left),
    // while Win32 mouse coords are Y-down (origin top-left), so flip Y here.
    const float cx = static_cast<float>(mouseClient.x);
    const float cy = static_cast<float>(clientHeight - 1 - mouseClient.y);

    // A small box centered at the cursor. Size is defined in client pixels.
    const float boxHalf = 0.5f * static_cast<float>(SELECTION_BOX_SIZE_PX);

    const glm::vec3 c(cx, cy, 0.0f);
    const glm::vec3 b00(cx - boxHalf, cy - boxHalf, 0.0f);
    const glm::vec3 b10(cx + boxHalf, cy - boxHalf, 0.0f);
    const glm::vec3 b11(cx + boxHalf, cy + boxHalf, 0.0f);
    const glm::vec3 b01(cx - boxHalf, cy + boxHalf, 0.0f);

    // Crosshair lines ONLY when selection system is inactive.
    if (!selectionMode)
    {
        const float w = static_cast<float>(clientWidth);
        const float h = static_cast<float>(clientHeight);

        const glm::vec3 h0(0.0f, cy, 0.0f);
        const glm::vec3 h1(w, cy, 0.0f);
        const glm::vec3 v0(cx, 0.0f, 0.0f);
        const glm::vec3 v1(cx, h, 0.0f);

        // Use first two cross entities; collapse the rest.
        if (auto* l0 = FindEntityById(entityBook, cursorCrossId[0])) { l0->screenSpace = true; l0->line.start = h0; l0->line.end = h1; }
        if (auto* l1 = FindEntityById(entityBook, cursorCrossId[1])) { l1->screenSpace = true; l1->line.start = v0; l1->line.end = v1; }

        for (int i = 2; i < 6; ++i)
        {
            if (auto* l = FindEntityById(entityBook, cursorCrossId[i]))
            {
                l->screenSpace = true;
                l->line.start = c;
                l->line.end = c;
            }
        }
    }
    else
    {
        // Selection active: no crosshair lines.
        for (int i = 0; i < 6; ++i)
        {
            if (auto* l = FindEntityById(entityBook, cursorCrossId[i]))
            {
                l->screenSpace = true;
                l->line.start = c;
                l->line.end = c;
            }
        }
    }

    // Center box always exists.
    if (auto* b0 = FindEntityById(entityBook, cursorBoxId[0])) { b0->screenSpace = true; b0->line.start = b00; b0->line.end = b10; }
    if (auto* b1 = FindEntityById(entityBook, cursorBoxId[1])) { b1->screenSpace = true; b1->line.start = b10; b1->line.end = b11; }
    if (auto* b2 = FindEntityById(entityBook, cursorBoxId[2])) { b2->screenSpace = true; b2->line.start = b11; b2->line.end = b01; }
    if (auto* b3 = FindEntityById(entityBook, cursorBoxId[3])) { b3->screenSpace = true; b3->line.start = b01; b3->line.end = b00; }

    // Optional marquee rectangle (screen space)
    UpdateMarqueeOverlay();
}



// ------------------------------------------------------------
// Demo scene/grid (simple, safe defaults)
// ------------------------------------------------------------
void Application::RebuildScene()
{
    const int dragonIterations = 12; // 4096 segments
    const glm::vec3 dragonOriginWorld(0.0f, 0.0f, 0.0f); // TRUE world origin

    // Deterministic random colors (stable between rebuilds)
    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> dist(0.15f, 1.0f);
    auto RandColor = [&]()
        {
            return glm::vec4(dist(rng), dist(rng), dist(rng), 1.0f);
        };

    DragonCurve curve;
    const auto segs = curve.Build(dragonIterations, dragonOriginWorld);

    const int drawOrder = 100;
    const float thickness = 2.0f;

    for (const auto& s : segs)
    {
        entityBook.AddEntity(MakeLine(
            nextId++,
            EntityTag::Scene,
            drawOrder,
            s.a,
            s.b,
            RandColor(),
            thickness,
            false)); // false = not HUD → world space
    }

    // HUD text (unchanged)
    entityBook.AddEntity(MakeText(nextId++, EntityTag::Hud, 950,
        selectionMode ? "Selection: ON (LMB pick)" : "Selection: OFF (Crosshair)",
        glm::vec3(16, 24, 0),
        900, 40,
        false,
        TextHAlign::Left,
        1.0f,
        glm::vec4(1, 1, 1, 1),
        1.0f,
        true));
}



void Application::RebuildGrid()
{
    if (!gridEnabled)
        return;

    // World-space grid (zooms & pans with camera)

    // Standard grid colors
    const glm::vec4 minor(0.22f, 0.22f, 0.22f, 1.0f);
    const glm::vec4 major(0.32f, 0.32f, 0.32f, 1.0f);

    // Origin axes (faded)
    const glm::vec4 xAxisColor(0.2f, 0.8f, 0.2f, 1.0f); // X == 0 → green
    const glm::vec4 yAxisColor(0.8f, 0.2f, 0.2f, 1.0f); // Y == 0 → red

    const int minorStep = 25;
    const int majorStep = 100;

    const float safeZoom = std::max(0.0001f, zoom);
    const float invZoom = 1.0f / safeZoom;

    // Visible world rect
    const float worldL = panPixels.x;
    const float worldT = panPixels.y;
    const float worldR = panPixels.x + (float)clientWidth * invZoom;
    const float worldB = panPixels.y + (float)clientHeight * invZoom;

    // Overscan to avoid edge gaps
    const float overscanScreens = 1.0f;
    const float padX = (float)clientWidth * invZoom * overscanScreens;
    const float padY = (float)clientHeight * invZoom * overscanScreens;

    const float L = worldL - padX;
    const float R = worldR + padX;
    const float T = worldT - padY;
    const float B = worldB + padY;

    auto floorToStep = [](float v, int step) -> int
        {
            return (int)std::floor(v / (float)step) * step;
        };
    auto ceilToStep = [](float v, int step) -> int
        {
            return (int)std::ceil(v / (float)step) * step;
        };

    const int x0 = floorToStep(L, minorStep);
    const int x1 = ceilToStep(R, minorStep);
    const int y0 = floorToStep(T, minorStep);
    const int y1 = ceilToStep(B, minorStep);

    // ------------------------------------------------------------
    // Vertical grid lines (constant X)
    // ------------------------------------------------------------
    for (int x = x0; x <= x1; x += minorStep)
    {
        glm::vec4 color = minor;

        if (x == 0)
        {
            color = xAxisColor;   // X axis
        }
        else if ((x % majorStep) == 0)
        {
            color = major;
        }

        entityBook.AddEntity(MakeLine(nextId++, EntityTag::Grid, 0,
            glm::vec3((float)x, (float)y0, 0.0f),
            glm::vec3((float)x, (float)y1, 0.0f),
            color, 1.5f, false));
    }

    // ------------------------------------------------------------
    // Horizontal grid lines (constant Y)
    // ------------------------------------------------------------
    for (int y = y0; y <= y1; y += minorStep)
    {
        glm::vec4 color = minor;

        if (y == 0)
        {
            color = yAxisColor;   // Y axis
        }
        else if ((y % majorStep) == 0)
        {
            color = major;
        }

        entityBook.AddEntity(MakeLine(nextId++, EntityTag::Grid, 0,
            glm::vec3((float)x0, (float)y, 0.0f),
            glm::vec3((float)x1, (float)y, 0.0f),
            color, 1.5f, false));
    }

    (void)wipeoutEnabled;
}







