/* qlaunch-ext touchscreen polling (C) 2026 Souldbminer */
/* Licensed under the GPLv2                          */
#pragma once
#include <switch.h>

namespace Touch {

    /// @brief Maximum fingers tracked per frame.
    constexpr int kTouchMaxFingers = 8;

    /// @brief Finger phase inside a frame.
    constexpr int kTouchDown = 0;
    constexpr int kTouchHeld = 1;
    constexpr int kTouchUp = 2;

    /// @brief One finger in design space (1920x1080).
    struct TouchPoint {
        float x;
        float y;
        int fingerId;
        int phase;
    };

    /// @brief Raw touch frame, refreshed by Poll().
    struct TouchFrame {
        TouchPoint points[kTouchMaxFingers];
        int count;
    };

    /// @brief Maximum registered touch cells (one grid scan each).
    constexpr int kTouchMaxElements = 128;

    /// @brief One tappable cell registered by submenu Draw code.
    /// Cells are only meaningful for the tab that drew them;
    /// registration is rebuilt every Draw, consumed next frame.
    struct TouchElement {
        int tab;
        int index;
        float x;
        float y;
        float w;
        float h;
    };

    /// @brief Open the touchscreen channel (safe to call once at boot).
    /// @return True when polling may be used.
    bool Init();

    /// @brief Sample the touchscreen into the current frame (call once per frame).
    void Poll();

    /// @brief Give the latest polled frame.
    /// @return Frame valid until the next Poll().
    const TouchFrame &Frame();

    /// @brief Consume the oldest queued tap, if any.
    /// Taps queue in Poll() and expire after 0.5s, so one touch fires
    /// exactly once no matter which layer consumes it.
    /// @param xOut Tap X in design space.
    /// @param yOut Tap Y in design space.
    /// @return True when a tap was consumed.
    bool TakeTap(float *xOut, float *yOut);

    /// @brief Reset the touch state machine (modal transitions).
    /// Drops queued taps and releases tracked fingers so a touch that
    /// opened a modal can never act inside it.
    void Disarm();

    /// @brief Mark which tab is currently drawing (tags new cells).
    /// Called by the submenu manager around each tab's Draw.
    /// @param tabIndex Tabs registry index, or -1 when unknown.
    void BeginTab(int tabIndex);

    /// @brief Register one tappable cell for the drawing tab.
    /// @param index Cell id meaningful to that tab.
    /// @param x Left edge in design space.
    /// @param y Top edge in design space.
    /// @param w Cell width.
    /// @param h Cell height.
    /// @return True when the cell was recorded.
    bool RegisterCell(int index, float x, float y, float w, float h);

    /// @brief Hit-test a point against one tab's registered cells.
    /// Later registrations win ties (topmost drawn last).
    /// @param tab Tabs registry index to test.
    /// @param x Tap X in design space.
    /// @param y Tap Y in design space.
    /// @param indexOut Cell index on hit.
    /// @return True when a cell of that tab was hit.
    bool HitTest(int tab, float x, float y, int *indexOut);

} /* namespace Touch */
