#include "world/city_layout.h"
#include "world/prefabs/street_props/streetlamp.h"
#include "world/prefabs/street_props/trash_can.h"
#include "world/prefabs/street_props/bollard.h"
#include "world/prefabs/street_props/bench.h"
#include "world/prefabs/street_props/fire_barrel.h"
#include "world/prefabs/vehicles/vehicle_husk.h"
#include "world/prefabs/vehicles/crate.h"
#include "world/prefabs/street_props/barricade.h"
#include "world/prefabs/street_props/dumpster.h"
#include "world/prefabs/structural/rubble_pile.h"
#include "world/prefabs/building_mounted/sign_board.h"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Deterministic RNG helpers
// ---------------------------------------------------------------------------
static uint32_t cityRng(uint32_t& s) {
    s = s * 1103515245u + 12345u;
    return (s >> 16) & 0x7FFF;
}

static float cityRngFloat(uint32_t& s) {
    return cityRng(s) / 32767.0f;
}

// ---------------------------------------------------------------------------
// Building shape picker — assigns visual shape based on district & floors
// ---------------------------------------------------------------------------
static BuildingShape pickBuildingShape(DistrictType dt, int floors, uint32_t& rng) {
    float roll = cityRngFloat(rng);

    switch (dt) {
    case DistrictType::Downtown:
        // Tall downtown buildings get the most variety
        if (floors >= 12) {
            if (roll < 0.30f)      return BuildingShape::Setback;
            else if (roll < 0.55f) return BuildingShape::Tiered;
            else if (roll < 0.75f) return BuildingShape::Tower;
            else                   return BuildingShape::Box;
        } else if (floors >= 6) {
            if (roll < 0.25f)      return BuildingShape::Setback;
            else if (roll < 0.45f) return BuildingShape::Tiered;
            else if (roll < 0.55f) return BuildingShape::Wedge;
            else                   return BuildingShape::Box;
        }
        return BuildingShape::Box;

    case DistrictType::Tech:
        if (roll < 0.30f)      return BuildingShape::Setback;
        else if (roll < 0.50f) return BuildingShape::Wedge;
        else if (roll < 0.65f) return BuildingShape::Tiered;
        else                   return BuildingShape::Box;

    case DistrictType::Industrial:
        if (roll < 0.20f)      return BuildingShape::Wedge;
        else                   return BuildingShape::Box;

    case DistrictType::Slums:
        if (roll < 0.15f)      return BuildingShape::Wedge;
        else if (roll < 0.25f) return BuildingShape::Tiered;
        else                   return BuildingShape::Box;

    case DistrictType::Residential:
        if (roll < 0.15f)      return BuildingShape::Setback;
        else if (roll < 0.25f) return BuildingShape::Wedge;
        else                   return BuildingShape::Box;

    case DistrictType::Wasteland:
        return BuildingShape::Box; // Ruins stay simple

    default:
        return BuildingShape::Box;
    }
}

// ---------------------------------------------------------------------------
// Street grid constants
// ---------------------------------------------------------------------------
static constexpr float WORLD_SIZE   = 256.0f;
static constexpr float MAJOR_SPACING = 64.0f;  // every 4 chunks
static constexpr float MINOR_SPACING = 32.0f;  // every 2 chunks
static constexpr float MAJOR_WIDTH   = 8.0f;
static constexpr float MINOR_WIDTH   = 4.0f;
static constexpr float SIDEWALK_W    = 1.5f;
static constexpr float SIDEWALK_H    = 0.15f;
static constexpr float ROAD_Y        = 0.01f;
static constexpr float SIDEWALK_Y    = 0.075f; // half of 0.15

// ---------------------------------------------------------------------------
// Palette helper
// ---------------------------------------------------------------------------
static DistrictPalette makePalette(DistrictType t) {
    switch (t) {
        case DistrictType::Downtown:    return CityMaterials::downtown();
        case DistrictType::Tech:        return CityMaterials::tech();
        case DistrictType::Industrial:  return CityMaterials::industrial();
        case DistrictType::Slums:       return CityMaterials::slums();
        case DistrictType::Residential: return CityMaterials::residential();
        case DistrictType::Wasteland:   return CityMaterials::wasteland();
    }
    return CityMaterials::downtown();
}

