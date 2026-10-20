/* qlaunch-ext touchscreen polling (C) 2026 Souldbminer */
/* Licensed under the GPLv2                          */

#include "Touch.hpp"
#include "Log.hpp"

namespace Touch {

    namespace {

        /// @brief Design-space scale for the 1280x720 touch panel.
        constexpr float kTouchScale = 1.5f;

        /// @brief Maximum tap duration in seconds.
        constexpr float kTapMaxTime = 0.35f;

        /// @brief Tap slop in design pixels (matches layout kTouchSlop).
        constexpr float kTapSlop = 10.0f;

        /// @brief Tap event lifetime in seconds (bounds staleness).
        constexpr float kTapExpiry = 0.5f;

        /// @brief Tap event queue depth.
        constexpr int kTapQueueMax = 4;

        /// @brief Finger lifecycle states (explicit machine, no guessing).
        enum FingerLifecycle {
            kFingerIdle = 0,
            kFingerDown = 1,
            kFingerHeld = 2,
        };

        /// @brief Per-finger tracking slot.
        struct FingerSlot {
            bool active;
            int lifecycle;
            int fingerId;
            float startX;
            float startY;
            float lastX;
            float lastY;
            float downAt;
            bool moved;
        };

        /// @brief One queued tap event.
        struct TapEvent {
            float x;
            float y;
            float time;
        };

        static TouchFrame s_frame = {};
        static FingerSlot s_slots[kTouchMaxFingers] = {};
        static TapEvent s_taps[kTapQueueMax] = {};
        static int s_tapHead = 0;
        static int s_tapCount = 0;
        static bool s_ready = false;
        static float s_clock = 0.0f;
        static TouchElement s_elements[kTouchMaxElements] = {};
        static int s_elementCount = 0;
        static int s_registerTab = -1;

        /// @brief Find the slot tracking a finger id.
        /// @param fingerId Panel finger id.
        /// @return Slot index, or -1 when untracked.
        int FindSlot(int fingerId)
        {
            for (int i = 0; i < kTouchMaxFingers; i++) {
                if (s_slots[i].active && s_slots[i].fingerId == fingerId) {
                    return i;
                }
            }

            return -1;
        }

        /// @brief Claim a free slot for a new finger.
        /// @param fingerId Panel finger id.
        /// @return Slot index, or -1 when full.
        int ClaimSlot(int fingerId)
        {
            for (int i = 0; i < kTouchMaxFingers; i++) {
                if (!s_slots[i].active) {
                    s_slots[i].active = true;
                    s_slots[i].lifecycle = kFingerDown;
                    s_slots[i].fingerId = fingerId;
                    s_slots[i].moved = false;
                    return i;
                }
            }

            return -1;
        }

        /// @brief Queue one tap event, dropping the oldest on overflow.
        /// @param x Tap X in design space.
        /// @param y Tap Y in design space.
        void PushTap(float x, float y)
        {
            if (s_tapCount >= kTapQueueMax) {
                s_tapHead = (s_tapHead + 1) % kTapQueueMax;
                s_tapCount--;
            }

            int tailIndex = (s_tapHead + s_tapCount) % kTapQueueMax;
            s_taps[tailIndex].x = x;
            s_taps[tailIndex].y = y;
            s_taps[tailIndex].time = s_clock;
            s_tapCount++;
        }

    } // namespace

    /// @brief Open the touchscreen channel (safe to call once at boot).
    /// @return True when polling may be used.
    bool Init()
    {
        hidInitializeTouchScreen();
        s_ready = true;
        return true;
    }

    /// @brief Sample the touchscreen into the current frame (call once per frame).
    void Poll()
    {
        s_frame.count = 0;

        if (!s_ready) {
            return;
        }

        s_clock += 1.0f / 60.0f;

        HidTouchScreenState screenState = {};
        hidGetTouchScreenStates(&screenState, 1);

        bool seenIds[kTouchMaxFingers];
        int seenCount = 0;

        int touchCount = screenState.count;

        if (touchCount < 0) {
            touchCount = 0;
        }

        if (touchCount > 16) {
            touchCount = 16;
        }

        for (int touchIndex = 0; touchIndex < touchCount; touchIndex++) {
            const HidTouchState &touch = screenState.touches[touchIndex];
            float touchX = (float)touch.x * kTouchScale;
            float touchY = (float)touch.y * kTouchScale;
            int slotIndex = FindSlot((int)touch.finger_id);
            int touchPhase = kTouchHeld;

            if (slotIndex < 0) {
                slotIndex = ClaimSlot((int)touch.finger_id);

                if (slotIndex < 0) {
                    continue;
                }

                s_slots[slotIndex].startX = touchX;
                s_slots[slotIndex].startY = touchY;
                s_slots[slotIndex].downAt = s_clock;
                touchPhase = kTouchDown;
            } else if (s_slots[slotIndex].lifecycle == kFingerDown) {
                s_slots[slotIndex].lifecycle = kFingerHeld;
            }

            s_slots[slotIndex].lastX = touchX;
            s_slots[slotIndex].lastY = touchY;

            float travelX = touchX - s_slots[slotIndex].startX;
            float travelY = touchY - s_slots[slotIndex].startY;

            if (travelX * travelX + travelY * travelY > kTapSlop * kTapSlop) {
                s_slots[slotIndex].moved = true;
            }

            if (seenCount < kTouchMaxFingers) {
                seenIds[seenCount++] = (int)touch.finger_id;
            }

            if (s_frame.count < kTouchMaxFingers) {
                TouchPoint &point = s_frame.points[s_frame.count++];
                point.x = touchX;
                point.y = touchY;
                point.fingerId = (int)touch.finger_id;
                point.phase = touchPhase;
            }
        }

        for (int i = 0; i < kTouchMaxFingers; i++) {
            if (!s_slots[i].active) {
                continue;
            }

            bool stillDown = false;

            for (int seenIndex = 0; seenIndex < seenCount; seenIndex++) {
                if (seenIds[seenIndex] == s_slots[i].fingerId) {
                    stillDown = true;
                    break;
                }
            }

            if (stillDown) {
                continue;
            }

            /* Release: quick and unmoved means exactly one tap event. */
            if (!s_slots[i].moved && s_clock - s_slots[i].downAt <= kTapMaxTime) {
                PushTap(s_slots[i].lastX, s_slots[i].lastY);
            }

            if (s_frame.count < kTouchMaxFingers) {
                TouchPoint &point = s_frame.points[s_frame.count++];
                point.x = s_slots[i].lastX;
                point.y = s_slots[i].lastY;
                point.fingerId = s_slots[i].fingerId;
                point.phase = kTouchUp;
            }

            s_slots[i].active = false;
        }
    }

