#pragma once
#include "ui/menu/menu_common.h"

// Static-only screen (no interaction, just rendering)
class GeneratingScreen {
public:
    static void render(int screenW, int screenH, int chunksGenerated, int totalChunks);
};
