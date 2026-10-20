/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Sfx.hpp"
#include "../core/Touch.hpp"
#include <string.h>

namespace WPopup {

    namespace {

        constexpr float kPopupDimAlpha = 0.6f;
        constexpr float kPopupOutline = 6.0f;
        constexpr float kPopupDialogOutline = 8.0f;
        constexpr float kPopupItemRadius = 12.0f;
        constexpr float kPopupListWidth = 680.0f;
        constexpr float kPopupListItemHeight = 64.0f;
        constexpr float kPopupListBodyTop = 96.0f;
        constexpr float kPopupListMessageHeight = 42.0f;
        constexpr float kPopupListBottomPad = 40.0f;
        constexpr float kPopupListItemInset = 24.0f;
        constexpr float kPopupRowWidth = 760.0f;
        constexpr float kPopupRowHeight = 400.0f;
        constexpr float kPopupRowButtonWidth = 280.0f;
        constexpr float kPopupRowButtonHeight = 80.0f;
        constexpr float kPopupRowButtonGap = 40.0f;
        constexpr float kPopupRowButtonTop = 240.0f;

        /// @brief Shared modal popup state, single global.
        struct PopupState {
            bool isOpen = false;
            PopupLayout layout = PopupLayout::List;
            int itemCount = 0;
            int selection = 0;
            u64 ignoreMask = 0;
            char title[kPopupTextMax];
            char message[kPopupTextMax];
            char labels[kPopupMaxItems][kPopupTextMax];
            float swatches[kPopupMaxItems][3];
            bool hasSwatches = false;
            int gridColumns = 1;
        };

        static PopupState s_state = {};

        /// @brief Copy popup text, empty on null.
        /// @param destinationText Destination buffer.
        /// @param sourceText Source text (may be null).
        static void CopyPopupText(char *destinationText, const char *sourceText)
        {
            if (!sourceText) {
                destinationText[0] = 0;
                return;
            }

            unsigned charIndex = 0;

            while (sourceText[charIndex] != 0 && charIndex + 1 < kPopupTextMax) {
                destinationText[charIndex] = sourceText[charIndex];
                charIndex++;
            }

            destinationText[charIndex] = 0;
        }

