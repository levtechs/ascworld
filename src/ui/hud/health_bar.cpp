#include "ui/hud/health_bar.h"
#include <cstdio>
#include <algorithm>

void renderHealthBar(int screenW, int screenH, float health, float maxHealth) {
    if (maxHealth <= 0.0f) return;
    float frac = health / maxHealth;
    frac = std::max(0.0f, std::min(1.0f, frac));

    (void)screenH;
    const int barWidth = 18;
    int row = 1;
    int colPos = 2;

    const char* colGood = "\033[38;2;90;210;95m";
    const char* colMid  = "\033[38;2;235;195;85m";
    const char* colLow  = "\033[38;2;230;90;75m";
    const char* colDim  = "\033[38;2;70;72;80m";
    const char* colTxt  = "\033[38;2;220;225;235m";
    const char* reset   = "\033[0m";

    const char* barCol = colGood;
    if (frac < 0.35f) barCol = colLow;
    else if (frac < 0.65f) barCol = colMid;

    int fill = static_cast<int>(frac * barWidth + 0.5f);
    if (fill < 0) fill = 0;
    if (fill > barWidth) fill = barWidth;

    printf("\033[%d;%dH%sHP %3d [", row, colPos, colTxt, static_cast<int>(health + 0.5f));
    for (int i = 0; i < barWidth; ++i) {
        if (i < fill) printf("%s#", barCol);
        else printf("%s-", colDim);
    }
    printf("%s]%s", colTxt, reset);
}
