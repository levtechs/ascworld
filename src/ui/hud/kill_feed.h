#pragma once
#include "game/entity.h"
#include <string>
#include <vector>

struct KillFeedEntry {
    std::string killerName;
    std::string victimName;
    EntityType weaponType = EntityType::None;
    float timeRemaining = 0.0f;  // seconds left to show
};

class KillFeed {
public:
    KillFeed() = default;

    // Add a new kill event
    void addKill(const std::string& killer, const std::string& victim, EntityType weapon);

    // Tick timers, remove expired entries
    void update(float dt);

    // Render in top-right corner
    void render(int screenW, int screenH) const;

private:
    std::vector<KillFeedEntry> m_entries;
    static constexpr int MAX_ENTRIES = 3;
    static constexpr float DISPLAY_TIME = 5.0f;
};
