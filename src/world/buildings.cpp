// ============================================================
// buildings.cpp  —  Building generator implementation
// ============================================================
// Produces exterior shell, windows, doors, optional interiors
// (rooms, stairs, furniture, lights), rooftop details, and
// collision geometry for a single building.
// ============================================================

#include "world/buildings.h"

// Structural prefabs
#include "world/prefabs/structural/staircase.h"
#include "world/prefabs/structural/door_frame.h"

// Interior prefabs
#include "world/prefabs/interior/table.h"
#include "world/prefabs/interior/shelf_rack.h"
#include "world/prefabs/interior/terminal.h"
#include "world/prefabs/interior/pillar.h"
#include "world/prefabs/interior/debris.h"

// Street props used in interiors
#include "world/prefabs/street_props/fire_barrel.h"

// Vehicles
#include "world/prefabs/vehicles/crate.h"

// Building-mounted
#include "world/prefabs/building_mounted/antenna.h"
#include "world/prefabs/building_mounted/pipe_run.h"
#include "world/prefabs/building_mounted/sign_board.h"

#include <cmath>
#include <algorithm>

// ── Constants ──────────────────────────────────────────────────

static constexpr float FLOOR_HEIGHT   = 3.0f;
static constexpr float WALL_THICKNESS = 0.3f;
static constexpr float DOOR_WIDTH     = 1.6f;
static constexpr float DOOR_HEIGHT    = 2.4f;
static constexpr float WINDOW_WIDTH   = 0.8f;
static constexpr float WINDOW_HEIGHT  = 1.2f;
static constexpr float WINDOW_SPACING = 2.0f;   // horizontal interval
static constexpr float WINDOW_DEPTH   = 0.05f;  // thin surface patch

// ── Deterministic RNG ──────────────────────────────────────────

static uint32_t buildingRng(uint32_t& s) {
    s = s * 1103515245u + 12345u;
    return (s >> 16) & 0x7FFF;
}

static float buildingRngFloat(uint32_t& s) {
    return buildingRng(s) / 32767.0f;
}

// ── Helper: push a scaled cube ─────────────────────────────────
// Creates a unit cube mesh, pushes it into meshStorage, then adds
// a SceneObject with translate(center) * scale(size).

static void pushBox(
    const Vec3& center,
    const Vec3& size,
    const Material& mat,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects)
{
    Mat4 xf = Mat4::translate(center) * Mat4::scale(size);
    objects.push_back(SceneObject{meshCache.cube(), xf, mat});
}

// ── Helper: push an AABB from center + half-extents ────────────

static void pushAABB(
    const Vec3& center,
    const Vec3& halfSize,
    std::vector<AABB>& colliders)
{
    colliders.push_back(AABB(center - halfSize, center + halfSize));
}

// ── Helper: push a wall slab (visual + collider) ───────────────

static void pushWall(
    const Vec3& center,
    const Vec3& size,
    const Material& mat,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<AABB>& colliders)
{
    pushBox(center, size, mat, meshCache, objects);
    Vec3 half = size * 0.5f;
    pushAABB(center, half, colliders);
}

// ================================================================
//  generateBuilding — main entry point
// ================================================================