        /// @brief Draw one outlined item, ringed when selected.
        /// @param itemX Item left X.
        /// @param itemY Item top Y.
        /// @param itemWidth Item width.
        /// @param itemHeight Item height.
        /// @param labelText Item label (centered).
        /// @param labelPx Label pixel size.
        /// @param labelMaxW Label maximum width.
        /// @param labelBaselineY Label baseline Y.
        /// @param swatchRgb Swatch color (3 floats), or null for text only.
        /// @param isSelected True for the highlighted item.
        static void DrawPopupItem(float itemX, float itemY, float itemWidth, float itemHeight,
                                  const char *labelText, float labelPx, float labelMaxW,
                                  float labelBaselineY, const float *swatchRgb, bool isSelected)
        {
            float menuBarRed = 0.0f;
            float menuBarGreen = 0.0f;
            float menuBarBlue = 0.0f;
            Theme::Color("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);

            float backgroundRed = 0.0f;
            float backgroundGreen = 0.0f;
            float backgroundBlue = 0.0f;
            Theme::Color("bg", &backgroundRed, &backgroundGreen, &backgroundBlue);

            float inkRed = 0.0f;
            float inkGreen = 0.0f;
            float inkBlue = 0.0f;
            Theme::Color("ink", &inkRed, &inkGreen, &inkBlue);

            Gfx::PushOverlayPanel(itemX - kPopupOutline, itemY - kPopupOutline,
                                   itemWidth + kPopupOutline * 2.0f, itemHeight + kPopupOutline * 2.0f,
                                   kPopupItemRadius + kPopupOutline, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
            Gfx::PushOverlayPanel(itemX, itemY, itemWidth, itemHeight, kPopupItemRadius,
                                   backgroundRed, backgroundGreen, backgroundBlue, 1.0f);

            if (swatchRgb) {
                float swatchSize = 40.0f;
                Gfx::PushOverlayPanel(itemX + 24.0f, itemY + (itemHeight - swatchSize) * 0.5f,
                                      swatchSize, swatchSize, 10.0f,
                                      swatchRgb[0], swatchRgb[1], swatchRgb[2], 1.0f);
            }

            if (isSelected) {
                Gfx::PushOverlayRing(itemX - kPopupOutline, itemY - kPopupOutline,
                                      itemWidth + kPopupOutline * 2.0f, itemHeight + kPopupOutline * 2.0f,
                                      kPopupItemRadius + kPopupOutline + 2.0f, 8.0f);
            }

            Font::Centered(labelText, itemX + itemWidth * 0.5f, labelBaselineY, labelPx, labelMaxW,
                            inkRed, inkGreen, inkBlue);
        }

    } // namespace

    /// @brief Open a modal popup, replacing any current one.
    /// @param layout List (vertical rows) or Row (side-by-side buttons).
    /// @param titleText Title line (copied, "Are you sure?" when null/empty).
    /// @param messageText Subtitle line (copied, may be null).
    /// @param labels Item labels (copied, capped at kPopupMaxItems).
    /// @param swatchColors Per-item swatch RGB triples, or null for text only.
    /// @param labelCount Item count.
    /// @param initialSelection Initially highlighted item.
    /// @param heldButtons Buttons currently held; disarmed until released so
    /// the opening press can never confirm or dismiss.
    void Open(PopupLayout layout, const char *titleText, const char *messageText,
              const char * const *labels, const float *swatchColors, int labelCount,
              int initialSelection, u64 heldButtons)
    {
        if (!titleText || !titleText[0]) {
            titleText = "Are you sure?";
        }

        if (!messageText) {
            messageText = "";
        }

        if (!labels || labelCount <= 0) {
            labelCount = 0;
        }

        if (labelCount > kPopupMaxItems) {
            labelCount = kPopupMaxItems;
        }

        s_state.layout = layout;
        s_state.itemCount = labelCount;
        s_state.hasSwatches = (swatchColors != 0 && labelCount > 0);
        s_state.gridColumns = 1;
        CopyPopupText(s_state.title, titleText);
        CopyPopupText(s_state.message, messageText);

        for (int itemIndex = 0; itemIndex < labelCount; itemIndex++) {
            CopyPopupText(s_state.labels[itemIndex], labels[itemIndex]);

            if (s_state.hasSwatches) {
                s_state.swatches[itemIndex][0] = swatchColors[itemIndex * 3 + 0];
                s_state.swatches[itemIndex][1] = swatchColors[itemIndex * 3 + 1];
                s_state.swatches[itemIndex][2] = swatchColors[itemIndex * 3 + 2];
            }
        }

        if (initialSelection < 0 || initialSelection >= labelCount) {
            initialSelection = 0;
        }

        s_state.selection = initialSelection;
        s_state.ignoreMask = heldButtons;
        s_state.isOpen = true;

        /* A touch that opened this modal must not act inside it. */
        Touch::Disarm();
    }

    /// @brief Dismiss the popup without acting.
    void Close()
    {
        s_state.isOpen = false;
        s_state.ignoreMask = 0;
        s_state.itemCount = 0;
        s_state.selection = 0;
        s_state.hasSwatches = false;
        s_state.gridColumns = 1;
    }

    /// @brief Arrange List items in columns (1 keeps the single column).
    /// @param columns Column count (below 1 reads as 1).
    void SetGridColumns(int columns)
    {
        if (columns < 1) {
            columns = 1;
        }

        s_state.gridColumns = columns;
    }

    /// @brief Whether a modal popup covers the screen.
    /// @return True when open.
    bool IsOpen()
    {
        return s_state.isOpen;
    }

    /// @brief Give the highlighted item.
    /// @return Selection index.
    int Selection()
    {
        return s_state.selection;
    }

    /// @brief Hit-test a design-space point against popup items.
    /// Mirrors Draw geometry exactly (outline margin included).
    /// @param x X in design space.
    /// @param y Y in design space.
    /// @return Item index, or -1 when the point hits no item.
    int CellAt(float x, float y)
    {
        if (!s_state.isOpen || s_state.itemCount <= 0) {
            return -1;
        }

        if (s_state.layout == PopupLayout::Row) {
            float panelY = (1080.0f - kPopupRowHeight) * 0.5f;
            float totalWidth = (float)s_state.itemCount * kPopupRowButtonWidth +
                               (float)(s_state.itemCount - 1) * kPopupRowButtonGap;
            float buttonX = 960.0f - totalWidth * 0.5f;
            float buttonY = panelY + kPopupRowButtonTop;

            for (int itemIndex = 0; itemIndex < s_state.itemCount; itemIndex++) {
                float itemX = buttonX + (float)itemIndex * (kPopupRowButtonWidth + kPopupRowButtonGap);

                if (x >= itemX - kPopupOutline && x <= itemX + kPopupRowButtonWidth + kPopupOutline &&
                    y >= buttonY - kPopupOutline && y <= buttonY + kPopupRowButtonHeight + kPopupOutline) {
                    return itemIndex;
                }
            }

            return -1;
        }

        int gridColumns = s_state.gridColumns;

        if (gridColumns < 1) {
            gridColumns = 1;
        }

        bool hasMessage = s_state.message[0] != 0;
        float bodyTop = kPopupListBodyTop + (hasMessage ? kPopupListMessageHeight : 0.0f);
        float itemPitch = kPopupListItemHeight + kPopupRowButtonGap;
        float columnGap = gridColumns > 1 ? (float)kPopupRowButtonGap : 0.0f;
        int gridRows = (s_state.itemCount + gridColumns - 1) / gridColumns;
        float panelHeight = bodyTop + (float)(gridRows - 1) * itemPitch +
                            kPopupListItemHeight + kPopupListBottomPad;
        float panelX = (1920.0f - kPopupListWidth) * 0.5f;
        float panelY = (1080.0f - panelHeight) * 0.5f;
        float itemWidth = (kPopupListWidth - kPopupListItemInset * 2.0f -
                           (float)(gridColumns - 1) * columnGap) / (float)gridColumns;

        for (int itemIndex = 0; itemIndex < s_state.itemCount; itemIndex++) {
            int gridColumn = itemIndex % gridColumns;
            int gridRow = itemIndex / gridColumns;
            float itemX = panelX + kPopupListItemInset + (float)gridColumn * (itemWidth + columnGap);
            float itemY = panelY + bodyTop + (float)gridRow * itemPitch;

            if (x >= itemX - kPopupOutline && x <= itemX + itemWidth + kPopupOutline &&
                y >= itemY - kPopupOutline && y <= itemY + kPopupListItemHeight + kPopupOutline) {
                return itemIndex;
            }
        }

        return -1;
    }

    /// @brief Highlight one popup item.
    /// @param index Item index (clamped).
    void SetSelection(int index)
    {
        if (s_state.itemCount <= 0) {
            return;
        }

        if (index < 0) {
            index = 0;
        }

        if (index >= s_state.itemCount) {
            index = s_state.itemCount - 1;
        }

        s_state.selection = index;
    }

    /// @brief Draw the dim + outlined dialog.
    void Draw()
    {
        if (!s_state.isOpen) {
            return;
        }

        Gfx::PushLayer(Gfx::GfxLayer::Overlay);

        float menuBarRed = 0.0f;
        float menuBarGreen = 0.0f;
        float menuBarBlue = 0.0f;
        Theme::Color("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);

        float backgroundRed = 0.0f;
        float backgroundGreen = 0.0f;
        float backgroundBlue = 0.0f;
        Theme::Color("bg", &backgroundRed, &backgroundGreen, &backgroundBlue);

        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        Theme::Color("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        Theme::Color("dim", &dimRed, &dimGreen, &dimBlue);

        Gfx::PushOverlayQuad(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, kPopupDimAlpha);

        if (s_state.layout == PopupLayout::Row) {
            float panelX = (1920.0f - kPopupRowWidth) * 0.5f;
            float panelY = (1080.0f - kPopupRowHeight) * 0.5f;
            Gfx::PushOverlayPanel(panelX - kPopupDialogOutline, panelY - kPopupDialogOutline,
                                   kPopupRowWidth + kPopupDialogOutline * 2.0f,
                                   kPopupRowHeight + kPopupDialogOutline * 2.0f,
                                   24.0f + kPopupDialogOutline, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
            Gfx::PushOverlayPanel(panelX, panelY, kPopupRowWidth, kPopupRowHeight, 24.0f,
                                   backgroundRed, backgroundGreen, backgroundBlue, 1.0f);

            Font::Centered(s_state.title, 960.0f, panelY + 100.0f, 40.0f, 700.0f, inkRed, inkGreen, inkBlue);
            Font::Centered(s_state.message, 960.0f, panelY + 160.0f, 30.0f, 700.0f, dimRed, dimGreen, dimBlue);

            float totalWidth = (float)s_state.itemCount * kPopupRowButtonWidth +
                (float)(s_state.itemCount - 1) * kPopupRowButtonGap;
            float buttonX = 960.0f - totalWidth * 0.5f;
            float buttonY = panelY + kPopupRowButtonTop;

            for (int itemIndex = 0; itemIndex < s_state.itemCount; itemIndex++) {
                DrawPopupItem(buttonX + (float)itemIndex * (kPopupRowButtonWidth + kPopupRowButtonGap),
                              buttonY, kPopupRowButtonWidth, kPopupRowButtonHeight,
                              s_state.labels[itemIndex], 32.0f, kPopupRowButtonWidth - 20.0f,
                              buttonY + 52.0f, 0, itemIndex == s_state.selection);
            }

            Gfx::PopLayer();
            return;
        }

        int gridColumns = s_state.gridColumns;

        if (gridColumns < 1) {
            gridColumns = 1;
        }

        int gridRows = (s_state.itemCount + gridColumns - 1) / gridColumns;

        if (gridRows < 1) {
            gridRows = 1;
        }

        bool hasMessage = s_state.message[0] != 0;
        float bodyTop = kPopupListBodyTop + (hasMessage ? kPopupListMessageHeight : 0.0f);
        float itemPitch = kPopupListItemHeight + kPopupRowButtonGap;
        float columnGap = gridColumns > 1 ? kPopupRowButtonGap : 0.0f;
        float panelHeight = bodyTop + (float)(gridRows - 1) * itemPitch +
            kPopupListItemHeight + kPopupListBottomPad;
        float panelX = (1920.0f - kPopupListWidth) * 0.5f;
        float panelY = (1080.0f - panelHeight) * 0.5f;
        Gfx::PushOverlayPanel(panelX - kPopupDialogOutline, panelY - kPopupDialogOutline,
                               kPopupListWidth + kPopupDialogOutline * 2.0f,
                               panelHeight + kPopupDialogOutline * 2.0f,
                               24.0f + kPopupDialogOutline, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
        Gfx::PushOverlayPanel(panelX, panelY, kPopupListWidth, panelHeight, 24.0f,
                               backgroundRed, backgroundGreen, backgroundBlue, 1.0f);

        Font::Centered(s_state.title, 960.0f, panelY + 68.0f, 40.0f, 600.0f, inkRed, inkGreen, inkBlue);

        if (hasMessage) {
            Font::Centered(s_state.message, 960.0f, panelY + 110.0f, 30.0f, 600.0f,
                            dimRed, dimGreen, dimBlue);
        }

        float itemWidth = (kPopupListWidth - kPopupListItemInset * 2.0f - (float)(gridColumns - 1) * columnGap) / (float)gridColumns;
        float itemHeight = kPopupListItemHeight;

        for (int itemIndex = 0; itemIndex < s_state.itemCount; itemIndex++) {
            int gridColumn = itemIndex % gridColumns;
            int gridRow = itemIndex / gridColumns;
            float itemX = panelX + kPopupListItemInset + (float)gridColumn * (itemWidth + columnGap);
            float itemY = panelY + bodyTop + (float)gridRow * itemPitch;
            DrawPopupItem(itemX, itemY, itemWidth, itemHeight,
                          s_state.labels[itemIndex], 34.0f, itemWidth - 48.0f,
                          itemY + 46.0f, s_state.hasSwatches ? s_state.swatches[itemIndex] : 0,
                          itemIndex == s_state.selection);
        }
        Gfx::PopLayer();
    }

    /// @brief Handle popup navigation input.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    /// @return Confirmed on A, Cancelled on B/X, else None.
    PopupResult Input(u64 buttonsDown, u64 buttonsHeld)
    {
        if (!s_state.isOpen || s_state.itemCount <= 0) {
            return PopupResult::None;
        }

        s_state.ignoreMask &= buttonsHeld;
        u64 freshPress = buttonsDown & ~s_state.ignoreMask;

        if (s_state.layout == PopupLayout::Row) {
            if (freshPress & HidNpadButton_AnyLeft) {
                s_state.selection = (s_state.selection + s_state.itemCount - 1) % s_state.itemCount;
                Sfx::Play(Sfx::Hover);
            }

            if (freshPress & HidNpadButton_AnyRight) {
                s_state.selection = (s_state.selection + 1) % s_state.itemCount;
                Sfx::Play(Sfx::Hover);
            }
        } else if (s_state.gridColumns > 1) {
            if (freshPress & HidNpadButton_AnyLeft) {
                s_state.selection = (s_state.selection + s_state.itemCount - 1) % s_state.itemCount;
                Sfx::Play(Sfx::Hover);
            }

            if (freshPress & HidNpadButton_AnyRight) {
                s_state.selection = (s_state.selection + 1) % s_state.itemCount;
                Sfx::Play(Sfx::Hover);
            }

            if (freshPress & HidNpadButton_AnyUp) {
                s_state.selection = (s_state.selection + s_state.itemCount - s_state.gridColumns) % s_state.itemCount;
                Sfx::Play(Sfx::Hover);
            }

            if (freshPress & HidNpadButton_AnyDown) {
                s_state.selection = (s_state.selection + s_state.gridColumns) % s_state.itemCount;
                Sfx::Play(Sfx::Hover);
            }
        } else {
            if (freshPress & HidNpadButton_AnyUp) {
                s_state.selection = (s_state.selection + s_state.itemCount - 1) % s_state.itemCount;
                Sfx::Play(Sfx::Hover);
            }

            if (freshPress & HidNpadButton_AnyDown) {
                s_state.selection = (s_state.selection + 1) % s_state.itemCount;
                Sfx::Play(Sfx::Hover);
            }
        }

        if (freshPress & HidNpadButton_A) {
            return PopupResult::Confirmed;
        }

        if (freshPress & (HidNpadButton_B | HidNpadButton_X)) {
            return PopupResult::Cancelled;
        }

        return PopupResult::None;
    }

} /* namespace WPopup */
