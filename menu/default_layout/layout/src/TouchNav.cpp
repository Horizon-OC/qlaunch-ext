/* default layout touch navigation (C) 2026 Souldbminer */
/* Licensed under the GPLv2                          */

#include "TouchNav.hpp"
#include "LayoutHost.hpp"
#include "Widgets.hpp"
#include "MenuHost.hpp"
#include "Gfx.hpp"
#include "Sfx.hpp"
#include "App.hpp"

namespace TouchNav {

    namespace {

        /// @brief Maximum fingers tracked for gestures.
        constexpr int kGestureMaxFingers = 8;

        /// @brief Drag-start travel in design pixels (matches core kTapSlop).
        constexpr float kTouchSlop = 10.0f;

        /// @brief Swipe fire distance in design pixels.
        constexpr float kSwipeDistance = 140.0f;

        /// @brief Maximum swipe length in frames before it becomes a drag.
        constexpr int kSwipeMaxFrames = 25;

        /// @brief Per-finger gesture tracking slot.
        struct GestureSlot {
            bool active;
            bool claimed;
            int fingerId;
            float startX;
            float startY;
            float lastX;
            float lastY;
            int startFrame;
            bool swiped;
            bool dragged;
        };

        static GestureSlot s_gestures[kGestureMaxFingers] = {};
        static int s_frameCounter = 0;

        /// @brief Find the gesture slot tracking a finger id.
        /// @param fingerId Touch finger id.
        /// @return Slot index, or -1 when untracked.
        int FindGesture(int fingerId)
        {
            for (int i = 0; i < kGestureMaxFingers; i++) {
                if (s_gestures[i].active && s_gestures[i].fingerId == fingerId) {
                    return i;
                }
            }

            return -1;
        }

        /// @brief Claim a free gesture slot for a new finger.
        /// @param fingerId Touch finger id.
        /// @param x Down X in design space.
        /// @param y Down Y in design space.
        /// @return Slot index, or -1 when full.
        int ClaimGesture(int fingerId, float x, float y)
        {
            for (int i = 0; i < kGestureMaxFingers; i++) {
                if (!s_gestures[i].active) {
                    s_gestures[i].active = true;
                    s_gestures[i].fingerId = fingerId;
                    s_gestures[i].startX = x;
                    s_gestures[i].startY = y;
                    s_gestures[i].lastX = x;
                    s_gestures[i].lastY = y;
                    s_gestures[i].startFrame = s_frameCounter;
                    s_gestures[i].swiped = false;
                    s_gestures[i].dragged = false;
                    s_gestures[i].claimed = false;
                    return i;
                }
            }

            return -1;
        }

        /// @brief Switch straight to a tab by registry index.
        /// @param tabIndex Tabs registry index.
        void SwitchToTab(int tabIndex)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            const char *tabId = hostApi->tabIdAt(tabIndex);

            if (!tabId || !tabId[0]) {
                return;
            }

            hostApi->switchMenuById(tabId);
            hostApi->menuJingle(tabIndex);
        }

        /// @brief Route one queued tap to tabs, popups, manual submenu handlers,
        /// registered cells, or the A fallback, in that order.
        /// Registered cells dispatch through SubmenuTouchAction
        /// (select on tap, act on re-tap).
        /// @param tapX Tap X in design space.
        /// @param tapY Tap Y in design space.
        /// @param buttonsDown Proven pressed mask (A/B pulsed in as needed).
        void HandleTap(float tapX, float tapY, u64 &buttonsDown)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            int tappedTab = -1;

            if (WTopBar::TabAtPosition(tapX, tapY, &tappedTab) && tappedTab >= 0) {
                SwitchToTab(tappedTab);
                return;
            }

            if (hostApi->popupIsOpen()) {
                int popupCell = hostApi->popupCellAt(tapX, tapY);

                if (popupCell < 0) {
                    buttonsDown |= HidNpadButton_B;
                } else if (popupCell == hostApi->popupSelection()) {
                    buttonsDown |= HidNpadButton_A;
                } else {
                    hostApi->popupSelectCell(popupCell);
                }

                return;
            }

            bool chromeFocused = hostApi->topFocus() || hostApi->bottomFocus();
            int currentTab = hostApi->appMenu();

            if (!chromeFocused && hostApi->submenuTap(currentTab, tapX, tapY)) {
                return;
            }

            if (!chromeFocused) {
                int hitIndex = -1;

                if (hostApi->touchHitTest(currentTab, tapX, tapY, &hitIndex) && hitIndex >= 0) {
                    hostApi->submenuTouchAction(currentTab, hitIndex);
                    return;
                }
            }

