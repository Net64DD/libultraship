#ifdef __SWITCH__
#include "SwitchImpl.h"
#include "SwitchKeyboard.h"
#include <switch.h>
#include <SDL2/SDL.h>
#include "SwitchPerformanceProfiles.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include <spdlog/spdlog.h>
#include "ship/Context.h"
#include "ship/audio/Audio.h"

#define DOCKED_MODE 1
#define HANDHELD_MODE 0

static AppletHookCookie applet_hook_cookie;
static bool isRunning = true;
static bool hasFocus = true;

void DetectAppletMode();

static void on_applet_hook(AppletHookType hook, void* param);

void Ship::Switch::Init(SwitchPhase phase) {
    switch (phase) {
        case PreInitPhase:
            DetectAppletMode();
            socketInitializeDefault();
#ifdef _DEBUG
            nxlinkStdio();
#endif
            break;
        case PostInitPhase:
            appletInitializeGamePlayRecording();
            appletSetGamePlayRecordingState(true);
            appletHook(&applet_hook_cookie, on_applet_hook, NULL);
            appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
            if (!hosversionBefore(8, 0, 0)) {
                clkrstInitialize();
            }
            hidsysInitialize();
            padConfigureInput(8, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadGc | HidNpadStyleTag_NpadLark |
                                     HidNpadStyleTag_NpadHandheldLark | HidNpadStyleTag_NpadLucia |
                                     HidNpadStyleTag_NpadLagon | HidNpadStyleTag_NpadLager |
                                     HidNpadStyleTag_NpadSystemExt | HidNpadStyleTag_NpadSystem);
            break;
    }
}

void Ship::Switch::Exit() {
    Keyboard::Close();
    hidsysExit();
    socketExit();
    clkrstExit();
    appletSetGamePlayRecordingState(false);
}

void Ship::Switch::ImGuiSetupFont(ImFontAtlas* fonts) {
    plInitialize(PlServiceType_User);
    static PlFontData stdFontData, extFontData;

    PlFontData fonts_std;
    PlFontData fonts_ext;

    plGetSharedFontByType(&fonts_std, PlSharedFontType_Standard);
    plGetSharedFontByType(&fonts_ext, PlSharedFontType_NintendoExt);

    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;

    strcpy(config.Name, "Nintendo Standard");
    fonts->AddFontFromMemoryTTF(fonts_std.address, fonts_std.size, 24.0f, &config, fonts->GetGlyphRangesCyrillic());

    strcpy(config.Name, "Nintendo Ext");
    static const ImWchar ranges[] = {
        0xE000, 0xE06B, 0xE070, 0xE07E, 0xE080, 0xE099, 0xE0A0, 0xE0BA, 0xE0C0, 0xE0D6, 0xE0E0, 0xE0F5, 0xE100,
        0xE105, 0xE110, 0xE116, 0xE121, 0xE12C, 0xE130, 0xE13C, 0xE140, 0xE14D, 0xE150, 0xE153, 0,
    };

    fonts->AddFontFromMemoryTTF(fonts_ext.address, fonts_ext.size, 24.0f, &config, ranges);
    fonts->Build();

    plExit();
}

bool Ship::Switch::IsRunning() {
    return isRunning;
}

void Ship::Switch::GetDisplaySize(int* width, int* height) {
    switch (appletGetOperationMode()) {
        case DOCKED_MODE:
            *width = 1920;
            *height = 1080;
            break;
        case HANDHELD_MODE:
            *width = 1280;
            *height = 720;
            break;
    }
}

void Ship::Switch::ApplyOverclock(void) {
    SwitchProfiles perfMode = (SwitchProfiles)CVarGetInteger(CVAR_SWITCH_PERF_MODE, (int)Ship::MAXIMUM);

    if (perfMode >= 0 && perfMode <= Ship::POWERSAVINGM3) {
        if (hosversionBefore(8, 0, 0)) {
            pcvSetClockRate(PcvModule_CpuBus, SWITCH_CPU_SPEEDS_VALUES[perfMode]);
        } else {
            ClkrstSession session = { 0 };
            clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
            clkrstSetClockRate(&session, SWITCH_CPU_SPEEDS_VALUES[perfMode]);
            clkrstCloseSession(&session);
        }
    }
}