// ---------------------------------------------------------------------------
// Query helpers
// ---------------------------------------------------------------------------
DistrictType CityLayout::districtAt(float wx, float wz) const {
    int cx = (int)std::floor(wx / CHUNK_SIZE);
    int cz = (int)std::floor(wz / CHUNK_SIZE);
    cx = std::max(0, std::min(WORLD_CHUNKS - 1, cx));
    cz = std::max(0, std::min(WORLD_CHUNKS - 1, cz));
    return m_districtMap[cz][cx];
}

const DistrictPalette& CityLayout::paletteAt(float wx, float wz) const {
    return m_palettes[(int)districtAt(wx, wz)];
}

// ---------------------------------------------------------------------------
// Top-level generate
// ---------------------------------------------------------------------------
void CityLayout::generate(uint32_t seed,
                           MeshCache& meshCache,
                           std::vector<SceneObject>& objects,
                           std::vector<PointLight>& lights,
                           std::vector<AABB>& colliders,
                           std::vector<SlopeCollider>& slopeColliders) {
    m_seed = seed;

    // Cache palettes
    m_palettes[0] = CityMaterials::downtown();
    m_palettes[1] = CityMaterials::tech();
    m_palettes[2] = CityMaterials::industrial();
    m_palettes[3] = CityMaterials::slums();
    m_palettes[4] = CityMaterials::residential();
    m_palettes[5] = CityMaterials::wasteland();

    assignDistricts(seed);
    generateGroundPlane(meshCache, objects, colliders);
    generateStreets(seed, meshCache, objects, lights, colliders);

    // Identify city blocks (areas between streets) and fill them
    // We walk through all minor-grid cells. A "block" is the area between
    // adjacent N-S and E-W streets.
    // Streets lie at every MINOR_SPACING (32 units), starting at 0.
    // The block interior starts after the street half-width + sidewalk on
    // each side.
    int numStreets = (int)(WORLD_SIZE / MINOR_SPACING) + 1; // 0,32,64,...,256 -> 9

    for (int bz = 0; bz < numStreets - 1; bz++) {
        for (int bx = 0; bx < numStreets - 1; bx++) {
            float streetX0 = bx * MINOR_SPACING;
            float streetX1 = (bx + 1) * MINOR_SPACING;
            float streetZ0 = bz * MINOR_SPACING;
            float streetZ1 = (bz + 1) * MINOR_SPACING;

            // Determine street widths on each edge
            auto halfStreet = [](float pos) -> float {
                // Check if major or minor
                float rem = std::fmod(pos, MAJOR_SPACING);
                if (rem < 0.01f || rem > MAJOR_SPACING - 0.01f) return MAJOR_WIDTH * 0.5f;
                return MINOR_WIDTH * 0.5f;
            };

            float marginX0 = halfStreet(streetX0) + SIDEWALK_W;
            float marginX1 = halfStreet(streetX1) + SIDEWALK_W;
            float marginZ0 = halfStreet(streetZ0) + SIDEWALK_W;
            float marginZ1 = halfStreet(streetZ1) + SIDEWALK_W;

            float x0 = streetX0 + marginX0;
            float x1 = streetX1 - marginX1;
            float z0 = streetZ0 + marginZ0;
            float z1 = streetZ1 - marginZ1;

            // Skip degenerate blocks
            if (x1 - x0 < 4.0f || z1 - z0 < 4.0f) continue;

            generateBlock(bx, bz, x0, z0, x1, z1, seed,
                          meshCache, objects, lights, colliders, slopeColliders);
        }
    }
}

