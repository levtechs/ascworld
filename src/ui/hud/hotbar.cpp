#include "ui/hud/hotbar.h"
#include "game/inventory.h"
#include "game/item.h"
#include <cstdio>
#include <cstring>

// Colors (ANSI truecolor)
static const char* COL_BORDER    = "\033[38;2;120;120;140m"; // muted gray border
static const char* COL_SELECTED  = "\033[38;2;255;220;100m"; // warm gold for selected
static const char* COL_ITEM_NAME = "\033[38;2;220;220;230m"; // bright white-ish
static const char* COL_ITEM_DIM  = "\033[38;2;100;100;110m"; // dim for empty slots
static const char* COL_COUNT     = "\033[38;2;180;200;180m"; // muted green for count
static const char* COL_RESET     = "\033[0m";

void renderHotbar(int screenW, int screenH, const Inventory& inv) {
    // We display up to HOTBAR_DISPLAY slots (all that have items + selected area)
    // Find the last occupied slot to determine visible width
    int lastOccupied = 0;
    for (int i = 0; i < Inventory::HOTBAR_SIZE; ++i) {
        if (!inv.getSlot(i).empty()) lastOccupied = i;
    }
    // Show at least slots up to max(lastOccupied, selectedSlot) + 1, min 3
    int visibleSlots = std::max(3, std::max(lastOccupied, inv.selectedSlot()) + 1);
    if (visibleSlots > Inventory::HOTBAR_SIZE) visibleSlots = Inventory::HOTBAR_SIZE;

    // Each slot is 5 chars wide: [XXX]
    // Total width: visibleSlots * 5 + 1 (for closing bracket)
    int slotWidth = 5;
    int totalWidth = visibleSlots * slotWidth + 1;
    int startCol = (screenW - totalWidth) / 2;
    if (startCol < 0) startCol = 0;

    // Position: 3 rows from bottom
    int row = screenH - 2;
    if (row < 1) row = 1;

    // Row 1: top border
    // Row 2: item names (3-char abbreviation)
    // Row 3: bottom border with selection indicator

    // --- Top border ---
    printf("\033[%d;%dH", row, startCol + 1);
    for (int i = 0; i < visibleSlots; ++i) {
        bool sel = (i == inv.selectedSlot());
        printf("%s%s", sel ? COL_SELECTED : COL_BORDER,
               i == 0 ? "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" : // ┌───
                         "\xe2\x94\xac\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"); // ┬───
    }
    printf("%s\xe2\x94\x90%s", COL_BORDER, COL_RESET); // ┐

    // --- Item row ---
    printf("\033[%d;%dH", row + 1, startCol + 1);
    for (int i = 0; i < visibleSlots; ++i) {
        bool sel = (i == inv.selectedSlot());
        const auto& slot = inv.getSlot(i);

        printf("%s\xe2\x94\x82", sel ? COL_SELECTED : COL_BORDER); // │

        if (!slot.empty()) {
            const auto& def = slot.def();
            // Show 3-char short name
            printf("%s%s", sel ? COL_SELECTED : COL_ITEM_NAME, def.shortName);
        } else {
            printf("%s   ", COL_ITEM_DIM); // empty
        }
    }
    printf("%s\xe2\x94\x82%s", COL_BORDER, COL_RESET); // │

    // --- Bottom border ---
    printf("\033[%d;%dH", row + 2, startCol + 1);
    for (int i = 0; i < visibleSlots; ++i) {
        bool sel = (i == inv.selectedSlot());
        printf("%s%s", sel ? COL_SELECTED : COL_BORDER,
               i == 0 ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80" : // └───
                         "\xe2\x94\xb4\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"); // ┴───
    }
    printf("%s\xe2\x94\x98%s", COL_BORDER, COL_RESET); // ┘

    // --- Selection indicator (▲ under selected slot) ---
    // First clear the entire indicator row to remove stale arrows/counts
    {
        printf("\033[%d;%dH", row + 3, startCol + 1);
        for (int i = 0; i < totalWidth; i++) printf(" ");
    }
    {
        int selCol = startCol + inv.selectedSlot() * slotWidth + 2; // center of slot
        printf("\033[%d;%dH%s\xe2\x96\xb2%s", row + 3, selCol + 1, COL_SELECTED, COL_RESET);
    }

    // --- Stack count for selected item (if stackable and count > 1) ---
    {
        const auto& slot = inv.getSlot(inv.selectedSlot());
        if (!slot.empty() && slot.count > 1) {
            int selCol = startCol + inv.selectedSlot() * slotWidth + 3;
            printf("\033[%d;%dH%sx%d%s", row + 3, selCol + 1, COL_COUNT, slot.count, COL_RESET);
        }
    }
}