static void on_applet_hook(AppletHookType hook, void* param) {
    AppletFocusState focus_state;

    /* Exit request */
    switch (hook) {
        case AppletHookType_OnExitRequest:
            isRunning = false;
            break;

            /* Focus state*/
        case AppletHookType_OnFocusState:
            focus_state = appletGetFocusState();
            hasFocus = focus_state == AppletFocusState_InFocus;

            if (!hasFocus) {
                if (hosversionBefore(8, 0, 0)) {
                    pcvSetClockRate(PcvModule_CpuBus, SWITCH_CPU_SPEEDS_VALUES[Ship::STOCK]);
                } else {
                    ClkrstSession session = { 0 };
                    clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
                    clkrstSetClockRate(&session, SWITCH_CPU_SPEEDS_VALUES[Ship::STOCK]);
                    clkrstCloseSession(&session);
                }
            } else {
                Ship::Switch::ApplyOverclock();
                SPDLOG_INFO("restarting SDL audio system to work around audio problems on resume");
                if (auto audio = Ship::Context::GetRawInstance()->GetAudio(); audio != nullptr) {
                    audio->SetCurrentAudioBackend(Ship::AudioBackend::SDL);
                }
            }

            break;

        case AppletHookType_OnResume:
            Ship::Switch::ApplyOverclock();
            break;

            /* Performance mode */
        case AppletHookType_OnPerformanceMode:
            Ship::Switch::ApplyOverclock();
            break;
        default:
            break;
    }
}

void Ship::Switch::ShowErrorApplet(const char* format, ...) {
    ErrorSystemConfig errorConfig = {};

    char messageBuffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
    va_end(args);

    const Result rc = errorSystemCreate(&errorConfig, messageBuffer, nullptr);

    if (R_SUCCEEDED(rc)) {
        errorSystemSetResult(&errorConfig, MAKERESULT(400, 1));
        errorSystemShow(&errorConfig);
    }
}

const char* RandomTexts[] = {
    "Psst, don't forget to blame Melon",
    "Potsanity when?",
    "Why are you acting so random?",
    "Enough! My ship sails in the morning",
    "Do you want 2 or 7 of those?",
    "Lamp oil, rope, bombs you want it, it's yours my friend as long as you have enough rupees",
    "You can build it yourself",
    "Descargar para android",
    "Made with <3 by the Harbour Masters!",
    "They say that Kenix is not a developer",
    "Squadala we're off",
    "They say one once saw an equals not get set equals",
    "This is the port all true gamers dock at"
    "Enhancements? Times Savers? Cheats? You want them? They're yours my friend!",
    "They say you gotta have the BIIIIG salad",
    "They say Louis stopped working on the imports so he can focus on the exports",
    "They say ZAPD is good software",
};

void DetectAppletMode() {
    AppletType at = appletGetAppletType();
    if (at == AppletType_Application || at == AppletType_SystemApplication)
        return;

    Ship::Switch::ShowErrorApplet("You've launched the Ship while in Applet mode.\n"
                                  "Please relaunch while in full-memory mode.\n"
                                  "Hold R when opening any game to enter HBMenu.\n\n"
                                  "%s.",
                                  RandomTexts[rand() % 25]);
    exit(1);
}

void Ship::Switch::ThrowMissingOTR(std::string otrPath) {
    Ship::Switch::ShowErrorApplet("You've launched the Ship without an OTR/O2R file.\n"
                                  "Please relaunch making sure %s exists.\n\n"
                                  "%s.",
                                  otrPath.c_str(), RandomTexts[rand() % 25]);
    exit(2);
}
#endif