// ---------------------------------------------------------------------------
// assignDistricts
// ---------------------------------------------------------------------------
void CityLayout::assignDistricts(uint32_t seed) {
    float center = WORLD_SIZE * 0.5f; // 128

    for (int cz = 0; cz < WORLD_CHUNKS; cz++) {
        for (int cx = 0; cx < WORLD_CHUNKS; cx++) {
            // Chunk center in world coords
            float wcx = (cx + 0.5f) * CHUNK_SIZE;
            float wcz = (cz + 0.5f) * CHUNK_SIZE;

            float dx = wcx - center;
            float dz = wcz - center;
            float dist = std::sqrt(dx * dx + dz * dz) / CHUNK_SIZE; // distance in chunks

            uint32_t r = chunkRandom(seed, cx, cz, 0);
            float t = (float)(r & 0xFFFF) / 65535.0f;

            DistrictType type;
            if (dist < 3.0f) {
                type = DistrictType::Downtown;
            } else if (dist < 5.0f) {
                type = (t < 0.6f) ? DistrictType::Tech : DistrictType::Downtown;
            } else if (dist < 7.0f) {
                if (t < 0.4f) type = DistrictType::Residential;
                else if (t < 0.8f) type = DistrictType::Industrial;
                else type = DistrictType::Tech;
            } else {
                type = (t < 0.6f) ? DistrictType::Slums : DistrictType::Wasteland;
            }

            m_districtMap[cz][cx] = type;
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: add a thin box (road segment or sidewalk segment) as a cube mesh
// ---------------------------------------------------------------------------
static void addBox(float cx, float cy, float cz,
                   float sx, float sy, float sz,
                   const Material& mat,
                   MeshCache& meshCache,
                   std::vector<SceneObject>& objects,
                   std::vector<AABB>* colliders) {
    const Mesh* m = meshCache.cube();

    Mat4 tf = Mat4::identity();
    tf = tf * Mat4::translate(Vec3(cx, cy, cz));
    tf = tf * Mat4::scale(Vec3(sx, sy, sz));

    SceneObject obj;
    obj.mesh = m;
    obj.transform = tf;
    obj.material = mat;
    objects.push_back(obj);

    if (colliders) {
        float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
        colliders->push_back(AABB(
            Vec3(cx - hx, cy - hy, cz - hz),
            Vec3(cx + hx, cy + hy, cz + hz)
        ));
    }
}

// ---------------------------------------------------------------------------
// generateGroundPlane
// ---------------------------------------------------------------------------
void CityLayout::generateGroundPlane(MeshCache& meshCache,
                                      std::vector<SceneObject>& objects,
                                      std::vector<AABB>& colliders) {
    // Large ground plane at Y=0 covering entire world
    // Implemented as a thin cube so we get a collision AABB
    float half = WORLD_SIZE * 0.5f;
    float thickness = 0.5f;

    // Darken the road material for the ground
    Material groundMat = CityMaterials::downtown().road;
    groundMat.color = Color3(groundMat.color.r * 0.6f,
                             groundMat.color.g * 0.6f,
                             groundMat.color.b * 0.6f);

    addBox(half, -thickness * 0.5f, half,
           WORLD_SIZE, thickness, WORLD_SIZE,
           groundMat, meshCache, objects, &colliders);
}

// ---------------------------------------------------------------------------
// generateStreets
// ---------------------------------------------------------------------------
void CityLayout::generateStreets(uint32_t seed,
                                  MeshCache& meshCache,
                                  std::vector<SceneObject>& objects,
                                  std::vector<PointLight>& lights,
                                  std::vector<AABB>& colliders) {
    // Use the downtown palette for all road/sidewalk surfaces (uniform look)
    const DistrictPalette& roadPal = m_palettes[(int)DistrictType::Downtown];

    // Helper lambda: lay a road strip + sidewalks for a street segment
    // axis=0 means the street runs along X (E-W), axis=2 means along Z (N-S)
    auto layRoad = [&](float roadCenter, float roadStart, float roadEnd,
                       float roadWidth, int axis) {
        float halfW = roadWidth * 0.5f;
        float length = roadEnd - roadStart;
        float midLen = (roadStart + roadEnd) * 0.5f;

        // Road surface (thin cube at ROAD_Y)
        if (axis == 2) {
            // N-S road: center at (roadCenter, ROAD_Y, midLen)
            addBox(roadCenter, ROAD_Y, midLen,
                   roadWidth, 0.02f, length,
                   roadPal.road, meshCache, objects, nullptr);
            // Sidewalks on both sides
            addBox(roadCenter - halfW - SIDEWALK_W * 0.5f, SIDEWALK_Y, midLen,
                   SIDEWALK_W, SIDEWALK_H, length,
                   roadPal.sidewalk, meshCache, objects, &colliders);
            addBox(roadCenter + halfW + SIDEWALK_W * 0.5f, SIDEWALK_Y, midLen,
                   SIDEWALK_W, SIDEWALK_H, length,
                   roadPal.sidewalk, meshCache, objects, &colliders);
        } else {
            // E-W road: center at (midLen, ROAD_Y, roadCenter)
            addBox(midLen, ROAD_Y, roadCenter,
                   length, 0.02f, roadWidth,
                   roadPal.road, meshCache, objects, nullptr);
            // Sidewalks
            addBox(midLen, SIDEWALK_Y, roadCenter - halfW - SIDEWALK_W * 0.5f,
                   length, SIDEWALK_H, SIDEWALK_W,
                   roadPal.sidewalk, meshCache, objects, &colliders);
            addBox(midLen, SIDEWALK_Y, roadCenter + halfW + SIDEWALK_W * 0.5f,
                   length, SIDEWALK_H, SIDEWALK_W,
                   roadPal.sidewalk, meshCache, objects, &colliders);
        }
    };

    // Lay N-S streets (along Z axis)
    for (float x = 0.0f; x <= WORLD_SIZE + 0.01f; x += MINOR_SPACING) {
        float rem = std::fmod(x, MAJOR_SPACING);
        bool major = (rem < 0.01f || rem > MAJOR_SPACING - 0.01f);
        float w = major ? MAJOR_WIDTH : MINOR_WIDTH;
        layRoad(x, 0.0f, WORLD_SIZE, w, 2);
    }

    // Lay E-W streets (along X axis)
    for (float z = 0.0f; z <= WORLD_SIZE + 0.01f; z += MINOR_SPACING) {
        float rem = std::fmod(z, MAJOR_SPACING);
        bool major = (rem < 0.01f || rem > MAJOR_SPACING - 0.01f);
        float w = major ? MAJOR_WIDTH : MINOR_WIDTH;
        layRoad(z, 0.0f, WORLD_SIZE, w, 0);
    }

    // ── Street props along N-S streets ─────────────────────────
    uint32_t propSeed = seed ^ 0xBEEFCAFEu;

    for (float x = 0.0f; x <= WORLD_SIZE + 0.01f; x += MINOR_SPACING) {
        float rem = std::fmod(x, MAJOR_SPACING);
        bool major = (rem < 0.01f || rem > MAJOR_SPACING - 0.01f);
        float halfW = (major ? MAJOR_WIDTH : MINOR_WIDTH) * 0.5f;

        // Streetlamps every ~16 units along this N-S street
        for (float z = 8.0f; z < WORLD_SIZE; z += 16.0f) {
            const DistrictPalette& pal = paletteAt(x, z);

            // Left side lamp
            placeStreetlamp(x - halfW - SIDEWALK_W * 0.5f, SIDEWALK_H, z, 1.5708f,
                            pal, meshCache, objects, lights, colliders);
            // Right side lamp (every other one to save geometry)
            uint32_t rs = propSeed ^ ((uint32_t)(x * 100.0f) + (uint32_t)(z * 37.0f));
            if (cityRng(rs) % 2 == 0) {
                placeStreetlamp(x + halfW + SIDEWALK_W * 0.5f, SIDEWALK_H, z, -1.5708f,
                                pal, meshCache, objects, lights, colliders);
            }

            // ── Random sidewalk props (district-aware weights) ──
            float propRoll = cityRngFloat(rs);
            DistrictType localDt = districtAt(x, z);

            if (propRoll < 0.10f) {
                // Trash can — common everywhere
                placeTrashCan(x - halfW - SIDEWALK_W * 0.7f, SIDEWALK_H, z + 2.0f, 0.0f,
                              pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.16f) {
                // Bollard — more in Downtown/Tech
                placeBollard(x + halfW + SIDEWALK_W * 0.7f, SIDEWALK_H, z - 1.5f, 0.0f,
                             pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.22f) {
                // Bench
                placeBench(x - halfW - SIDEWALK_W * 0.6f, SIDEWALK_H, z + 3.0f, 0.0f,
                           pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.27f) {
                // Fire barrel — more common in Slums/Wasteland
                placeFireBarrel(x + halfW + SIDEWALK_W * 0.5f, SIDEWALK_H, z + 1.0f, 0.0f,
                                pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.32f) {
                // Dumpster — alley/industrial feel
                float dumpYaw = (cityRng(rs) % 2 == 0) ? 0.0f : 1.5708f;
                placeDumpster(x - halfW - SIDEWALK_W * 0.5f, SIDEWALK_H, z - 2.0f,
                              dumpYaw, pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.36f &&
                       (localDt == DistrictType::Slums ||
                        localDt == DistrictType::Industrial ||
                        localDt == DistrictType::Wasteland)) {
                // Barricade — only in rougher districts
                placeBarricade(x + halfW + SIDEWALK_W * 0.5f, SIDEWALK_H, z + 2.5f,
                               1.5708f, pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.40f &&
                       (localDt == DistrictType::Slums ||
                        localDt == DistrictType::Wasteland)) {
                // Rubble pile — wasteland & slums sidewalks
                uint32_t rubbleSeed = rs ^ (uint32_t)(z * 311.0f);
                placeRubblePile(x - halfW - SIDEWALK_W * 0.5f, SIDEWALK_H, z + 1.5f,
                                rubbleSeed, pal, meshCache, objects, colliders);
            } else if (propRoll < 0.44f) {
                // Crate cluster — a couple of crates near the curb
                float crateYaw = cityRngFloat(rs) * 3.14159f;
                placeCrate(x + halfW + SIDEWALK_W * 0.3f, SIDEWALK_H, z - 0.5f,
                           crateYaw, pal, meshCache, objects, colliders);
                if (cityRng(rs) % 2 == 0) {
                    placeCrate(x + halfW + SIDEWALK_W * 0.3f, SIDEWALK_H, z + 0.8f,
                               crateYaw + 0.7f, pal, meshCache, objects, colliders);
                }
            }
        }
    }

    // ── Street props along E-W streets ─────────────────────────
    uint32_t ewPropSeed = seed ^ 0xCAFE1234u;

    for (float z = 0.0f; z <= WORLD_SIZE + 0.01f; z += MINOR_SPACING) {
        float rem = std::fmod(z, MAJOR_SPACING);
        bool major = (rem < 0.01f || rem > MAJOR_SPACING - 0.01f);
        float halfW = (major ? MAJOR_WIDTH : MINOR_WIDTH) * 0.5f;

        for (float x = 8.0f; x < WORLD_SIZE; x += 16.0f) {
            const DistrictPalette& pal = paletteAt(x, z);

            // Streetlamp on the north sidewalk
            placeStreetlamp(x, SIDEWALK_H, z - halfW - SIDEWALK_W * 0.5f, 3.14159f,
                            pal, meshCache, objects, lights, colliders);

            // South sidewalk lamp (50% chance)
            uint32_t rs = ewPropSeed ^ ((uint32_t)(x * 53.0f) + (uint32_t)(z * 97.0f));
            if (cityRng(rs) % 2 == 0) {
                placeStreetlamp(x, SIDEWALK_H, z + halfW + SIDEWALK_W * 0.5f, 0.0f,
                                pal, meshCache, objects, lights, colliders);
            }

            // Random props on E-W sidewalks
            float propRoll = cityRngFloat(rs);
            DistrictType localDt = districtAt(x, z);

            if (propRoll < 0.10f) {
                placeTrashCan(x + 2.0f, SIDEWALK_H, z - halfW - SIDEWALK_W * 0.7f,
                              1.5708f, pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.16f) {
                placeBollard(x - 1.5f, SIDEWALK_H, z + halfW + SIDEWALK_W * 0.7f,
                             0.0f, pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.22f) {
                placeBench(x + 3.0f, SIDEWALK_H, z - halfW - SIDEWALK_W * 0.6f,
                           1.5708f, pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.27f) {
                placeFireBarrel(x + 1.0f, SIDEWALK_H, z + halfW + SIDEWALK_W * 0.5f,
                                0.0f, pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.32f) {
                float dumpYaw = (cityRng(rs) % 2 == 0) ? 0.0f : 1.5708f;
                placeDumpster(x - 2.0f, SIDEWALK_H, z - halfW - SIDEWALK_W * 0.5f,
                              dumpYaw, pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.36f &&
                       (localDt == DistrictType::Slums ||
                        localDt == DistrictType::Industrial ||
                        localDt == DistrictType::Wasteland)) {
                placeBarricade(x + 2.5f, SIDEWALK_H, z + halfW + SIDEWALK_W * 0.5f,
                               0.0f, pal, meshCache, objects, lights, colliders);
            } else if (propRoll < 0.40f &&
                       (localDt == DistrictType::Slums ||
                        localDt == DistrictType::Wasteland)) {
                uint32_t rubbleSeed = rs ^ (uint32_t)(x * 431.0f);
                placeRubblePile(x + 1.5f, SIDEWALK_H, z - halfW - SIDEWALK_W * 0.5f,
                                rubbleSeed, pal, meshCache, objects, colliders);
            } else if (propRoll < 0.44f) {
                float crateYaw = cityRngFloat(rs) * 3.14159f;
                placeCrate(x - 0.5f, SIDEWALK_H, z + halfW + SIDEWALK_W * 0.3f,
                           crateYaw, pal, meshCache, objects, colliders);
            }
        }
    }

    // ── Vehicle husks on streets ───────────────────────────────
    uint32_t vehSeed = seed ^ 0xDEAD0101u;

    // E-W streets
    for (float z = MINOR_SPACING; z < WORLD_SIZE; z += MINOR_SPACING) {
        for (float x = 16.0f; x < WORLD_SIZE; x += 32.0f) {
            uint32_t vs = vehSeed ^ ((uint32_t)(x * 73.0f) + (uint32_t)(z * 131.0f));
            if (cityRngFloat(vs) < 0.15f) {
                float yaw = cityRngFloat(vs) * 6.2832f;
                const DistrictPalette& pal = paletteAt(x, z);
                placeVehicleHusk(x, 0.0f, z, yaw,
                                 pal, meshCache, objects, colliders);
            }
        }
    }

    // N-S streets — occasional vehicles too
    for (float x = MINOR_SPACING; x < WORLD_SIZE; x += MINOR_SPACING) {
        for (float z = 16.0f; z < WORLD_SIZE; z += 32.0f) {
            uint32_t vs = vehSeed ^ ((uint32_t)(x * 137.0f) + (uint32_t)(z * 89.0f));
            if (cityRngFloat(vs) < 0.10f) {
                float yaw = 1.5708f + cityRngFloat(vs) * 0.4f - 0.2f; // roughly perpendicular
                const DistrictPalette& pal = paletteAt(x, z);
                placeVehicleHusk(x, 0.0f, z, yaw,
                                 pal, meshCache, objects, colliders);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// generateBlock
// ---------------------------------------------------------------------------
void CityLayout::generateBlock(int blockX, int blockZ,
                                float x0, float z0, float x1, float z1,
                                uint32_t seed,
                                MeshCache& meshCache,
                                std::vector<SceneObject>& objects,
                                std::vector<PointLight>& lights,
                                std::vector<AABB>& colliders,
                                std::vector<SlopeCollider>& slopeColliders) {
    // Find the center of the block to determine district
    float midX = (x0 + x1) * 0.5f;
    float midZ = (z0 + z1) * 0.5f;
    DistrictType dt = districtAt(midX, midZ);
    const DistrictPalette& pal = m_palettes[(int)dt];

    // Determine building parameters based on district
    int minFloors, maxFloors;
    float interiorProb;

    switch (dt) {
        case DistrictType::Downtown:
            minFloors = 6;  maxFloors = 20; interiorProb = 0.80f; break;
        case DistrictType::Tech:
            minFloors = 3;  maxFloors = 6;  interiorProb = 0.90f; break;
        case DistrictType::Industrial:
            minFloors = 1;  maxFloors = 3;  interiorProb = 0.85f; break;
        case DistrictType::Slums:
            minFloors = 1;  maxFloors = 4;  interiorProb = 0.75f; break;
        case DistrictType::Residential:
            minFloors = 2;  maxFloors = 4;  interiorProb = 0.85f; break;
        case DistrictType::Wasteland:
            minFloors = 0;  maxFloors = 2;  interiorProb = 0.40f; break;
        default:
            minFloors = 1;  maxFloors = 3;  interiorProb = 0.60f; break;
    }

    // Per-block RNG
    uint32_t blockSeed = seed ^ ((uint32_t)(blockX * 7919) + (uint32_t)(blockZ * 104729));

    // Decide how many buildings in this block (1-3)
    int numBuildings = 1 + (int)(cityRng(blockSeed) % 3);

    // Wasteland blocks sometimes have no buildings
    if (dt == DistrictType::Wasteland && cityRngFloat(blockSeed) < 0.35f) {
        numBuildings = 0;
    }

    float blockW = x1 - x0;
    float blockD = z1 - z0;

    struct Footprint {
        float minX, minZ, maxX, maxZ;
    };
    std::vector<Footprint> buildingFootprints;
    buildingFootprints.reserve(4);

    auto emitBuilding = [&](const BuildingSpec& spec, uint32_t buildSeed) {
        generateBuilding(spec, pal, buildSeed,
                         meshCache, objects, lights, colliders, slopeColliders);
        buildingFootprints.push_back(Footprint{
            spec.x, spec.z,
            spec.x + spec.width,
            spec.z + spec.depth
        });
    };

    if (numBuildings == 1) {
        // Single building fills most of the block
        float margin = 1.0f;
        float bw = blockW - margin * 2.0f;
        float bd = blockD - margin * 2.0f;
        if (bw < 3.0f || bd < 3.0f) return;

        int floors = minFloors + (int)(cityRng(blockSeed) % (uint32_t)(maxFloors - minFloors + 1));
        if (floors < 1 && dt != DistrictType::Wasteland) floors = 1;

        bool interior = cityRngFloat(blockSeed) < interiorProb;

        BuildingSpec spec;
        spec.x = x0 + margin;
        spec.z = z0 + margin;
        spec.width = bw;
        spec.depth = bd;
        spec.floors = floors;
        spec.hasInterior = interior;
        spec.district = dt;
        spec.shape = pickBuildingShape(dt, floors, blockSeed);

        emitBuilding(spec, blockSeed);

    } else if (numBuildings == 2) {
        // Split block along longest axis
        float gap = 1.5f;
        float margin = 1.0f;

        if (blockW >= blockD) {
            // Split along X
            float halfW = (blockW - gap) * 0.5f - margin;
            if (halfW < 3.0f) halfW = 3.0f;
            float bd = blockD - margin * 2.0f;
            if (bd < 3.0f) return;

            for (int i = 0; i < 2; i++) {
                float bx = x0 + margin + i * (halfW + gap + margin);
                int floors = minFloors + (int)(cityRng(blockSeed) % (uint32_t)(maxFloors - minFloors + 1));
                if (floors < 1 && dt != DistrictType::Wasteland) floors = 1;
                bool interior = cityRngFloat(blockSeed) < interiorProb;

                BuildingSpec spec;
                spec.x = bx;
                spec.z = z0 + margin;
                spec.width = std::min(halfW, x1 - bx - margin);
                spec.depth = bd;
                spec.floors = floors;
                spec.hasInterior = interior;
                spec.district = dt;
                spec.shape = pickBuildingShape(dt, floors, blockSeed);

                if (spec.width > 2.0f && spec.depth > 2.0f)
                    emitBuilding(spec, blockSeed + i);
            }
        } else {
            // Split along Z
            float halfD = (blockD - gap) * 0.5f - margin;
            if (halfD < 3.0f) halfD = 3.0f;
            float bw = blockW - margin * 2.0f;
            if (bw < 3.0f) return;

            for (int i = 0; i < 2; i++) {
                float bz = z0 + margin + i * (halfD + gap + margin);
                int floors = minFloors + (int)(cityRng(blockSeed) % (uint32_t)(maxFloors - minFloors + 1));
                if (floors < 1 && dt != DistrictType::Wasteland) floors = 1;
                bool interior = cityRngFloat(blockSeed) < interiorProb;

                BuildingSpec spec;
                spec.x = x0 + margin;
                spec.z = bz;
                spec.width = bw;
                spec.depth = std::min(halfD, z1 - bz - margin);
                spec.floors = floors;
                spec.hasInterior = interior;
                spec.district = dt;
                spec.shape = pickBuildingShape(dt, floors, blockSeed);

                if (spec.width > 2.0f && spec.depth > 2.0f)
                    emitBuilding(spec, blockSeed + i + 100);
            }
        }
    } else {
        // 3 buildings: one large + two small stacked
        float margin = 1.0f;
        float gap = 1.5f;

        // Large building takes ~60% of width
        float largeW = (blockW - gap) * 0.6f - margin;
        float smallW = (blockW - gap) * 0.4f - margin;
        float bd = blockD - margin * 2.0f;
        if (largeW < 3.0f || smallW < 2.5f || bd < 3.0f) {
            // Fall back to 1 building
            numBuildings = 1;
            float bw = blockW - margin * 2.0f;
            if (bw < 3.0f || bd < 3.0f) return;
            int floors = minFloors + (int)(cityRng(blockSeed) % (uint32_t)(maxFloors - minFloors + 1));
            if (floors < 1 && dt != DistrictType::Wasteland) floors = 1;
            BuildingSpec spec;
            spec.x = x0 + margin;
            spec.z = z0 + margin;
            spec.width = bw;
            spec.depth = bd;
            spec.floors = floors;
            spec.hasInterior = cityRngFloat(blockSeed) < interiorProb;
            spec.district = dt;
            spec.shape = pickBuildingShape(dt, floors, blockSeed);
            emitBuilding(spec, blockSeed);
            return;
        }

        // Large building (left side)
        {
            int floors = minFloors + (int)(cityRng(blockSeed) % (uint32_t)(maxFloors - minFloors + 1));
            if (floors < 1 && dt != DistrictType::Wasteland) floors = 1;
            BuildingSpec spec;
            spec.x = x0 + margin;
            spec.z = z0 + margin;
            spec.width = largeW;
            spec.depth = bd;
            spec.floors = floors;
            spec.hasInterior = cityRngFloat(blockSeed) < interiorProb;
            spec.district = dt;
            spec.shape = pickBuildingShape(dt, floors, blockSeed);
            emitBuilding(spec, blockSeed + 200);
        }

        // Two small buildings (right side, stacked vertically)
        float halfD = (bd - gap) * 0.5f;
        float rightX = x0 + margin + largeW + gap;

        for (int i = 0; i < 2; i++) {
            float bz = z0 + margin + i * (halfD + gap);
            int floors = minFloors + (int)(cityRng(blockSeed) % (uint32_t)(maxFloors - minFloors + 1));
            if (floors < 1 && dt != DistrictType::Wasteland) floors = 1;

            BuildingSpec spec;
            spec.x = rightX;
            spec.z = bz;
            spec.width = std::min(smallW, x1 - rightX - margin);
            spec.depth = std::min(halfD, z1 - bz - margin);
            spec.floors = floors;
            spec.hasInterior = cityRngFloat(blockSeed) < interiorProb;
            spec.district = dt;
            spec.shape = pickBuildingShape(dt, floors, blockSeed);

            if (spec.width > 2.0f && spec.depth > 2.0f)
                emitBuilding(spec, blockSeed + 300 + i);
        }
    }

    // ── Alley clutter (props in block margins & gaps) ──────────
    // Scatter crates, rubble, dumpsters, and barrels in the
    // margins around buildings to fill the alleys with life.
    if (numBuildings >= 2 || dt == DistrictType::Slums ||
        dt == DistrictType::Industrial || dt == DistrictType::Wasteland) {

        uint32_t alleySeed = blockSeed ^ 0xA11E7000u;

        // Scatter several props around the block edges
        int numAlleyProps = 2 + (int)(cityRng(alleySeed) % 4);
        for (int i = 0; i < numAlleyProps; ++i) {
            float px = 0.0f;
            float pz = 0.0f;
            bool foundSpot = false;

            // Try several times to avoid spawning blockers right at doors/walls.
            for (int attempt = 0; attempt < 8; ++attempt) {
                px = x0 + 0.5f + cityRngFloat(alleySeed) * (blockW - 1.0f);
                pz = z0 + 0.5f + cityRngFloat(alleySeed) * (blockD - 1.0f);

                bool tooCloseToBuilding = false;
                for (const auto& fp : buildingFootprints) {
                    const float pad = 1.8f;
                    if (px >= fp.minX - pad && px <= fp.maxX + pad &&
                        pz >= fp.minZ - pad && pz <= fp.maxZ + pad) {
                        tooCloseToBuilding = true;
                        break;
                    }
                }
                if (!tooCloseToBuilding) {
                    foundSpot = true;
                    break;
                }
            }
            if (!foundSpot) continue;

            float yaw = cityRngFloat(alleySeed) * 6.2832f;
            float roll = cityRngFloat(alleySeed);

            if (roll < 0.30f) {
                placeCrate(px, 0.0f, pz, yaw, pal, meshCache, objects, colliders);
            } else if (roll < 0.55f) {
                placeRubblePile(px, 0.0f, pz, alleySeed + i,
                                pal, meshCache, objects, colliders);
            } else if (roll < 0.75f) {
                placeDumpster(px, 0.0f, pz, yaw,
                              pal, meshCache, objects, lights, colliders);
            } else {
                placeFireBarrel(px, 0.0f, pz, 0.0f,
                                pal, meshCache, objects, lights, colliders);
            }
        }
    }
}
