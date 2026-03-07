#include "world/world.h"

World::World() {}

void World::clear() {
    m_meshes.clear();
    m_objects.clear();
    m_lights.clear();
    m_lightGrid.clear();
    m_colliders.clear();
    m_slopes.clear();
}

void World::generate(uint32_t seed) {
    clear();
    m_objects.reserve(50000);
    m_colliders.reserve(20000);
    m_lights.reserve(5000);
    m_cityLayout.generate(seed, m_meshCache, m_objects, m_lights, m_colliders, m_slopes);

    // Build spatial light grid once (lights are static after world gen)
    m_lightGrid.reserve(m_lights.size());
    Renderer::buildLightGrid(m_lights, LIGHT_CELL_SIZE, m_lightGrid);
}
