#include "ui/hud/kill_feed.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

static const char* weaponVerb(EntityType type) {
    switch (type) {
        case EntityType::LaserBeam:  return "shot";
        case EntityType::SaberSwing: return "sliced";
        case EntityType::Explosion:  return "blew up";
        default:                     return "killed";
    }
}

static const char* weaponColor(EntityType type) {
    switch (type) {
        case EntityType::LaserBeam:  return "\033[38;2;255;120;80m";   // orange-red
        case EntityType::SaberSwing: return "\033[38;2;100;220;255m";  // cyan
        case EntityType::Explosion:  return "\033[38;2;255;200;60m";   // yellow
        default:                     return "\033[38;2;180;180;180m";  // gray
    }
}

void KillFeed::addKill(const std::string& killer, const std::string& victim, EntityType weapon) {
    KillFeedEntry entry;
    entry.killerName = killer;
    entry.victimName = victim;
    entry.weaponType = weapon;
    entry.timeRemaining = DISPLAY_TIME;

    // Insert at front (newest first)
    m_entries.insert(m_entries.begin(), entry);

    // Trim to max entries
    if (static_cast<int>(m_entries.size()) > MAX_ENTRIES) {
        m_entries.resize(MAX_ENTRIES);
    }
}

void KillFeed::update(float dt) {
    for (auto& e : m_entries) {
        e.timeRemaining -= dt;
    }
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
            [](const KillFeedEntry& e) { return e.timeRemaining <= 0.0f; }),
        m_entries.end());
}

void KillFeed::render(int screenW, int screenH) const {
    (void)screenH;
    if (m_entries.empty()) return;

    const char* nameFg = "\033[38;2;220;220;230m";
    const char* reset   = "\033[0m";

    int row = 2;  // start at row 2 (below health bar area)
    for (const auto& e : m_entries) {
        // Build message: "killer verb victim"
        char msg[128];
        snprintf(msg, sizeof(msg), "%s %s %s",
                 e.killerName.c_str(), weaponVerb(e.weaponType), e.victimName.c_str());

        int msgLen = static_cast<int>(strlen(msg));
        int col = screenW - msgLen - 1;
        if (col < 1) col = 1;

        // Fade effect: dim when close to expiring
        float alpha = std::min(1.0f, e.timeRemaining / 1.0f);  // fade in last second
        int brightness = static_cast<int>(180 * alpha + 40);

        // Render with weapon-colored verb
        printf("\033[%d;%dH", row, col);
        // Killer name
        printf("\033[38;2;%d;%d;%dm%s ",
               brightness, brightness, static_cast<int>(brightness * 0.95f),
               e.killerName.c_str());
        // Verb (weapon colored)
        const char* vc = weaponColor(e.weaponType);
        printf("%s%s ", vc, weaponVerb(e.weaponType));
        // Victim name
        printf("\033[38;2;%d;%d;%dm%s%s",
               brightness, brightness, static_cast<int>(brightness * 0.95f),
               e.victimName.c_str(), reset);

        row++;
    }
}