            buttonsDown |= HidNpadButton_A;
        }

    } // namespace

    /// @brief Interpret one touch frame into buttons, taps, drags, and press claims.
    /// Taps go to tabs, popups, manual submenu handlers, registered
    /// cells (select, re-tap acts), or fall back to A. A finger down
    /// first offers SubmenuTouchDown; a claimed finger skips drag, swipe,
    /// and tap routing and gets SubmenuTouchUp on release. Drags scroll
    /// supporting content with inertia; fast horizontal swipes pulse
    /// Left/Right; swipe down exits the folder view. Ignored while
    /// transitioning.
    /// @param touches Touch frame (may be null when untouched).
    /// @param touchCount Touch point count.
    /// @param buttonsDown Proven pressed mask (pulses OR-ed in).
    void Handle(const LayoutTouch *touches, int touchCount, u64 &buttonsDown)
    {
        const MenuHostApi *hostApi = GetLayoutHost();
        s_frameCounter++;

        if (touchCount < 0) {
            touchCount = 0;
        }

        if (hostApi->menuT() < 1.0f) {
            return;
        }

        if (!touches || touchCount <= 0) {
            for (int i = 0; i < kGestureMaxFingers; i++) {
                s_gestures[i].active = false;
            }

            return;
        }

        int homeTabIndex = hostApi->tabFindById("home");
        bool inFolderView = homeTabIndex >= 0 && hostApi->appMenu() == homeTabIndex &&
                            hostApi->homeSub() == kHomeSubFolders &&
                            !hostApi->topFocus() && !hostApi->bottomFocus();

        int liveFingers = 0;

        for (int touchIndex = 0; touchIndex < touchCount; touchIndex++) {
            if (touches[touchIndex].phase != kLayoutTouchUp) {
                liveFingers++;
            }
        }

        for (int touchIndex = 0; touchIndex < touchCount && touchIndex < kGestureMaxFingers; touchIndex++) {
            const LayoutTouch &touch = touches[touchIndex];

            if (touch.phase == kLayoutTouchDown) {
                int gestureIndex = ClaimGesture(touch.fingerId, touch.x, touch.y);

                if (gestureIndex >= 0 && !hostApi->popupIsOpen() &&
                    !hostApi->topFocus() && !hostApi->bottomFocus()) {
                    if (hostApi->submenuTouchDown(hostApi->appMenu(), touch.x, touch.y)) {
                        s_gestures[gestureIndex].claimed = true;
                        s_gestures[gestureIndex].swiped = true;
                    }
                }

                continue;
            }

            int gestureIndex = FindGesture(touch.fingerId);

            if (gestureIndex < 0) {
                continue;
            }

            GestureSlot &gesture = s_gestures[gestureIndex];
            float frameMoveX = touch.x - gesture.lastX;
            float frameMoveY = touch.y - gesture.lastY;
            gesture.lastX = touch.x;
            gesture.lastY = touch.y;

            bool singleFinger = (liveFingers <= 1);

            float deltaX = touch.x - gesture.startX;
            float deltaY = touch.y - gesture.startY;
            int ageFrames = s_frameCounter - gesture.startFrame;

            if (touch.phase == kLayoutTouchHeld && singleFinger && !gesture.claimed) {
                if (gesture.dragged) {
                    if (hostApi->submenuDrag(hostApi->appMenu(), frameMoveX, frameMoveY,
                                             kLayoutTouchHeld) == 0) {
                        gesture.dragged = false;
                    }
                } else {
                    float travelSq = deltaX * deltaX + deltaY * deltaY;

                    if (travelSq > kTouchSlop * kTouchSlop) {
                        gesture.dragged = hostApi->submenuDrag(hostApi->appMenu(), frameMoveX, frameMoveY,
                                                               kLayoutTouchHeld) != 0;

                        if (gesture.dragged) {
                            gesture.swiped = true;
                        }
                    }
                }
            }

            if (touch.phase == kLayoutTouchHeld && !gesture.swiped && !gesture.dragged && !gesture.claimed &&
                singleFinger && ageFrames <= kSwipeMaxFrames) {
                float absX = deltaX < 0.0f ? -deltaX : deltaX;
                float absY = deltaY < 0.0f ? -deltaY : deltaY;

                if (absX >= kSwipeDistance || absY >= kSwipeDistance) {
                    gesture.swiped = true;

                    if (absX >= absY) {
                        buttonsDown |= (deltaX < 0.0f ? HidNpadButton_AnyRight : HidNpadButton_AnyLeft);
                    } else if (deltaY > 0.0f && inFolderView) {
                        buttonsDown |= HidNpadButton_B;
                    }
                }
            }

            if (touch.phase == kLayoutTouchUp) {
                if (gesture.claimed) {
                    hostApi->submenuTouchUp(hostApi->appMenu(), touch.x, touch.y);

                    /* Drop this finger's queued tap so a claimed press
                       never leaks into the tap fallback. */
                    float dropX = 0.0f;
                    float dropY = 0.0f;
                    hostApi->takeTap(&dropX, &dropY);

                    gesture.active = false;
                    gesture.dragged = false;
                    gesture.claimed = false;
                    continue;
                }

                if (gesture.dragged) {
                    hostApi->submenuDrag(hostApi->appMenu(), 0.0f, 0.0f, kLayoutTouchUp);
                }

                /* Shared tap queue: each event fires exactly once, here or
                   in the prompt path, never both. */
                float tapX = 0.0f;
                float tapY = 0.0f;

                while (hostApi->takeTap(&tapX, &tapY)) {
                    HandleTap(tapX, tapY, buttonsDown);
                }

                gesture.active = false;
                gesture.dragged = false;
            }
        }
    }

} /* namespace TouchNav */
