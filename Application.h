// Application.h
#pragma once

#include <glm/glm.hpp>

#include "EntityBook.h"
#include "RGeometryTree.h" // BoundingBox + RGeometryTree

#include <optional>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

// -----------------------------------------------------------------------------
// UI tuning (client-space pixels)
// -----------------------------------------------------------------------------
// Size (in client pixels) of the selection/pick box drawn at the cursor.
// This is also used as the hover/pick query box size (converted to world units).
#ifndef SELECTION_BOX_SIZE_PX
#define SELECTION_BOX_SIZE_PX 12
#endif

class Application
{
public:
    Application();

    void Init(int windowWidth, int windowHeight);
    void OnResize(int w, int h);
    void Update(float deltaTime);

    // Matrices (world)
    const glm::mat4& GetProjectionMatrix() const { return projection; }
    const glm::mat4& GetViewMatrix() const { return view; }
    const glm::mat4& GetModelMatrix() const { return model; }

    // Input
    void SetMouseClient(int x, int y) { mouseClient = { x, y }; }

    // Right-button drag panning
    void BeginMousePan(int clientX, int clientY);
    void UpdateMousePan(int clientX, int clientY);
    void EndMousePan();
    bool IsMousePanning() const { return mousePanning; }

    // Camera controls
    void ZoomAtClient(int cx, int cy, float zoomFactor);
    void PanByPixels(int dx, int dy);

    // Modes
    void ToggleSelectionMode();
    void ToggleGrid();
    void ToggleWipeout();

    // Click handlers
    void OnLeftClick();
    void OnLeftClick(HWND hwnd);

    // New: marquee (rectangle) selection when click hits empty space.
    void OnLeftDown(HWND hwnd);
    void OnLeftUp(HWND hwnd);
    void UpdateMarqueeDrag(int clientX, int clientY);
    bool IsMarqueeSelecting() const { return marqueeActive; }

    EntityBook& GetEntityBook() { return entityBook; }

private:
    // Scene lifecycle
    void MarkAllDirty();
    void RebuildScene();
    void RebuildGrid();

    // Cursor overlay
    void EnsureCursorEntities();
    void UpdateCursorEntities();

    // Picking / hover
    void EnsurePickTree();
    void BuildPickTree();
    void UpdateHover();
    void ClearHover();

// Selection helpers
void ClearSelection();
void ApplySelection(const std::vector<std::size_t>& indices);

// Marquee selection
void BeginMarquee();
void FinishMarqueeSelect();
void UpdateMarqueeOverlay();

glm::vec2 WorldToClient(const glm::vec2& world) const;

    // Mouse helpers
    void OnMouseMove();
    glm::vec2 ClientToWorld(const glm::ivec2& client) const;
    glm::vec2 ClientToWorld(int cx, int cy) const { return ClientToWorld(glm::ivec2(cx, cy)); }

private:
    EntityBook entityBook{};

    int clientWidth = 1;
    int clientHeight = 1;

    // Camera
    glm::vec2 panPixels{ 0.0f, 0.0f };
    float zoom = 1.0f;


    void UpdateCameraMatrices();
    // Input state
    glm::ivec2 mouseClient{ 0, 0 };
    glm::vec2 mouseWorld{ 0.0f, 0.0f };

    // Right-button drag panning state
    bool mousePanning = false;
    glm::ivec2 mousePanLastClient{ 0, 0 };

    // Marquee drag (LMB in selection mode when click hits empty space)
    bool marqueeActive = false;
    glm::ivec2 marqueeStartClient{ 0,0 };
    glm::ivec2 marqueeEndClient{ 0,0 };
    glm::vec2 marqueeStartWorld{ 0.0f,0.0f };
    glm::vec2 marqueeEndWorld{ 0.0f,0.0f };

    // Selection
    // Multi-selection support for marquee.
    std::vector<std::size_t> selectedIndices;
    std::unordered_map<std::size_t, glm::vec4> selectedPrevColors;
    std::optional<std::size_t> selectedIndex; // kept for convenience (first selected)

    // Modes
    bool selectionMode = false;
    bool gridEnabled = true;
    bool wipeoutEnabled = true;

    // Dirty flags
    bool dirtyScene = true;
    bool dirtyPickTree = true;

    // Picking structure
    RGeometryTree pickTree;

    std::optional<std::size_t> hoveredIndex;
    glm::vec4 hoveredPrevColor{ 1,1,1,1 };

    // Cursor entity ids (screen space)
    bool cursorEntitiesValid = false;
    uint32_t cursorCrossId[6]{ 0,0,0,0,0,0 };
    uint32_t cursorBoxId[4]{ 0,0,0,0 };
    uint32_t marqueeBoxId[4]{ 0,0,0,0 };

    // Entity IDs
    uint32_t nextId = 1;

    // Matrices
    glm::mat4 projection{ 1.0f };
    glm::mat4 view{ 1.0f };
    glm::mat4 model{ 1.0f };
};





