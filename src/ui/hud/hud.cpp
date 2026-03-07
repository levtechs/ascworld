#include "ui/hud/hud.h"
#include "ui/hud/crosshair.h"
#include "ui/hud/hotbar.h"
#include "ui/hud/cooldown_indicator.h"
#include "ui/hud/pickup_prompt.h"
#include "ui/hud/health_bar.h"
#include "ui/hud/kill_counter.h"
#include "game/player.h"
#include "game/entity.h"

void renderHUD(int screenW, int screenH, const Player& player, float gameTime,
               const WorldEntity* nearbyItem, float health, float maxHealth,
               int kills, int deaths) {
    (void)gameTime;
    renderCrosshair(screenW, screenH);
    renderHealthBar(screenW, screenH, health, maxHealth);
    renderKillCounter(screenW, screenH, kills, deaths);
    renderHotbar(screenW, screenH, player.inventory());
    renderCooldownIndicator(screenW, screenH, player.inventory());
    renderPickupPrompt(screenW, screenH, nearbyItem);
}
