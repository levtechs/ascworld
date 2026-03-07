#include "input/input.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <pthread.h>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>  // for kVK_ key codes
#endif

static struct termios g_origTermios;
static bool g_origSaved = false;

static void restoreTerminalGlobal() {
    if (g_origSaved) {
        // Disable focus reporting before restoring terminal
        write(STDOUT_FILENO, "\033[?1004l", 8);
        write(STDOUT_FILENO, "\033[?25h", 6);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_origTermios);
    }
#ifdef __APPLE__
    CGDisplayShowCursor(CGMainDisplayID());
#endif
}

static void signalHandler(int) {
    restoreTerminalGlobal();
    _exit(1);
}

void installSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGSEGV, &sa, nullptr);
}

// ============================================================
// CGEventTap: captures both mouse + keyboard at OS level
// ============================================================

#ifdef __APPLE__

static Input* g_inputInstance = nullptr;
static CGPoint g_screenCenter = {0, 0};

static CGEventRef eventCallback(CGEventTapProxy, CGEventType type, CGEventRef event, void*) {
    if (!g_inputInstance) return event;

    // Re-enable tap if OS disabled it
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        if (g_inputInstance->m_eventTap) {
            CGEventTapEnable((CFMachPortRef)g_inputInstance->m_eventTap, true);
        }
        return event;
    }

    // ---- FOCUS GATE ----
    // Only process input when this instance's terminal window is focused.
    // Focus state is determined by terminal escape sequences (\033[I / \033[O)
    // which are per-window, unlike CGEventTap which is system-wide.
    bool isFocused = g_inputInstance->m_focused.load(std::memory_order_relaxed);

    // Mouse movement - only capture when focused AND mouse capture is active
    if (type == kCGEventMouseMoved || type == kCGEventLeftMouseDragged ||
        type == kCGEventRightMouseDragged || type == kCGEventOtherMouseDragged) {
        if (isFocused && g_inputInstance->mouseCapture()) {
            double dx = CGEventGetDoubleValueField(event, kCGMouseEventDeltaX);
            double dy = CGEventGetDoubleValueField(event, kCGMouseEventDeltaY);
            g_inputInstance->accumulateMouseDelta((float)dx, (float)dy);
            CGWarpMouseCursorPosition(g_screenCenter);
        }
        return event;
    }

    // Keyboard - only process when focused
    if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        if (!isFocused) return event; // Ignore all keyboard input when unfocused

        bool pressed = (type == kCGEventKeyDown);
        int keyCode = (int)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        InputState* state = g_inputInstance->m_sharedState;
        if (state) {
            // Text input mode: capture typed characters instead of game controls
            if (state->textInputMode.load(std::memory_order_relaxed) && pressed) {
                // Escape always exits text input (pushed as Back event)
                if (keyCode == kVK_Escape) {
                    state->pushPress(KeyPress::Back);
                    return event;
                }
                // Enter/Return
                if (keyCode == kVK_Return) {
                    std::lock_guard<std::mutex> lock(state->textMutex);
                    state->enterPressed = true;
                    return event;
                }
                // Backspace/Delete
                if (keyCode == kVK_Delete) {
                    std::lock_guard<std::mutex> lock(state->textMutex);
                    state->backspaceCount++;
                    return event;
                }
                // Get the actual character from the event
                UniChar chars[4];
                UniCharCount actualCount = 0;
                CGEventKeyboardGetUnicodeString(event, 4, &actualCount, chars);
                if (actualCount > 0 && chars[0] >= 32 && chars[0] < 127) {
                    char c = (char)chars[0];
                    // Convert to uppercase
                    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
                    std::lock_guard<std::mutex> lock(state->textMutex);
                    state->typedChars += c;
                }
                return event;
            }

            // Update held-state atomics (for gameplay movement only)
            switch (keyCode) {
            case kVK_ANSI_W:  state->forward.store(pressed, std::memory_order_relaxed);  break;
            case kVK_ANSI_S:  state->backward.store(pressed, std::memory_order_relaxed); break;
            case kVK_ANSI_A:  state->left.store(pressed, std::memory_order_relaxed);     break;
            case kVK_ANSI_D:  state->right.store(pressed, std::memory_order_relaxed);    break;
            case kVK_Space:   state->up.store(pressed, std::memory_order_relaxed);       break;
            case kVK_ANSI_C:  state->down.store(pressed, std::memory_order_relaxed);     break;
            }

            // Push one-shot press events for menu use (arrow keys + Enter + Esc)
            if (pressed) {
                switch (keyCode) {
                case kVK_UpArrow:    state->pushPress(KeyPress::Up);      break;
                case kVK_DownArrow:  state->pushPress(KeyPress::Down);    break;
                case kVK_LeftArrow:  state->pushPress(KeyPress::Left);    break;
                case kVK_RightArrow: state->pushPress(KeyPress::Right);   break;
                case kVK_Return:     state->pushPress(KeyPress::Confirm); break;
                case kVK_Space:      state->pushPress(KeyPress::Confirm); break;
                case kVK_Escape:     state->pushPress(KeyPress::Back);    break;
                }
            }
        }
        return event;
    }

    return event;
}

