#include "ui/hud/hud.h"
#include "ui/hud/crosshair.h"
#include "ui/hud/hotbar.h"
#include "ui/hud/cooldown_indicator.h"
#include "ui/hud/pickup_prompt.h"
#include "ui/hud/health_bar.h"
#include "ui/hud/kill_counter.h"
#include "game/player.h"
#include "game/entity.h"
#include <string>
#include <cstdio>

void renderHUD(int screenW, int screenH, const Player& player, float gameTime,
               const WorldEntity* nearbyItem, float health, float maxHealth,
               int kills, int deaths, int netPeerCount, bool isHost,
               const char* worldName) {
    (void)gameTime;
    renderCrosshair(screenW, screenH);
    renderHealthBar(screenW, screenH, health, maxHealth);
    renderKillCounter(screenW, screenH, kills, deaths);
    renderHotbar(screenW, screenH, player.inventory());
    renderCooldownIndicator(screenW, screenH, player.inventory());
    renderPickupPrompt(screenW, screenH, nearbyItem);
    
    // Multiplayer/world info - top right
    std::string info;
    if (worldName && worldName[0]) {
        info += worldName;
        info += " ";
    }
    if (netPeerCount > 0 || isHost) {
        std::string mode = isHost ? "HOST" : "CLIENT";
        int totalPlayers = netPeerCount + 1;
        info += "[" + mode + "] " + std::to_string(totalPlayers) + "P";
    }
    if (!info.empty()) {
        int x = screenW - (int)info.size() - 1;
        if (x < 1) x = 1;
        printf("\033[%d;%dH\033[38;5;214m%s\033[0m", 1, x, info.c_str());
    }
}
