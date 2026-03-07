#include "ui/menu/generating.h"

void GeneratingScreen::render(int screenW, int screenH, int chunksGenerated, int totalChunks) {
    std::string output;
    output.reserve(screenW * screenH * 2);
    output += "\033[H";

    const char* bg = "\033[48;2;8;10;18m";

    int centerY = screenH / 2;

    int barWidth = 30;
    if (barWidth > screenW - 10) barWidth = screenW - 10;
    float progress = (float)chunksGenerated / (float)totalChunks;
    int filled = (int)(progress * barWidth);

    std::string barStr = "[";
    for (int i = 0; i < barWidth; i++)
        barStr += (i < filled) ? '#' : '.';
    barStr += "]";

    char pctBuf[32];
    snprintf(pctBuf, sizeof(pctBuf), " %d%%", (int)(progress * 100));
    barStr += pctBuf;

    for (int y = 0; y < screenH; y++) {
        if (y == centerY - 1) {
            MenuScreen::renderCenteredText(output, "Generating World...", screenW,
                                           "\033[38;2;180;170;140m", bg);
        } else if (y == centerY + 1) {
            MenuScreen::renderCenteredText(output, barStr.c_str(), screenW,
                                           "\033[38;2;140;180;120m", bg);
        } else {
            MenuScreen::renderEmptyLine(output, screenW, bg);
        }
        if (y < screenH - 1) output += "\r\n";
    }

    output += "\033[0m";
    fwrite(output.c_str(), 1, output.size(), stdout);
    fflush(stdout);
}