void generateBuilding(
    const BuildingSpec& spec,
    const DistrictPalette& pal,
    uint32_t seed,
    MeshCache& meshCache,
    std::vector<SceneObject>& objects,
    std::vector<PointLight>& lights,
    std::vector<AABB>& colliders,
    std::vector<SlopeCollider>& slopeColliders)
{
    // Derive RNG state from seed + position
    uint32_t rng = seed ^ (uint32_t)(spec.x * 7919.0f)
                        ^ (uint32_t)(spec.z * 6271.0f);
    buildingRng(rng); // warm up

    const float W  = spec.width;
    const float D  = spec.depth;
    const float H  = spec.floors * FLOOR_HEIGHT;
    const float gY = 0.0f; // ground level

    // Corner of the building (min X, min Z)
    const float bx = spec.x;
    const float bz = spec.z;

    // Centre of footprint
    const float cx = bx + W * 0.5f;
    const float cz = bz + D * 0.5f;

    // ────────────────────────────────────────────────────────────
    //  1. Exterior shell (solid building or with interior)
    // ────────────────────────────────────────────────────────────
    //
    //  Building shapes are implemented via a "tier" system.
    //  Each tier is a rectangular volume with its own footprint
    //  (x, z, width, depth) and floor range.  A Box building
    //  is a single tier; Setback/Tiered/Tower/Wedge produce
    //  multiple tiers or special geometry.

    // Choose exterior door face and position early so we can cut
    // the wall opening in the hasInterior path.
    // 0=front(−Z), 1=back(+Z), 2=left(−X), 3=right(+X)
    int doorFace = buildingRng(rng) % 4;

    // ── Stairwell bounds (pre-computed for floor slab cutouts) ──
    // These mirror the values computed in the interior section so
    // the tier loop can leave openings in floor slabs.
    const float iMinX_pre = bx + WALL_THICKNESS;
    const float iMaxX_pre = bx + W - WALL_THICKNESS;
    const float iMinZ_pre = bz + WALL_THICKNESS;
    const float iMaxZ_pre = bz + D - WALL_THICKNESS;
    const float swW     = 1.2f;                            // stairwell width
    const float swD     = 2.5f;                            // stairwell depth
    const float swX     = iMaxX_pre - swW - 0.2f;         // stairwell center X
    const float swZ     = iMaxZ_pre - swD - 0.2f;         // stairwell start Z
    const float swMinX  = swX - swW * 0.5f;
    const float swMaxX  = swX + swW * 0.5f;
    const float swMinZ  = swZ;
    const float swMaxZ  = swZ + swD;
    // Only cut stairwell openings if building has interior + >1 floor
    const bool  hasStairwell = spec.hasInterior && spec.floors > 1;

    // ── Tier definition ─────────────────────────────────────────
    struct Tier {
        float tx, tz, tw, td;   // corner (x,z) + width, depth
        int   startFloor;       // inclusive
        int   endFloor;         // exclusive (first floor NOT in tier)
    };

    // Build the tier list based on shape
    std::vector<Tier> tiers;
    bool hasWedgeTop = false;       // true for Wedge shape (sloped roof)
    int  wedgeTallSide = 0;         // 0=front(-Z), 1=back(+Z), 2=left(-X), 3=right(+X)
    int  wedgeExtraFloors = 0;      // extra floors on the tall side

    switch (spec.shape) {
    default:
    case BuildingShape::Box:
        // Single tier covering all floors
        tiers.push_back({bx, bz, W, D, 0, spec.floors});
        break;

    case BuildingShape::Setback: {
        // Base floors at full footprint, upper floors narrower.
        // Setback happens at ~40-60% of total floors.
        int setbackFloor = std::max(2, (int)(spec.floors * (0.4f + buildingRngFloat(rng) * 0.2f)));
        float insetX = W * (0.1f + buildingRngFloat(rng) * 0.1f);  // 10-20% narrower per side
        float insetZ = D * (0.1f + buildingRngFloat(rng) * 0.1f);

        tiers.push_back({bx, bz, W, D, 0, setbackFloor});
        tiers.push_back({bx + insetX, bz + insetZ,
                         W - 2.0f * insetX, D - 2.0f * insetZ,
                         setbackFloor, spec.floors});
        break;
    }

    case BuildingShape::Tiered: {
        // 2-3 tiers, each progressively narrower
        int numTiers = 2 + (buildingRng(rng) % 2);  // 2 or 3
        float curX = bx, curZ = bz, curW = W, curD = D;
        int floorsPerTier = std::max(1, spec.floors / numTiers);
        int floorsUsed = 0;

        for (int t = 0; t < numTiers; ++t) {
            int tierFloors = (t == numTiers - 1) ? (spec.floors - floorsUsed) : floorsPerTier;
            if (tierFloors <= 0) break;

            tiers.push_back({curX, curZ, curW, curD, floorsUsed, floorsUsed + tierFloors});
            floorsUsed += tierFloors;

            // Inset for next tier
            float stepX = curW * (0.12f + buildingRngFloat(rng) * 0.08f);
            float stepZ = curD * (0.12f + buildingRngFloat(rng) * 0.08f);
            curX += stepX;
            curZ += stepZ;
            curW -= 2.0f * stepX;
            curD -= 2.0f * stepZ;
            if (curW < 2.0f || curD < 2.0f) break;
        }
        break;
    }

    case BuildingShape::Tower: {
        // Wide podium base (2-3 floors) + narrow tower on top
        int podiumFloors = std::min(3, std::max(1, spec.floors / 3));
        float towerInsetX = W * (0.2f + buildingRngFloat(rng) * 0.1f);
        float towerInsetZ = D * (0.2f + buildingRngFloat(rng) * 0.1f);

        tiers.push_back({bx, bz, W, D, 0, podiumFloors});
        tiers.push_back({bx + towerInsetX, bz + towerInsetZ,
                         W - 2.0f * towerInsetX, D - 2.0f * towerInsetZ,
                         podiumFloors, spec.floors});
        break;
    }

    case BuildingShape::Wedge: {
        // Full footprint for most floors, but one side is taller
        // The main body goes to (floors - extra), then a half-width
        // extension adds extra floors on one side.
        wedgeExtraFloors = std::max(1, spec.floors / 4);
        int mainFloors = spec.floors - wedgeExtraFloors;
        wedgeTallSide = buildingRng(rng) % 4;
        hasWedgeTop = true;

        tiers.push_back({bx, bz, W, D, 0, mainFloors});

        // The "tall half" extension
        float halfW = W * 0.5f, halfD = D * 0.5f;
        switch (wedgeTallSide) {
        case 0: // front (-Z) side is taller
            tiers.push_back({bx, bz, W, halfD, mainFloors, spec.floors});
            break;
        case 1: // back (+Z) side is taller
            tiers.push_back({bx, bz + halfD, W, halfD, mainFloors, spec.floors});
            break;
        case 2: // left (-X) side is taller
            tiers.push_back({bx, bz, halfW, D, mainFloors, spec.floors});
            break;
        case 3: // right (+X) side is taller
            tiers.push_back({bx + halfW, bz, halfW, D, mainFloors, spec.floors});
            break;
        }
        break;
    }
    }

    // ── Helper: split wall for door opening ─────────────────────
    auto pushSplitWall = [&](
        int wallAxis, float wallFixedCoord,
        float wallSpanStart, float wallSpanLen,
        float doorAlongPos, float floorY)
    {
        float halfDoor = DOOR_WIDTH * 0.5f;
        float doorLeft  = doorAlongPos - halfDoor;
        float doorRight = doorAlongPos + halfDoor;
        float spanEnd   = wallSpanStart + wallSpanLen;
        float wallCY    = floorY + FLOOR_HEIGHT * 0.5f;

        // Left segment (from span start to door left edge)
        float leftW = doorLeft - wallSpanStart;
        if (leftW > 0.01f) {
            float leftCenter = wallSpanStart + leftW * 0.5f;
            if (wallAxis == 0) {
                pushWall(Vec3(leftCenter, wallCY, wallFixedCoord),
                         Vec3(leftW, FLOOR_HEIGHT, WALL_THICKNESS),
                         pal.wall, meshCache, objects, colliders);
            } else {
                pushWall(Vec3(wallFixedCoord, wallCY, leftCenter),
                         Vec3(WALL_THICKNESS, FLOOR_HEIGHT, leftW),
                         pal.wall, meshCache, objects, colliders);
            }
        }

        // Right segment (from door right edge to span end)
        float rightW = spanEnd - doorRight;
        if (rightW > 0.01f) {
            float rightCenter = doorRight + rightW * 0.5f;
            if (wallAxis == 0) {
                pushWall(Vec3(rightCenter, wallCY, wallFixedCoord),
                         Vec3(rightW, FLOOR_HEIGHT, WALL_THICKNESS),
                         pal.wall, meshCache, objects, colliders);
            } else {
                pushWall(Vec3(wallFixedCoord, wallCY, rightCenter),
                         Vec3(WALL_THICKNESS, FLOOR_HEIGHT, rightW),
                         pal.wall, meshCache, objects, colliders);
            }
        }

        // Lintel above door opening
        float lintelH = FLOOR_HEIGHT - DOOR_HEIGHT;
        if (lintelH > 0.01f) {
            float lintelCY = floorY + DOOR_HEIGHT + lintelH * 0.5f;
            if (wallAxis == 0) {
                pushWall(Vec3(doorAlongPos, lintelCY, wallFixedCoord),
                         Vec3(DOOR_WIDTH, lintelH, WALL_THICKNESS),
                         pal.wall, meshCache, objects, colliders);
            } else {
                pushWall(Vec3(wallFixedCoord, lintelCY, doorAlongPos),
                         Vec3(WALL_THICKNESS, lintelH, DOOR_WIDTH),
                         pal.wall, meshCache, objects, colliders);
            }
        }
    };

    // ── Generate walls + slabs per tier ─────────────────────────
    for (size_t ti = 0; ti < tiers.size(); ++ti) {
        const Tier& tier = tiers[ti];
        float tCX = tier.tx + tier.tw * 0.5f;
        float tCZ = tier.tz + tier.td * 0.5f;

        // Door is only on the ground-floor tier (tier containing floor 0)
        // and only the base footprint tier.
        bool tierHasDoor = (ti == 0 && spec.hasInterior);

        if (!spec.hasInterior && ti == 0) {
            // --- Solid tier: four exterior walls, floor, roof ---
            float tierH = (tier.endFloor - tier.startFloor) * FLOOR_HEIGHT;
            float baseY = gY + tier.startFloor * FLOOR_HEIGHT;

            pushWall(Vec3(tCX, baseY + tierH * 0.5f, tier.tz + WALL_THICKNESS * 0.5f),
                     Vec3(tier.tw, tierH, WALL_THICKNESS), pal.wall, meshCache, objects, colliders);
            pushWall(Vec3(tCX, baseY + tierH * 0.5f, tier.tz + tier.td - WALL_THICKNESS * 0.5f),
                     Vec3(tier.tw, tierH, WALL_THICKNESS), pal.wall, meshCache, objects, colliders);
            pushWall(Vec3(tier.tx + WALL_THICKNESS * 0.5f, baseY + tierH * 0.5f, tCZ),
                     Vec3(WALL_THICKNESS, tierH, tier.td), pal.wall, meshCache, objects, colliders);
            pushWall(Vec3(tier.tx + tier.tw - WALL_THICKNESS * 0.5f, baseY + tierH * 0.5f, tCZ),
                     Vec3(WALL_THICKNESS, tierH, tier.td), pal.wall, meshCache, objects, colliders);

            // Ground slab (only for first tier)
            if (ti == 0) {
                pushBox(Vec3(tCX, gY, tCZ), Vec3(tier.tw, WALL_THICKNESS, tier.td),
                        pal.floor, meshCache, objects);
            }

            // Roof/ledge slab at top of this tier
            pushBox(Vec3(tCX, baseY + tierH, tCZ),
                    Vec3(tier.tw, WALL_THICKNESS, tier.td),
                    (ti == tiers.size() - 1) ? pal.roof : pal.floor,
                    meshCache, objects);

            // Solid interior AABB
            colliders.push_back(AABB(
                Vec3(tier.tx + WALL_THICKNESS, baseY, tier.tz + WALL_THICKNESS),
                Vec3(tier.tx + tier.tw - WALL_THICKNESS, baseY + tierH,
                     tier.tz + tier.td - WALL_THICKNESS)));

        } else if (!spec.hasInterior && ti > 0) {
            // Upper tier of solid building — same but no door
            float tierH = (tier.endFloor - tier.startFloor) * FLOOR_HEIGHT;
            float baseY = gY + tier.startFloor * FLOOR_HEIGHT;

            pushWall(Vec3(tCX, baseY + tierH * 0.5f, tier.tz + WALL_THICKNESS * 0.5f),
                     Vec3(tier.tw, tierH, WALL_THICKNESS), pal.wall, meshCache, objects, colliders);
            pushWall(Vec3(tCX, baseY + tierH * 0.5f, tier.tz + tier.td - WALL_THICKNESS * 0.5f),
                     Vec3(tier.tw, tierH, WALL_THICKNESS), pal.wall, meshCache, objects, colliders);
            pushWall(Vec3(tier.tx + WALL_THICKNESS * 0.5f, baseY + tierH * 0.5f, tCZ),
                     Vec3(WALL_THICKNESS, tierH, tier.td), pal.wall, meshCache, objects, colliders);
            pushWall(Vec3(tier.tx + tier.tw - WALL_THICKNESS * 0.5f, baseY + tierH * 0.5f, tCZ),
                     Vec3(WALL_THICKNESS, tierH, tier.td), pal.wall, meshCache, objects, colliders);

            pushBox(Vec3(tCX, baseY + tierH, tCZ),
                    Vec3(tier.tw, WALL_THICKNESS, tier.td),
                    (ti == tiers.size() - 1) ? pal.roof : pal.floor,
                    meshCache, objects);

            colliders.push_back(AABB(
                Vec3(tier.tx + WALL_THICKNESS, baseY, tier.tz + WALL_THICKNESS),
                Vec3(tier.tx + tier.tw - WALL_THICKNESS, baseY + tierH,
                     tier.tz + tier.td - WALL_THICKNESS)));

        } else {
            // --- Interior building tier -------------------------
            for (int f = tier.startFloor; f < tier.endFloor; ++f) {
                float floorY = gY + f * FLOOR_HEIGHT;
                float wallCY = floorY + FLOOR_HEIGHT * 0.5f;

                // Front wall (−Z)
                float frontZ = tier.tz + WALL_THICKNESS * 0.5f;
                if (doorFace == 0 && f == 0 && tierHasDoor) {
                    pushSplitWall(0, frontZ, tier.tx, tier.tw, tCX, floorY);
                } else {
                    pushWall(Vec3(tCX, wallCY, frontZ),
                             Vec3(tier.tw, FLOOR_HEIGHT, WALL_THICKNESS),
                             pal.wall, meshCache, objects, colliders);
                }

                // Back wall (+Z)
                float backZ = tier.tz + tier.td - WALL_THICKNESS * 0.5f;
                if (doorFace == 1 && f == 0 && tierHasDoor) {
                    pushSplitWall(0, backZ, tier.tx, tier.tw, tCX, floorY);
                } else {
                    pushWall(Vec3(tCX, wallCY, backZ),
                             Vec3(tier.tw, FLOOR_HEIGHT, WALL_THICKNESS),
                             pal.wall, meshCache, objects, colliders);
                }

                // Left wall (−X)
                float leftX = tier.tx + WALL_THICKNESS * 0.5f;
                if (doorFace == 2 && f == 0 && tierHasDoor) {
                    pushSplitWall(2, leftX, tier.tz, tier.td, tCZ, floorY);
                } else {
                    pushWall(Vec3(leftX, wallCY, tCZ),
                             Vec3(WALL_THICKNESS, FLOOR_HEIGHT, tier.td),
                             pal.wall, meshCache, objects, colliders);
                }

                // Right wall (+X)
                float rightX = tier.tx + tier.tw - WALL_THICKNESS * 0.5f;
                if (doorFace == 3 && f == 0 && tierHasDoor) {
                    pushSplitWall(2, rightX, tier.tz, tier.td, tCZ, floorY);
                } else {
                    pushWall(Vec3(rightX, wallCY, tCZ),
                             Vec3(WALL_THICKNESS, FLOOR_HEIGHT, tier.td),
                             pal.wall, meshCache, objects, colliders);
                }
            }

            // Floor slabs for this tier
            // For interior buildings with stairs, cut a stairwell opening
            // in floor slabs above ground level so the player can see and
            // use the stairs between floors.
            for (int f = tier.startFloor; f <= tier.endFloor; ++f) {
                // Skip ground slab if not first tier (already placed by tier 0)
                if (f == 0 && ti > 0) continue;
                float slabY = gY + f * FLOOR_HEIGHT;
                bool isTopOfTier = (f == tier.endFloor);
                bool isTopOfBuilding = (ti == (int)tiers.size() - 1 && isTopOfTier);
                Material slabMat = isTopOfBuilding ? pal.roof : pal.floor;

                // Interior traversability: add colliders for non-ground interior
                // floor slabs so the player can step off stairs onto upper floors.
                // Keep ground slab collider-free to avoid lifting the player at
                // street level when entering buildings.
                auto pushInteriorSlab = [&](const Vec3& c, const Vec3& s) {
                    pushBox(c, s, slabMat, meshCache, objects);
                    if (spec.hasInterior && ti == 0 && f > 0 && !isTopOfBuilding) {
                        pushAABB(c, s * 0.5f, colliders);
                    }
                };

                // Cut stairwell opening on interior floors (f > 0, not the
                // roof slab of the topmost tier) when the stairwell overlaps
                // this tier's footprint.
                bool cutStairwell = hasStairwell && f > 0 && !isTopOfBuilding
                    && ti == 0; // stairs only in base tier

                if (!cutStairwell) {
                    pushInteriorSlab(Vec3(tCX, slabY, tCZ),
                                     Vec3(tier.tw, WALL_THICKNESS, tier.td));
                } else {
                    // Split the floor slab into up to 4 pieces around the
                    // stairwell rectangle [swMinX..swMaxX, swMinZ..swMaxZ].
                    // Tier footprint: [tier.tx .. tier.tx+tier.tw] x
                    //                 [tier.tz .. tier.tz+tier.td]
                    float tMinX = tier.tx, tMaxX = tier.tx + tier.tw;
                    float tMinZ = tier.tz, tMaxZ = tier.tz + tier.td;

                    // Add a margin around the stairwell for the opening
                    float margin = 0.1f;
                    float oMinX = swMinX - margin;
                    float oMaxX = swMaxX + margin;
                    float oMinZ = swMinZ - margin;
                    float oMaxZ = swMaxZ + margin;

                    // Clamp to tier bounds
                    if (oMinX < tMinX) oMinX = tMinX;
                    if (oMaxX > tMaxX) oMaxX = tMaxX;
                    if (oMinZ < tMinZ) oMinZ = tMinZ;
                    if (oMaxZ > tMaxZ) oMaxZ = tMaxZ;

                    // Piece 1: full width strip from tMinZ to oMinZ (front)
                    float frontD = oMinZ - tMinZ;
                    if (frontD > 0.01f) {
                        float cz1 = (tMinZ + oMinZ) * 0.5f;
                        pushInteriorSlab(Vec3(tCX, slabY, cz1),
                                         Vec3(tier.tw, WALL_THICKNESS, frontD));
                    }
                    // Piece 2: full width strip from oMaxZ to tMaxZ (back)
                    float backD = tMaxZ - oMaxZ;
                    if (backD > 0.01f) {
                        float cz2 = (oMaxZ + tMaxZ) * 0.5f;
                        pushInteriorSlab(Vec3(tCX, slabY, cz2),
                                         Vec3(tier.tw, WALL_THICKNESS, backD));
                    }
                    // Piece 3: left strip in the stairwell Z band
                    float midZLen = oMaxZ - oMinZ;
                    float leftW = oMinX - tMinX;
                    if (leftW > 0.01f && midZLen > 0.01f) {
                        float cx3 = (tMinX + oMinX) * 0.5f;
                        float cz3 = (oMinZ + oMaxZ) * 0.5f;
                        pushInteriorSlab(Vec3(cx3, slabY, cz3),
                                         Vec3(leftW, WALL_THICKNESS, midZLen));
                    }
                    // Piece 4: right strip in the stairwell Z band
                    float rightW = tMaxX - oMaxX;
                    if (rightW > 0.01f && midZLen > 0.01f) {
                        float cx4 = (oMaxX + tMaxX) * 0.5f;
                        float cz4 = (oMinZ + oMaxZ) * 0.5f;
                        pushInteriorSlab(Vec3(cx4, slabY, cz4),
                                         Vec3(rightW, WALL_THICKNESS, midZLen));
                    }
                    // The stairwell rectangle [oMinX..oMaxX, oMinZ..oMaxZ]
                    // is left open — no slab here.
                }
            }

            // Door frame trim (ground floor of base tier only)
            if (tierHasDoor) {
                float doorX, doorZ, doorYaw;
                float doorWallW = DOOR_WIDTH + 0.4f;
                float doorWallH = FLOOR_HEIGHT;
                float doorWallD = WALL_THICKNESS;

                switch (doorFace) {
                default:
                case 0: doorX = tCX;             doorZ = tier.tz;             doorYaw = 0.0f;      break;
                case 1: doorX = tCX;             doorZ = tier.tz + tier.td;   doorYaw = 3.14159f;  break;
                case 2: doorX = tier.tx;         doorZ = tCZ;                 doorYaw = 1.5708f;   break;
                case 3: doorX = tier.tx + tier.tw; doorZ = tCZ;              doorYaw = -1.5708f;  break;
                }

                placeDoorFrame(doorX, gY, doorZ, doorYaw,
                               doorWallW, doorWallH, doorWallD,
                               pal, meshCache, objects, colliders);
            }
        }

        // ── Ledge / cornice between tiers ───────────────────────
        // Add a decorative horizontal band at the top of each tier
        // (except the topmost) where the setback occurs.
        if (ti < tiers.size() - 1) {
            float ledgeY = gY + tier.endFloor * FLOOR_HEIGHT;
            // Ledge slightly wider than the tier footprint
            pushBox(Vec3(tCX, ledgeY + 0.05f, tCZ),
                    Vec3(tier.tw + 0.2f, 0.1f, tier.td + 0.2f),
                    pal.trim, meshCache, objects);
        }

        // ── Accent band at base of each tier ────────────────────
        // Thin horizontal stripe in accent color at the base
        if (tier.startFloor > 0) {
            float bandY = gY + tier.startFloor * FLOOR_HEIGHT + 0.15f;
            pushBox(Vec3(tCX, bandY, tCZ),
                    Vec3(tier.tw + 0.02f, 0.08f, tier.td + 0.02f),
                    pal.accent, meshCache, objects);
        }
    }

    // ── Facade details: base trim + parapet ─────────────────────
    // Ground-level base trim (darker band at the bottom of the building)
    pushBox(Vec3(cx, gY + 0.08f, cz),
            Vec3(W + 0.06f, 0.16f, D + 0.06f),
            pal.wallDark, meshCache, objects);

    // Parapet wall at the very top tier
    if (!tiers.empty()) {
        const Tier& topTier = tiers.back();
        float topCX = topTier.tx + topTier.tw * 0.5f;
        float topCZ = topTier.tz + topTier.td * 0.5f;
        float roofTop = gY + topTier.endFloor * FLOOR_HEIGHT;
        float parapetH = 0.3f;

        // 4 parapet wall segments (thin walls above the roof edge)
        pushBox(Vec3(topCX, roofTop + parapetH * 0.5f,
                     topTier.tz + 0.05f),
                Vec3(topTier.tw, parapetH, 0.1f),
                pal.wallDark, meshCache, objects);
        pushBox(Vec3(topCX, roofTop + parapetH * 0.5f,
                     topTier.tz + topTier.td - 0.05f),
                Vec3(topTier.tw, parapetH, 0.1f),
                pal.wallDark, meshCache, objects);
        pushBox(Vec3(topTier.tx + 0.05f, roofTop + parapetH * 0.5f, topCZ),
                Vec3(0.1f, parapetH, topTier.td),
                pal.wallDark, meshCache, objects);
        pushBox(Vec3(topTier.tx + topTier.tw - 0.05f,
                     roofTop + parapetH * 0.5f, topCZ),
                Vec3(0.1f, parapetH, topTier.td),
                pal.wallDark, meshCache, objects);
    }

    // ────────────────────────────────────────────────────────────
    //  2. Windows on all four exterior faces (per tier)
    // ────────────────────────────────────────────────────────────

    auto placeWindowStripRange = [&](
        int axis,        // 0 = along X (front/back walls), 2 = along Z (left/right walls)
        float fixedPos,  // the fixed coordinate (z for front/back, x for left/right)
        float spanStart, // start of span (x or z)
        float spanLen,   // length of span
        int floorStart,  // first floor (inclusive)
        int floorEnd)    // last floor (exclusive)
    {
        int nWin = std::max(1, (int)(spanLen / WINDOW_SPACING));
        float step = spanLen / (float)nWin;

        for (int f = floorStart; f < floorEnd; ++f) {
            float winCY = gY + f * FLOOR_HEIGHT + FLOOR_HEIGHT * 0.5f;

            for (int w = 0; w < nWin; ++w) {
                float along = spanStart + step * (w + 0.5f);

                // Alternate bright / dim accent
                const Material& winMat =
                    ((f + w) & 1) ? pal.accent : pal.accentDim;

                Vec3 center, size;
                if (axis == 0) {
                    center = Vec3(along, winCY, fixedPos);
                    size   = Vec3(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_DEPTH);
                } else {
                    center = Vec3(fixedPos, winCY, along);
                    size   = Vec3(WINDOW_DEPTH, WINDOW_HEIGHT, WINDOW_WIDTH);
                }

                pushBox(center, size, winMat, meshCache, objects);
            }
        }
    };

    // Place windows per tier (each tier has its own footprint)
    for (const auto& tier : tiers) {
        float twt = WALL_THICKNESS;
        // Front windows (−Z face)
        placeWindowStripRange(0, tier.tz - WINDOW_DEPTH * 0.5f,
                              tier.tx + twt, tier.tw - 2.0f * twt,
                              tier.startFloor, tier.endFloor);
        // Back windows (+Z face)
        placeWindowStripRange(0, tier.tz + tier.td + WINDOW_DEPTH * 0.5f,
                              tier.tx + twt, tier.tw - 2.0f * twt,
                              tier.startFloor, tier.endFloor);
        // Left windows (−X face)
        placeWindowStripRange(2, tier.tx - WINDOW_DEPTH * 0.5f,
                              tier.tz + twt, tier.td - 2.0f * twt,
                              tier.startFloor, tier.endFloor);
        // Right windows (+X face)
        placeWindowStripRange(2, tier.tx + tier.tw + WINDOW_DEPTH * 0.5f,
                              tier.tz + twt, tier.td - 2.0f * twt,
                              tier.startFloor, tier.endFloor);
    }

    // ────────────────────────────────────────────────────────────
    //  4. Interior rooms, stairs, furniture, lighting
    // ────────────────────────────────────────────────────────────

    if (spec.hasInterior) {
        // Interior usable area (inside exterior walls)
        const float iMinX = bx + WALL_THICKNESS;
        const float iMaxX = bx + W - WALL_THICKNESS;
        const float iMinZ = bz + WALL_THICKNESS;
        const float iMaxZ = bz + D - WALL_THICKNESS;
        const float iW = iMaxX - iMinX;
        const float iD = iMaxZ - iMinZ;
        const float iCX = (iMinX + iMaxX) * 0.5f;
        const float iCZ = (iMinZ + iMaxZ) * 0.5f;

        // Decide room layout: split each floor with one X-wall
        // and one Z-wall through the center, creating up to 4 rooms.
        // RNG decides whether to place 1 or 2 dividing walls.
        bool splitX = (buildingRng(rng) % 3) != 0; // ~67 %
        bool splitZ = (buildingRng(rng) % 3) != 0;

        // Staircase placement: always in back-right quadrant,
        // running along Z axis, one per floor except the top.
        float stairW     = 1.2f;
        float stairDepth = 2.5f;
        float stairX     = iMaxX - stairW - 0.2f;
        float stairZ     = iMaxZ - stairDepth - 0.2f;

        for (int f = 0; f < spec.floors; ++f) {
            float floorY = gY + f * FLOOR_HEIGHT;

            // ── Interior dividing walls ────────────────────────
            // Split each dividing wall to leave a door opening gap
            // at the center, rather than overlaying placeDoorFrame
            // on a full-width wall (which blocks passage).
            //
            // On the ground floor, skip the dividing wall that is
            // perpendicular to the exterior door face — it would
            // block the entrance path.
            //   doorFace 0/1 (front/back -Z/+Z) → player enters
            //     along Z, so the splitX wall (at iCZ) blocks them.
            //   doorFace 2/3 (left/right -X/+X) → player enters
            //     along X, so the splitZ wall (at iCX) blocks them.

            bool skipSplitX = (f == 0 && spec.hasInterior
                               && (doorFace == 0 || doorFace == 1));
            bool skipSplitZ = (f == 0 && spec.hasInterior
                               && (doorFace == 2 || doorFace == 3));

            if (splitX && !skipSplitX) {
                // Wall running along X at the Z-midpoint, split for door
                float halfDoor = DOOR_WIDTH * 0.5f;
                float wallCY   = floorY + FLOOR_HEIGHT * 0.5f;

                // Left segment
                float leftW = (iCX - halfDoor) - iMinX;
                if (leftW > 0.01f) {
                    float leftCenterX = iMinX + leftW * 0.5f;
                    pushWall(Vec3(leftCenterX, wallCY, iCZ),
                             Vec3(leftW, FLOOR_HEIGHT, WALL_THICKNESS),
                             pal.wallDark, meshCache, objects, colliders);
                }
                // Right segment
                float rightW = iMaxX - (iCX + halfDoor);
                if (rightW > 0.01f) {
                    float rightCenterX = (iCX + halfDoor) + rightW * 0.5f;
                    pushWall(Vec3(rightCenterX, wallCY, iCZ),
                             Vec3(rightW, FLOOR_HEIGHT, WALL_THICKNESS),
                             pal.wallDark, meshCache, objects, colliders);
                }
                // Lintel above door
                float lintelH = FLOOR_HEIGHT - DOOR_HEIGHT;
                if (lintelH > 0.01f) {
                    float lintelCY = floorY + DOOR_HEIGHT + lintelH * 0.5f;
                    pushWall(Vec3(iCX, lintelCY, iCZ),
                             Vec3(DOOR_WIDTH, lintelH, WALL_THICKNESS),
                             pal.wallDark, meshCache, objects, colliders);
                }
            }
            if (splitZ && !skipSplitZ) {
                // Wall running along Z at the X-midpoint, split for door
                float halfDoor = DOOR_WIDTH * 0.5f;
                float wallCY   = floorY + FLOOR_HEIGHT * 0.5f;

                // Near segment (low Z)
                float nearD = (iCZ - halfDoor) - iMinZ;
                if (nearD > 0.01f) {
                    float nearCenterZ = iMinZ + nearD * 0.5f;
                    pushWall(Vec3(iCX, wallCY, nearCenterZ),
                             Vec3(WALL_THICKNESS, FLOOR_HEIGHT, nearD),
                             pal.wallDark, meshCache, objects, colliders);
                }
                // Far segment (high Z)
                float farD = iMaxZ - (iCZ + halfDoor);
                if (farD > 0.01f) {
                    float farCenterZ = (iCZ + halfDoor) + farD * 0.5f;
                    pushWall(Vec3(iCX, wallCY, farCenterZ),
                             Vec3(WALL_THICKNESS, FLOOR_HEIGHT, farD),
                             pal.wallDark, meshCache, objects, colliders);
                }
                // Lintel above door
                float lintelH = FLOOR_HEIGHT - DOOR_HEIGHT;
                if (lintelH > 0.01f) {
                    float lintelCY = floorY + DOOR_HEIGHT + lintelH * 0.5f;
                    pushWall(Vec3(iCX, lintelCY, iCZ),
                             Vec3(WALL_THICKNESS, lintelH, DOOR_WIDTH),
                             pal.wallDark, meshCache, objects, colliders);
                }
            }

            // ── Staircase (not on top floor) ───────────────────
            if (f < spec.floors - 1) {
                placeStaircase(stairX, floorY, stairZ,
                               stairW, FLOOR_HEIGHT, stairDepth,
                               2, true, pal,
                               meshCache, objects, colliders, slopeColliders);

                // Keep stairwell clear of props/extra colliders by skipping
                // nearby room quadrants on floors where stairs exist.
            }

            // ── Per-room lighting (ceiling centre of each quadrant)
            // With the cross-walls we get up to 4 rooms.  Place a
            // light in the centre of each occupied quadrant.
            float lightY  = floorY + FLOOR_HEIGHT - 0.2f;
            float qOffX   = iW * 0.25f;
            float qOffZ   = iD * 0.25f;
            int   nRoomX  = splitZ ? 2 : 1;
            int   nRoomZ  = splitX ? 2 : 1;

            for (int rx = 0; rx < nRoomX; ++rx) {
                for (int rz = 0; rz < nRoomZ; ++rz) {
                    float lx = iMinX + qOffX + rx * iW * 0.5f;
                    float lz = iMinZ + qOffZ + rz * iD * 0.5f;
                    lights.push_back(PointLight(
                        Vec3(lx, lightY, lz),
                        pal.accent.color, 1.0f, 6.0f));
                }
            }

            // ── Furnish rooms based on district ────────────────
            // Place furniture in each room quadrant (up to 4 rooms
            // per floor depending on the dividing-wall layout).
            // Skip the staircase quadrant (back-right, rx==1 rz==1).

            for (int rx = 0; rx < nRoomX; ++rx) {
                for (int rz = 0; rz < nRoomZ; ++rz) {
                    // Skip staircase quadrant (back-right) on non-top floors
                    // where the staircase lives.
                    if (rx == nRoomX - 1 && rz == nRoomZ - 1
                        && f < spec.floors - 1)
                        continue;

                    // Also skip the room directly in front of the staircase to
                    // keep approach space clear and prevent accidental blocker
                    // colliders at the stair base.
                    if (rz == nRoomZ - 1 && rx == nRoomX - 2
                        && f < spec.floors - 1)
                        continue;

                    // Compute the bounds of this room quadrant
                    float roomMinX = iMinX + rx * iW * 0.5f;
                    float roomMaxX = (nRoomX == 1) ? iMaxX
                                     : iMinX + (rx + 1) * iW * 0.5f;
                    float roomMinZ = iMinZ + rz * iD * 0.5f;
                    float roomMaxZ = (nRoomZ == 1) ? iMaxZ
                                     : iMinZ + (rz + 1) * iD * 0.5f;

                    float furnX = roomMinX + 1.2f;
                    float furnZ = roomMinZ + 1.2f;
                    float roomCX = (roomMinX + roomMaxX) * 0.5f;
                    float roomCZ = (roomMinZ + roomMaxZ) * 0.5f;
                    float yaw = buildingRngFloat(rng) * 6.2832f;
                    uint32_t roomRoll = buildingRng(rng);

                    switch (spec.district) {
                    case DistrictType::Downtown:
                        if (f == 0) {
                            // Lobby: pillars at each room's corners + central terminal
                            placePillar(roomMinX + 0.5f, floorY, roomMinZ + 0.5f,
                                        FLOOR_HEIGHT, pal, meshCache, objects, colliders);
                            placePillar(roomMaxX - 0.5f, floorY, roomMinZ + 0.5f,
                                        FLOOR_HEIGHT, pal, meshCache, objects, colliders);
                            if (rx == 0 && rz == 0) {
                                placeTerminal(roomCX, floorY, roomMinZ + 1.5f, 0.0f,
                                              pal, meshCache, objects, lights, colliders);
                            }
                        } else {
                            // Office floors: each room gets desk + terminal or shelves
                            if (roomRoll % 2 == 0) {
                                placeTable(furnX, floorY, furnZ, yaw,
                                           pal, meshCache, objects, colliders);
                                placeTerminal(furnX + 2.0f, floorY, furnZ, yaw,
                                              pal, meshCache, objects, lights, colliders);
                            } else {
                                placeShelfRack(furnX, floorY, furnZ, yaw,
                                               pal, meshCache, objects, colliders);
                                if (roomRoll % 3 == 0) {
                                    placeTable(roomCX, floorY, furnZ + 1.5f, yaw,
                                               pal, meshCache, objects, colliders);
                                }
                            }
                        }
                        break;

                    case DistrictType::Tech:
                        // Lab room: table + shelf rack, and terminal with 50% chance
                        placeTable(furnX, floorY, furnZ, yaw,
                                   pal, meshCache, objects, colliders);
                        placeShelfRack(roomMaxX - 1.5f, floorY, furnZ, 3.14159f,
                                       pal, meshCache, objects, colliders);
                        if (roomRoll % 2 == 0) {
                            placeTerminal(roomCX, floorY, roomMinZ + 1.0f, yaw,
                                          pal, meshCache, objects, lights, colliders);
                        }
                        break;

                    case DistrictType::Industrial:
                        // Warehouse: crates, shelves, occasional debris
                        placeCrate(furnX, floorY, furnZ, yaw,
                                   pal, meshCache, objects, colliders);
                        if (roomRoll % 3 != 0) {
                            placeCrate(furnX + 1.5f, floorY, furnZ + 1.5f,
                                       yaw + 0.5f, pal, meshCache, objects, colliders);
                        }
                        placeShelfRack(roomMaxX - 1.5f, floorY, roomMinZ + 1.0f, 0.0f,
                                       pal, meshCache, objects, colliders);
                        if (roomRoll % 4 == 0) {
                            placeDebris(roomCX, floorY, roomCZ,
                                        rng, pal, meshCache, objects);
                        }
                        break;

                    case DistrictType::Slums:
                        // Squatter room: debris always, furniture sometimes
                        placeDebris(furnX, floorY, furnZ,
                                    rng, pal, meshCache, objects);
                        if (roomRoll % 2 == 0) {
                            placeTable(roomCX, floorY, furnZ + 1.5f, yaw,
                                       pal, meshCache, objects, colliders);
                        }
                        if (roomRoll % 3 == 0) {
                            placeFireBarrel(roomCX + 1.0f, floorY, roomCZ, 0.0f,
                                            pal, meshCache, objects, lights, colliders);
                        }
                        break;

                    case DistrictType::Residential:
                        // Apartment room: table + shelves, occasional terminal
                        placeTable(furnX, floorY, furnZ, yaw,
                                   pal, meshCache, objects, colliders);
                        placeShelfRack(roomMaxX - 1.5f, floorY, furnZ, 3.14159f,
                                       pal, meshCache, objects, colliders);
                        if (roomRoll % 3 == 0) {
                            placeTerminal(roomCX, floorY, roomMinZ + 1.0f, yaw,
                                          pal, meshCache, objects, lights, colliders);
                        }
                        break;

                    case DistrictType::Wasteland:
                        placeDebris(furnX, floorY, furnZ,
                                    rng, pal, meshCache, objects);
                        break;
                    }
                }
            }
        }
    }

    // ────────────────────────────────────────────────────────────
    //  5. Rooftop details (placed on topmost tier)
    // ────────────────────────────────────────────────────────────

    float roofY = gY + H;
    // Use the topmost tier's footprint for rooftop props
    float rtx = bx, rtz = bz, rtw = W, rtd = D;
    if (!tiers.empty()) {
        const Tier& topTier = tiers.back();
        rtx = topTier.tx; rtz = topTier.tz;
        rtw = topTier.tw; rtd = topTier.td;
    }
    float rtcx = rtx + rtw * 0.5f;
    float rtcz = rtz + rtd * 0.5f;

    switch (spec.district) {
    case DistrictType::Downtown:
        // Antenna on tall buildings
        if (spec.floors >= 8) {
            placeAntenna(rtcx, roofY, rtcz, pal, meshCache, objects);
        }
        break;

    case DistrictType::Tech:
        // Antenna + pipes
        placeAntenna(rtcx + std::min(1.0f, rtw * 0.3f), roofY, rtcz, pal, meshCache, objects);
        if (rtw > 2.5f) {
            placePipeRun(Vec3(rtx + 1.0f, roofY + 0.1f, rtz + 1.0f),
                         0, rtw - 2.0f, pal, meshCache, objects);
        }
        break;

    case DistrictType::Industrial:
        // Pipe runs along both axes
        if (rtw > 2.0f) {
            placePipeRun(Vec3(rtx + 0.5f, roofY + 0.1f, rtz + 0.5f),
                         0, rtw - 1.0f, pal, meshCache, objects);
        }
        if (rtd > 2.0f) {
            placePipeRun(Vec3(rtx + 0.5f, roofY + 0.3f, rtz + 0.5f),
                         2, rtd - 1.0f, pal, meshCache, objects);
        }
        break;

    case DistrictType::Slums:
        // Occasional antenna
        if (buildingRng(rng) % 3 == 0) {
            placeAntenna(rtcx, roofY, rtcz, pal, meshCache, objects);
        }
        break;

    case DistrictType::Residential:
        // Small antenna
        if (buildingRng(rng) % 2 == 0) {
            placeAntenna(rtcx, roofY, rtcz, pal, meshCache, objects);
        }
        break;

    case DistrictType::Wasteland:
        // Ruins get nothing on top
        break;
    }

    // ────────────────────────────────────────────────────────────
    //  6. Neon sign boards on building facades
    // ────────────────────────────────────────────────────────────
    // Place 1-3 illuminated sign boards on random exterior walls.
    // More signs in Downtown/Tech, fewer in residential areas, none
    // in Wasteland.

    {
        int maxSigns = 0;
        float signProb = 0.0f;

        switch (spec.district) {
        case DistrictType::Downtown:    maxSigns = 3; signProb = 0.85f; break;
        case DistrictType::Tech:        maxSigns = 2; signProb = 0.70f; break;
        case DistrictType::Industrial:  maxSigns = 1; signProb = 0.40f; break;
        case DistrictType::Slums:       maxSigns = 2; signProb = 0.50f; break;
        case DistrictType::Residential: maxSigns = 1; signProb = 0.30f; break;
        case DistrictType::Wasteland:   maxSigns = 0; signProb = 0.0f;  break;
        }

        for (int s = 0; s < maxSigns; ++s) {
            if (buildingRngFloat(rng) > signProb) continue;

            // Pick a random face: 0=front, 1=back, 2=left, 3=right
            int face = buildingRng(rng) % 4;
            // Pick a floor for the sign (between 1st and top floor)
            int signFloor = (spec.floors <= 1)
                ? 0
                : (int)(buildingRng(rng) % (uint32_t)spec.floors);
            float signY = gY + signFloor * FLOOR_HEIGHT + FLOOR_HEIGHT * 0.6f;

            // Find the tier that contains this floor
            float stx = bx, stz = bz, stw = W, std_ = D;
            for (const auto& tier : tiers) {
                if (signFloor >= tier.startFloor && signFloor < tier.endFloor) {
                    stx = tier.tx; stz = tier.tz; stw = tier.tw; std_ = tier.td;
                    break;
                }
            }

            // Randomize horizontal position along the wall face
            float along = 0.3f + buildingRngFloat(rng) * 0.4f; // 30-70% along wall

            Vec3 basePos;
            Vec3 wallNormal;

            switch (face) {
            default:
            case 0: // front (−Z)
                basePos    = Vec3(stx + stw * along, signY, stz);
                wallNormal = Vec3(0.0f, 0.0f, -1.0f);
                break;
            case 1: // back (+Z)
                basePos    = Vec3(stx + stw * along, signY, stz + std_);
                wallNormal = Vec3(0.0f, 0.0f, 1.0f);
                break;
            case 2: // left (−X)
                basePos    = Vec3(stx, signY, stz + std_ * along);
                wallNormal = Vec3(-1.0f, 0.0f, 0.0f);
                break;
            case 3: // right (+X)
                basePos    = Vec3(stx + stw, signY, stz + std_ * along);
                wallNormal = Vec3(1.0f, 0.0f, 0.0f);
                break;
            }

            placeSignBoard(basePos, wallNormal, pal, meshCache, objects, lights);
        }
    }
}
