#include "ui/hud/pickup_prompt.h"
#include "game/entity.h"
#include "game/item.h"
#include <cstdio>
#include <cstring>

static const char* COL_PROMPT = "\033[38;2;255;220;100m"; // warm gold
static const char* COL_KEY    = "\033[38;2;180;255;180m"; // green for key hint
static const char* COL_RESET  = "\033[0m";

void renderPickupPrompt(int screenW, int screenH, const WorldEntity* nearbyItem) {
    if (!nearbyItem) return;

    const auto* d = std::get_if<DroppedItemData>(&nearbyItem->data);
    if (!d) return;

    const auto& def = getItemDef(d->itemType);

    // Build prompt string: "[G] Pick up Saber"
    char buf[64];
    snprintf(buf, sizeof(buf), "[G] Pick up %s", def.name);

    int len = static_cast<int>(strlen(buf));
    int col = (screenW - len) / 2;
    int row = screenH - 6; // above cooldown bar
    if (row < 1) row = 1;

    // Print key hint in green, rest in gold
    printf("\033[%d;%dH%s[G]%s Pick up %s%s",
           row, col + 1,
           COL_KEY, COL_PROMPT,
           def.name,
           COL_RESET);
}
