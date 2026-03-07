#pragma once
// ============================================================
// buildings.h  —  Building generator for ascworld city theme
// ============================================================
// Generates complete buildings (exterior shells + optional
// explorable interiors) from a footprint, height, and palette.
//
// Building types by district:
//   Downtown    — skyscrapers, 6-20 floors, lobby + top floor interior
//   Tech        — labs, 3-6 floors, full interiors (exploration targets)
//   Industrial  — warehouses, 1-3 floors, large open interiors
//   Slums       — shanties, 1-4 floors, mix of open/closed
//   Residential — houses/apts, 2-4 floors, some interiors
//   Wasteland   — ruins, 0-2 floors, just broken walls
// ============================================================

#include "core/vec_math.h"
#include "rendering/mesh.h"
#include "rendering/renderer.h"
#include "rendering/framebuffer.h"
#include "game/player.h"
#include "world/prefabs/city_materials.h"
#include <vector>
#include <cstdint>

enum class DistrictType {
    Downtown, Tech, Industrial, Slums, Residential, Wasteland
};

// Building shape variations for visual diversity
enum class BuildingShape {
    Box,          // Standard rectangular prism (default)
    Setback,      // Upper floors narrower than base (skyscraper style)
    Tiered,       // 2-3 progressively narrower tiers
    Tower,        // Narrow tall tower on a wide base podium
    Wedge,        // One side taller than the other (sloped roofline)
};

struct BuildingSpec {
    float x, z;           // world position (corner)
    float width, depth;   // footprint
    int floors;           // number of floors
    bool hasInterior;     // whether to generate interior rooms
    DistrictType district;
    BuildingShape shape = BuildingShape::Box;
};

// Generate a complete building at the given spec
void generateBuilding(
    const BuildingSpec& spec,
    const DistrictPalette& pal,
    uint32_t seed,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders,
    std::vector<SlopeCollider>& slopeColliders
);
