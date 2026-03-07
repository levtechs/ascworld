#include "ui/hud/crosshair.h"
#include <cstdio>

void renderCrosshair(int screenW, int screenH) {
    int cx = screenW / 2;
    int cy = screenH / 2;

    // ANSI: move cursor to center, print bright white '+'
    // Row/col are 1-indexed in ANSI escape sequences
    printf("\033[%d;%dH\033[97m+\033[0m", cy + 1, cx + 1);
}
