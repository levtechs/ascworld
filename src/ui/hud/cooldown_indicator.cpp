#include "ui/hud/cooldown_indicator.h"
#include "game/inventory.h"
#include <cstdio>

static const char* COL_READY   = "\033[38;2;80;200;80m";   // green
static const char* COL_COOLING = "\033[38;2;200;80;60m";    // red
static const char* COL_MID     = "\033[38;2;220;180;60m";   // yellow
static const char* COL_DIM     = "\033[38;2;80;80;90m";     // dim gray
static const char* COL_RESET   = "\033[0m";

void renderCooldownIndicator(int screenW, int screenH, const Inventory& inv) {
    // Only show if active item has a cooldown and is currently cooling
    const auto& slot = inv.activeItem();
    if (slot.empty()) return;

    float frac = inv.getCooldownFraction();
    if (frac <= 0.0f) return; // ready, don't show

    // Bar width: 9 chars (matching roughly one hotbar slot width)
    const int barWidth = 9;
    int startCol = (screenW - barWidth) / 2;
    int row = screenH - 5; // above the hotbar
    if (row < 1) row = 1;

    // Pick color based on fraction remaining
    const char* col = COL_COOLING;
    if (frac < 0.3f) col = COL_READY;
    else if (frac < 0.6f) col = COL_MID;

    int filled = static_cast<int>((1.0f - frac) * barWidth);
    if (filled < 0) filled = 0;
    if (filled > barWidth) filled = barWidth;

    printf("\033[%d;%dH%s[", row, startCol + 1, col);
    for (int i = 0; i < barWidth; ++i) {
        if (i < filled)
            printf("\xe2\x96\x88"); // █ (filled)
        else
            printf("%s\xe2\x96\x91%s", COL_DIM, col); // ░ (empty)
    }
    printf("]%s", COL_RESET);
}
