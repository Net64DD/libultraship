#ifdef __SWITCH__
#include "SwitchKeyboard.h"
#include <switch.h>
#include <string>
#include <spdlog/spdlog.h>

#include <imgui_internal.h>

#include "ship/Context.h"
#include "ship/window/Window.h"
#include "ship/window/gui/Gui.h"

namespace ImStb {
#include <imstb_textedit.h>
}

// --- Inline keyboard state ---

static SwkbdInline sKeyboard;
static bool sIsKeyboardLaunched = false;
static bool sIsKeyboardActive = false;
static std::string sKeyboardText;
static bool sIsKeyboardTextChanged = false;
static bool sIsKeyboardSubmitted = false;
static ImGuiID sKeyboardOwner = 0;

static int sKeyboardCursorPos = -1;
static bool sIsCursorMoved = false;
static int sPendingCursorPos = -1;
static bool sPendingCancel = false;

static float sKeyboardYOffset = 0.0f;

// --- swkbd callbacks ---

static void OnKeyboardStringChanged(const char* str, SwkbdChangedStringArg* arg) {
    sKeyboardText = str ? str : "";
    sIsKeyboardTextChanged = true;
    sKeyboardCursorPos = arg->cursorPos;
}

static void OnKeyboardMovedCursor(const char* str, SwkbdMovedCursorArg* arg) {
    sKeyboardCursorPos = arg->cursorPos;
    sIsCursorMoved = true;
}

static void OnKeyboardDecidedEnter(const char* str, SwkbdDecidedEnterArg*) {
    sKeyboardText = str ? str : "";
    sIsKeyboardTextChanged = true;
    sIsKeyboardSubmitted = true;
    swkbdInlineDisappear(&sKeyboard);
    sIsKeyboardActive = false;
}

static void OnKeyboardDecidedCancel() {
    // Keep whatever the player left in the field, including empty, after erasing everything.
    sIsKeyboardTextChanged = true;
    sIsKeyboardActive = false;
}

// --- Internal helpers ---

// Shows the inline keyboard overlay, seeded with initialText.
static void ShowKeyboard(ImGuiID owner, const std::string& initialText) {
    if (!sIsKeyboardLaunched || sIsKeyboardActive) {
        return;
    }

    sKeyboardOwner = owner;
    sKeyboardText = initialText;
    sIsKeyboardSubmitted = false;
    swkbdInlineSetInputText(&sKeyboard, initialText.c_str());
    swkbdInlineSetCursorPos(&sKeyboard, static_cast<std::int32_t>(initialText.length()));

    SwkbdAppearArg appear = {};
    swkbdInlineMakeAppearArg(&appear, SwkbdType_Normal);
    swkbdInlineAppearArgSetStringLenMax(&appear, 255);
    swkbdInlineAppear(&sKeyboard, &appear);

    sIsKeyboardActive = true;
}

// Directly sets the ImGui InputText cursor position via the STB textedit state.
// This avoids OnKeyPressed which crashes due to text-buffer scanning.
static void SetInputTextCursorPos(ImGuiInputTextState* state, int pos) {
    if (!state || !state->Stb) {
        return;
    }
    pos = ImClamp(pos, 0, state->TextLen);
    state->Stb->cursor = pos;
    state->Stb->select_start = pos;
    state->Stb->select_end = pos;
    state->CursorFollow = true;
}

// --- Public API ---

void Ship::Switch::Keyboard::Create() {
    auto result = swkbdInlineCreate(&sKeyboard);
    if (R_FAILED(result)) {
        SPDLOG_ERROR("swkbdInlineCreate failed: {:#x}", result);
        return;
    }

    result = swkbdInlineLaunchForLibraryApplet(&sKeyboard, SwkbdInlineMode_AppletDisplay, 0);
    if (R_FAILED(result)) {
        SPDLOG_ERROR("swkbdInlineLaunchForLibraryApplet failed: {:#x}", result);
        swkbdInlineClose(&sKeyboard);
        return;
    }

    swkbdInlineSetChangedStringCallback(&sKeyboard, OnKeyboardStringChanged);
    swkbdInlineSetMovedCursorCallback(&sKeyboard, OnKeyboardMovedCursor);
    swkbdInlineSetDecidedEnterCallback(&sKeyboard, OnKeyboardDecidedEnter);
    swkbdInlineSetDecidedCancelCallback(&sKeyboard, OnKeyboardDecidedCancel);

    sIsKeyboardLaunched = true;
}

void Ship::Switch::Keyboard::Close() {
    if (sIsKeyboardLaunched) {
        swkbdInlineClose(&sKeyboard);
        sIsKeyboardLaunched = false;
    }
}

