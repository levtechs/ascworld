#include "ui/hud/kill_counter.h"
#include <cstdio>

void renderKillCounter(int screenW, int screenH, int kills, int deaths) {
    (void)screenH;
    const char* colTxt = "\033[38;2;200;205;215m";
    const char* colK   = "\033[38;2;90;210;95m";
    const char* colD   = "\033[38;2;230;90;75m";
    const char* reset  = "\033[0m";

    // Format: "K:2 D:1" in top-right corner
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "K:%d D:%d", kills, deaths);
    int col = screenW - len - 1;
    if (col < 1) col = 1;

    printf("\033[1;%dH%sK:%s%d %sD:%s%d%s",
           col, colTxt, colK, kills, colTxt, colD, deaths, reset);
}
