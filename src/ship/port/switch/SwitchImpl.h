#pragma once

#include <cstdint>
#include <string>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>
#include "SwitchPerformanceProfiles.h"

namespace Ship {
enum SwitchProfiles { MAXIMUM, HIGH, BOOST, STOCK, POWERSAVINGM1, POWERSAVINGM2, POWERSAVINGM3 };

enum SwitchPhase { PreInitPhase, PostInitPhase };

namespace Switch {
void Init(SwitchPhase phase);
void Exit();
void ImGuiSetupFont(ImFontAtlas* fonts);
bool IsRunning();
void GetDisplaySize(int* width, int* height);
void ApplyOverclock();
void ShowErrorApplet(const char* format, ...);
void ThrowMissingOTR(std::string otrPath);
char* GetControllerUUID(int controller);
}; // namespace Switch
}; // namespace Ship
