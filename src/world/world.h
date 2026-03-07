#pragma once
#include "core/chunk.h"
#include "core/noise.h"
#include "rendering/mesh.h"
#include "rendering/renderer.h"
#include "game/player.h"
#include "world/city_layout.h"
#include <vector>
#include <deque>
#include <unordered_map>
#include <cstdint>

class World {
public:
    static constexpr float CHUNK_SIZE = 16.0f;
    static constexpr int WORLD_CHUNKS = 16;

    World();
    void generate(uint32_t seed);
    void clear();

    const std::vector<SceneObject>& objects() const { return m_objects; }
    const std::vector<PointLight>& lights() const { return m_lights; }
    const LightGrid& lightGrid() const { return m_lightGrid; }
    const std::vector<AABB>& colliders() const { return m_colliders; }
    const std::vector<SlopeCollider>& slopes() const { return m_slopes; }

    // Terrain queries (flat city)
    float terrainHeightAt(float wx, float wz) const { return 0.0f; }
    float effectiveHeightAt(float wx, float wz) const { return 0.0f; }
    Vec3 terrainNormalAt(float wx, float wz) const { return Vec3(0, 1, 0); }
    bool isUnderwater(float wx, float wz) const { return false; }

    float waterLevel() const { return -1.0f; }
    float worldSize() const { return CHUNK_SIZE * WORLD_CHUNKS; }

    // City-specific queries
    const CityLayout& cityLayout() const { return m_cityLayout; }

private:
    CityLayout m_cityLayout;
    MeshCache m_meshCache;
    std::deque<Mesh> m_meshes;    // legacy storage (still used for unique meshes)
    std::vector<SceneObject> m_objects;
    std::vector<PointLight> m_lights;
    LightGrid m_lightGrid;
    std::vector<AABB> m_colliders;
    std::vector<SlopeCollider> m_slopes;
};
