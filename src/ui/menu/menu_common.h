#pragma once
#include "input/input.h"
#include <string>
#include <cstring>
#include <cstdio>

// ---- Menu result: what action the user chose this frame ----
enum class MenuResult {
    None,           // Still in menu, no action
    Continue,       // Resume current game (offline)
    NewGame,        // Start new offline game
    LoadWorld,      // Load a saved world
    Quit,           // Quit application
    OnlinePlay,     // User chose online play / confirmed username
    OfflinePlay,    // User chose offline play
    HostGame,       // User wants to host
    JoinGame,       // User wants to join a room
    Back,           // Navigate back one screen
    Disconnect,     // Cancel connection / disconnect
};

// ---- Base class for all menu screens ----
class MenuScreen {
public:
    virtual ~MenuScreen() = default;
    virtual MenuResult update(InputState& input, int screenW, int screenH) = 0;
    virtual void render(int screenW, int screenH) const = 0;

    // ---- Shared ANSI color escape sequences (public for use by static screens) ----
    static constexpr const char* BG         = "\033[48;2;8;10;18m";
    static constexpr const char* TITLE_FG   = "\033[38;2;180;170;140m";
    static constexpr const char* SUBTITLE_FG= "\033[38;2;100;110;130m";
    static constexpr const char* SELECTED_FG= "\033[38;2;255;230;180m";
    static constexpr const char* SELECTED_BG= "\033[48;2;30;35;50m";
    static constexpr const char* NORMAL_FG  = "\033[38;2;120;120;130m";
    static constexpr const char* HINT_FG    = "\033[38;2;60;65;80m";
    static constexpr const char* DETAIL_FG  = "\033[38;2;80;85;100m";
    static constexpr const char* ACCENT_FG  = "\033[38;2;140;180;120m";
    static constexpr const char* INPUT_FG   = "\033[38;2;255;230;180m";
    static constexpr const char* INPUT_BG   = "\033[48;2;25;28;40m";

    // ---- ASCII art title (shared across several screens) ----
    static constexpr int TITLE_LINES = 4;
    static constexpr int TITLE_WIDTH = 41;
    static void renderTitle(std::string& output, int screenW, int startY, int y);

    // ---- Common render helpers ----
    static void renderCenteredText(std::string& output, const char* text, int screenW,
                                   const char* fgColor, const char* bgColor);
    static void renderEmptyLine(std::string& output, int screenW, const char* bgColor);

    // Begin a full-screen output buffer
    static void beginOutput(std::string& output, int screenW, int screenH) {
        output.clear();
        output.reserve(screenW * screenH * 2);
        output += "\033[H";
    }

    // Flush a full-screen output buffer
    static void flushOutput(const std::string& output) {
        fwrite(output.c_str(), 1, output.size(), stdout);
        fflush(stdout);
    }

    // End-of-line handling
    static void endLine(std::string& output, int y, int screenH) {
        if (y < screenH - 1) output += "\r\n";
    }

    // Process press queue and return flags
    struct PressFlags {
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool confirm = false;
        bool back = false;
    };
    static PressFlags consumePressFlags(InputState& input);
};