void Ship::Switch::Keyboard::Update() {
    if (sIsKeyboardLaunched) {
        swkbdInlineUpdate(&sKeyboard, nullptr);
    }

    static bool sWasKeyboardActive = false;
    const bool isKeyboardActive = sIsKeyboardActive;
    if (isKeyboardActive) {
        Context::GetRawInstance()->GetWindow()->GetGui()->BlockGamepadNavigation();
    } else if (sWasKeyboardActive) {
        Context::GetRawInstance()->GetWindow()->GetGui()->UnblockGamepadNavigation();
    }
    sWasKeyboardActive = isKeyboardActive;

    if (!sIsKeyboardLaunched) {
        return;
    }

    ImGuiContext& g = *GImGui;
    ImGuiIO& io = ImGui::GetIO();

    // Deferred cancel from previous frame
    if (sPendingCancel) {
        sPendingCancel = false;
        if (g.ActiveId == sKeyboardOwner) {
            ImGui::ClearActiveID();
        }
        sKeyboardOwner = 0;
        sPendingCursorPos = -1;
    }

    // Apply deferred cursor position from previous frame's text change
    if (sPendingCursorPos >= 0) {
        ImGuiInputTextState* state = ImGui::GetInputTextState(sKeyboardOwner);
        if (state) {
            SetInputTextCursorPos(state, sPendingCursorPos);
        }
        sPendingCursorPos = -1;
    }

    // Auto-show: open the keyboard when an InputText becomes active
    if (io.WantTextInput && !sIsKeyboardActive && sKeyboardOwner == 0) {
        ImGuiInputTextState* state = ImGui::GetInputTextState(g.ActiveId);
        if (state) {
            std::string initialText;
            if (state->TextLen > 0) {
                initialText.assign(state->TextA.Data, state->TextLen);
            }
            ShowKeyboard(g.ActiveId, initialText);
        }
    }

    // Cursor-only move (L/R shoulder buttons)
    if (sIsCursorMoved && !sIsKeyboardTextChanged) {
        ImGuiInputTextState* state = ImGui::GetInputTextState(sKeyboardOwner);
        if (state && sKeyboardCursorPos >= 0) {
            SetInputTextCursorPos(state, sKeyboardCursorPos);
        }
        sIsCursorMoved = false;
    }

    // Replace the InputText content with the latest swkbd text
    if (sIsKeyboardTextChanged) {
        sIsKeyboardTextChanged = false;
        sIsCursorMoved = false;

        ImGuiInputTextState* state = ImGui::GetInputTextState(sKeyboardOwner);
        if (state) {
            state->ClearText();
            const char* s = sKeyboardText.c_str();
            while (*s) {
                unsigned int c;
                int adv = ImTextCharFromUtf8(&c, s, nullptr);
                if (c == 0)
                    break;
                io.InputQueueCharacters.push_back(static_cast<ImWchar>(c));
                s += adv;
            }
            sPendingCursorPos = sKeyboardCursorPos;
        }

        // Submit: inject enter so InputTextEx deactivate
        if (sIsKeyboardSubmitted) {
            sIsKeyboardSubmitted = false;
            io.SetAppAcceptingEvents(true);
            io.AddKeyEvent(ImGuiKey_Enter, true);
            io.AddKeyEvent(ImGuiKey_Enter, false);
        } else if (!sIsKeyboardActive) {
            // Keyboard was cancelled
            sPendingCancel = true;
        }
    }

    // Dismiss when the InputText is no longer active
    if (!io.WantTextInput && sIsKeyboardActive) {
        swkbdInlineDisappear(&sKeyboard);
        sIsKeyboardActive = false;
        sKeyboardOwner = 0;
    }

    if (!io.WantTextInput && !sIsKeyboardActive) {
        sKeyboardOwner = 0;
    }

    // Shift the ImGui display upward if the keyboard covers the active InputText.
    if (sIsKeyboardActive && g.PlatformImeDataPrev.WantVisible) {
        float inputY = g.PlatformImeDataPrev.InputPos.y + g.PlatformImeDataPrev.InputLineHeight;
        float keyboardTop = io.DisplaySize.y * 0.4f;
        if (inputY > keyboardTop) {
            sKeyboardYOffset = inputY - keyboardTop;
            sKeyboardYOffset = ImMin(sKeyboardYOffset, g.PlatformImeDataPrev.InputPos.y);
            sKeyboardYOffset = ImMax(sKeyboardYOffset, 0.0f);
        } else {
            sKeyboardYOffset = 0.0f;
        }
    } else {
        sKeyboardYOffset = 0.0f;
    }

    // Block SDL events from reaching ImGui while the keyboard overlay is on-screen
    io.SetAppAcceptingEvents(!sIsKeyboardActive);
}

float Ship::Switch::Keyboard::GetYOffset() {
    return sKeyboardYOffset;
}

#endif
