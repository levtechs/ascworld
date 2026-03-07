#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// Key press events for menus (one-shot, never missed)
enum class KeyPress {
    Up,         // Arrow Up
    Down,       // Arrow Down
    Left,       // Arrow Left
    Right,      // Arrow Right
    Confirm,    // Enter / Space
    Back,       // Esc
};

struct InputState {
    // Movement keys — true while physically held (for gameplay)
    std::atomic<bool> forward{false};   // W
    std::atomic<bool> backward{false};  // S
    std::atomic<bool> left{false};      // A
    std::atomic<bool> right{false};     // D
    std::atomic<bool> up{false};        // Space
    std::atomic<bool> down{false};      // C

    // Mouse delta (accumulated since last frame)
    float mouseDx = 0;
    float mouseDy = 0;

    // Quit signal (only set from menu)
    std::atomic<bool> quit{false};

    // Focus state — updated by Input::poll() via terminal focus events
    std::atomic<bool> focused{true};
    // Set to true once when focus is lost (one-shot, consumed by game loop)
    std::atomic<bool> focusLost{false};

    // --- Key press event queue (for reliable menu input) ---
    // These are one-shot events that can never be missed.
    // The CGEventTap pushes on keyDown, menu code consumes.
    std::mutex pressMutex;
    std::vector<KeyPress> pressQueue;

    void pushPress(KeyPress k) {
        std::lock_guard<std::mutex> lock(pressMutex);
        pressQueue.push_back(k);
    }

    std::vector<KeyPress> consumePresses() {
        std::lock_guard<std::mutex> lock(pressMutex);
        std::vector<KeyPress> out = std::move(pressQueue);
        pressQueue.clear();
        return out;
    }

    // --- Text input mode ---
    // When true, the event tap captures printable characters into typedChars
    // instead of using them for movement (W/A/S/D etc.)
    std::atomic<bool> textInputMode{false};

    // Typed characters buffer (protected by textMutex)
    std::mutex textMutex;
    std::string typedChars;     // chars typed since last consumed
    int backspaceCount = 0;     // backspaces pressed since last consumed
    bool enterPressed = false;  // enter/return pressed since last consumed

    // Consume all pending text input (clears the buffers)
    void consumeTextInput(std::string& outChars, int& outBackspaces, bool& outEnter) {
        std::lock_guard<std::mutex> lock(textMutex);
        outChars = std::move(typedChars);
        typedChars.clear();
        outBackspaces = backspaceCount;
        backspaceCount = 0;
        outEnter = enterPressed;
        enterPressed = false;
    }
};

class Input {
public:
    Input();
    ~Input();

    // Call once per frame: reads terminal focus events, collects mouse deltas.
    // Keyboard is handled async via the CGEventTap.
    void poll(InputState& state);

    // Start the OS-level event tap for keyboard capture.
    // Call this after setting m_sharedState.
    void startCapture();

    // Enable/disable mouse grab (warping + hiding cursor for FPS gameplay)
    void enableMouse();
    void disableMouse();

    // Enable/disable mouse capture (warping + hiding cursor).
    // The event tap runs always for keyboard; this controls mouse grab.
    void setMouseCapture(bool capture);
    bool mouseCapture() const { return m_mouseCapture; }

    // Public for C callback access
    void accumulateMouseDelta(float dx, float dy);
    void* m_eventTap = nullptr;

    // Shared input state pointer so the event tap callback can update keys
    InputState* m_sharedState = nullptr;

    // Focus state — updated by poll() reading terminal focus events (\033[I / \033[O).
    // Read by the CGEventTap callback to suppress input when unfocused.
    // Public because the C callback function needs to read it.
    std::atomic<bool> m_focused{true};

private:
    void enableRawMode();
    void restoreTerminal();

    // Parse stdin for terminal focus escape sequences and other buffered input.
    // Returns true if focus changed.
    void readTerminalEvents(InputState& state);

    bool m_rawMode = false;
    bool m_mouseEnabled = false;
    bool m_mouseCapture = false;
    bool m_cursorShownForUnfocus = false;

    // Raw mouse delta accumulator (written by CGEventTap callback on a background thread)
    std::atomic<float> m_rawDx{0};
    std::atomic<float> m_rawDy{0};

    // Opaque handle to the run loop source
    void* m_runLoopSource = nullptr;
    void* m_tapThread = nullptr;

    bool m_tapStarted = false;
    void startEventTap();
    void stopEventTap();

    friend void* mouseThreadFunc(void* arg);
};

// Install signal handlers to ensure terminal cleanup on crash/interrupt
void installSignalHandlers();