void* mouseThreadFunc(void* arg) {
    Input* self = (Input*)arg;

    CGEventMask mask = CGEventMaskBit(kCGEventMouseMoved) |
                       CGEventMaskBit(kCGEventLeftMouseDragged) |
                       CGEventMaskBit(kCGEventRightMouseDragged) |
                       CGEventMaskBit(kCGEventOtherMouseDragged) |
                       CGEventMaskBit(kCGEventKeyDown) |
                       CGEventMaskBit(kCGEventKeyUp);

    CFMachPortRef tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionListenOnly,
        mask,
        eventCallback,
        nullptr
    );

    if (!tap) {
        fprintf(stderr, "Warning: Could not create event tap. "
                "Grant accessibility permissions in System Settings > "
                "Privacy & Security > Input Monitoring.\n");
        return nullptr;
    }

    self->m_eventTap = (void*)tap;

    CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    self->m_runLoopSource = (void*)source;

    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);

    // Compute screen center for warping
    CGDirectDisplayID display = CGMainDisplayID();
    g_screenCenter = {
        (CGFloat)(CGDisplayPixelsWide(display) / 2),
        (CGFloat)(CGDisplayPixelsHigh(display) / 2)
    };

    CFRunLoopRun();
    return nullptr;
}

void Input::startEventTap() {
    g_inputInstance = this;
    m_rawDx.store(0, std::memory_order_relaxed);
    m_rawDy.store(0, std::memory_order_relaxed);

    pthread_t thread;
    pthread_create(&thread, nullptr, mouseThreadFunc, this);
    pthread_detach(thread);
    m_tapThread = (void*)(uintptr_t)thread;
}

void Input::stopEventTap() {
    if (m_eventTap) {
        CGEventTapEnable((CFMachPortRef)m_eventTap, false);
    }
    if (m_runLoopSource) {
        CFRunLoopSourceInvalidate((CFRunLoopSourceRef)m_runLoopSource);
        CFRelease((CFRunLoopSourceRef)m_runLoopSource);
        m_runLoopSource = nullptr;
    }
    if (m_eventTap) {
        CFRelease((CFMachPortRef)m_eventTap);
        m_eventTap = nullptr;
    }
    CGDisplayShowCursor(CGMainDisplayID());
    g_inputInstance = nullptr;
}

#else
void Input::startEventTap() {}
void Input::stopEventTap() {}
#endif

// ============================================================
// Terminal raw mode
// ============================================================

Input::Input() {
    enableRawMode();
}

Input::~Input() {
    setMouseCapture(false);
    stopEventTap();
    restoreTerminal();
}

void Input::enableRawMode() {
    if (m_rawMode) return;

    tcgetattr(STDIN_FILENO, &g_origTermios);
    g_origSaved = true;

    struct termios raw = g_origTermios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    write(STDOUT_FILENO, "\033[?25l", 6);    // Hide cursor
    write(STDOUT_FILENO, "\033[?1004h", 8);  // Enable terminal focus reporting
    m_rawMode = true;
}

void Input::restoreTerminal() {
    if (!m_rawMode) return;
    restoreTerminalGlobal();
    m_rawMode = false;
}

void Input::startCapture() {
    if (m_tapStarted) return;
    startEventTap();
    m_tapStarted = true;
}

void Input::enableMouse() {
    if (m_mouseEnabled) return;
    setMouseCapture(true);
    m_mouseEnabled = true;
}

void Input::disableMouse() {
    if (!m_mouseEnabled) return;
    setMouseCapture(false);
    m_mouseEnabled = false;
}