    /// @brief Give the latest polled frame.
    /// @return Frame valid until the next Poll().
    const TouchFrame &Frame()
    {
        return s_frame;
    }

    /// @brief Consume the oldest queued tap, if any.
    /// Expired taps drop silently, so one touch fires exactly once.
    /// @param xOut Tap X in design space.
    /// @param yOut Tap Y in design space.
    /// @return True when a tap was consumed.
    bool TakeTap(float *xOut, float *yOut)
    {
        while (s_tapCount > 0 && s_clock - s_taps[s_tapHead].time > kTapExpiry) {
            s_tapHead = (s_tapHead + 1) % kTapQueueMax;
            s_tapCount--;
        }

        if (s_tapCount <= 0) {
            return false;
        }

        if (xOut) {
            *xOut = s_taps[s_tapHead].x;
        }

        if (yOut) {
            *yOut = s_taps[s_tapHead].y;
        }

        s_tapHead = (s_tapHead + 1) % kTapQueueMax;
        s_tapCount--;
        return true;
    }

    /// @brief Reset the touch state machine (modal transitions).
    /// Drops queued taps and releases tracked fingers so a touch that
    /// opened a modal can never act inside it.
    void Disarm()
    {
        s_tapHead = 0;
        s_tapCount = 0;

        for (int i = 0; i < kTouchMaxFingers; i++) {
            s_slots[i].active = false;
            s_slots[i].lifecycle = kFingerIdle;
            s_slots[i].moved = false;
        }
    }

    /// @brief Mark which tab is currently drawing (tags new cells).
    /// Also purges that tab's previous cells so each Draw starts clean.
    /// @param tabIndex Tabs registry index, or -1 when unknown.
    void BeginTab(int tabIndex)
    {
        s_registerTab = tabIndex;

        if (tabIndex < 0) {
            return;
        }

        int keptCount = 0;

        for (int i = 0; i < s_elementCount; i++) {
            if (s_elements[i].tab != tabIndex) {
                s_elements[keptCount++] = s_elements[i];
            }
        }

        s_elementCount = keptCount;
    }

    /// @brief Register one tappable cell for the drawing tab.
    /// @param index Cell id meaningful to that tab.
    /// @param x Left edge in design space.
    /// @param y Top edge in design space.
    /// @param w Cell width.
    /// @param h Cell height.
    /// @return True when the cell was recorded.
    bool RegisterCell(int index, float x, float y, float w, float h)
    {
        if (s_registerTab < 0 || s_elementCount >= kTouchMaxElements) {
            return false;
        }

        TouchElement &slot = s_elements[s_elementCount++];
        slot.tab = s_registerTab;
        slot.index = index;
        slot.x = x;
        slot.y = y;
        slot.w = w;
        slot.h = h;
        return true;
    }

    /// @brief Hit-test a point against one tab's registered cells.
    /// Later registrations win ties (topmost drawn last).
    /// @param tab Tabs registry index to test.
    /// @param x Tap X in design space.
    /// @param y Tap Y in design space.
    /// @param indexOut Cell index on hit.
    /// @return True when a cell of that tab was hit.
    bool HitTest(int tab, float x, float y, int *indexOut)
    {
        for (int i = s_elementCount - 1; i >= 0; i--) {
            const TouchElement &cell = s_elements[i];

            if (cell.tab != tab) {
                continue;
            }

            if (x >= cell.x && x <= cell.x + cell.w && y >= cell.y && y <= cell.y + cell.h) {
                if (indexOut) {
                    *indexOut = cell.index;
                }

                return true;
            }
        }

        return false;
    }

} /* namespace Touch */