void Input::setMouseCapture(bool capture) {
    m_mouseCapture = capture;
    m_cursorShownForUnfocus = false; // reset unfocus tracking
#ifdef __APPLE__
    CGDirectDisplayID display = CGMainDisplayID();
    if (capture) {
        g_screenCenter = {
            (CGFloat)(CGDisplayPixelsWide(display) / 2),
            (CGFloat)(CGDisplayPixelsHigh(display) / 2)
        };
        // Disassociate mouse position from cursor — this effectively
        // hides the cursor and prevents it from moving visually.
        CGAssociateMouseAndMouseCursorPosition(false);
        CGDisplayHideCursor(display);
        CGWarpMouseCursorPosition(g_screenCenter);
    } else {
        CGAssociateMouseAndMouseCursorPosition(true);
        CGDisplayShowCursor(display);
    }
#endif
}

void Input::accumulateMouseDelta(float dx, float dy) {
    float oldX = m_rawDx.load(std::memory_order_relaxed);
    while (!m_rawDx.compare_exchange_weak(oldX, oldX + dx, std::memory_order_relaxed));

    float oldY = m_rawDy.load(std::memory_order_relaxed);
    while (!m_rawDy.compare_exchange_weak(oldY, oldY + dy, std::memory_order_relaxed));
}

// ============================================================
// readTerminalEvents: parse stdin for focus sequences
// ============================================================

void Input::readTerminalEvents(InputState& state) {
    // Read all available bytes from stdin.
    // We're looking for terminal focus reporting sequences:
    //   \033[I  = focus gained (this terminal pane/window got focus)
    //   \033[O  = focus lost   (this terminal pane/window lost focus)
    // These are sent by the terminal emulator (kitty, iTerm2, Terminal.app, etc.)
    // when mode 1004 is enabled. They are per-window/pane, not per-app.

    unsigned char buf[512];
    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            // Look for ESC [ I (focus in) or ESC [ O (focus out)
            if (buf[i] == 0x1B && i + 2 < n && buf[i + 1] == '[') {
                if (buf[i + 2] == 'I') {
                    // Focus gained
                    bool wasFocused = state.focused.load(std::memory_order_relaxed);
                    state.focused.store(true, std::memory_order_relaxed);
                    m_focused.store(true, std::memory_order_relaxed);
                    (void)wasFocused;
                    i += 2; // skip past the sequence
                    continue;
                }
                if (buf[i + 2] == 'O') {
                    // Focus lost
                    bool wasFocused = state.focused.load(std::memory_order_relaxed);
                    state.focused.store(false, std::memory_order_relaxed);
                    m_focused.store(false, std::memory_order_relaxed);
                    if (wasFocused) {
                        state.focusLost.store(true, std::memory_order_relaxed);
                    }
                    i += 2; // skip past the sequence
                    continue;
                }
            }
            // All other stdin bytes are ignored (keyboard is handled by CGEventTap)
        }
    }
}

// ============================================================
// poll: called once per frame
// ============================================================

void Input::poll(InputState& state) {
    // Read terminal events (focus in/out sequences) from stdin
    readTerminalEvents(state);

    bool isFocused = m_focused.load(std::memory_order_relaxed);

    if (isFocused) {
        // Mouse deltas: atomically swap accumulated deltas to zero
        state.mouseDx = m_rawDx.exchange(0, std::memory_order_relaxed);
        state.mouseDy = m_rawDy.exchange(0, std::memory_order_relaxed);

        // Re-hide cursor on refocus if mouse capture is active
        if (m_mouseCapture && m_cursorShownForUnfocus) {
#ifdef __APPLE__
            CGDisplayHideCursor(CGMainDisplayID());
#endif
            m_cursorShownForUnfocus = false;
        }
    } else {
        // Unfocused: discard any accumulated mouse deltas
        m_rawDx.store(0, std::memory_order_relaxed);
        m_rawDy.store(0, std::memory_order_relaxed);
        state.mouseDx = 0;
        state.mouseDy = 0;

        // Clear held-state keys so the player stops moving
        state.forward.store(false, std::memory_order_relaxed);
        state.backward.store(false, std::memory_order_relaxed);
        state.left.store(false, std::memory_order_relaxed);
        state.right.store(false, std::memory_order_relaxed);
        state.up.store(false, std::memory_order_relaxed);
        state.down.store(false, std::memory_order_relaxed);

        // Drain the press queue and text input so stale events don't fire on refocus
        state.consumePresses();
        {
            std::lock_guard<std::mutex> lock(state.textMutex);
            state.typedChars.clear();
            state.backspaceCount = 0;
            state.enterPressed = false;
        }

        // If mouse capture was active, show cursor once while unfocused
        if (m_mouseCapture && !m_cursorShownForUnfocus) {
#ifdef __APPLE__
            CGDisplayShowCursor(CGMainDisplayID());
#endif
            m_cursorShownForUnfocus = true;
        }
    }
}
