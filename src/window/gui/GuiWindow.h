#pragma once

#include <string>
#include <imgui.h>
#include <imgui_internal.h>
#include "window/gui/GuiElement.h"
#include <stdint.h>

namespace Ship {
class GuiWindow : public GuiElement {
  public:
    GuiWindow() = default;
    GuiWindow(const std::string& consoleVariable, bool isVisible, const std::string& name, ImVec2 originalSize,
              uint32_t windowFlags);
    GuiWindow(const std::string& consoleVariable, bool isVisible, const std::string& name, ImVec2 originalSize);
    GuiWindow(const std::string& consoleVariable, bool isVisible, const std::string& name);
    GuiWindow(const std::string& consoleVariable, const std::string& name, ImVec2 originalSize, uint32_t windowFlags);
    GuiWindow(const std::string& consoleVariable, const std::string& name, ImVec2 originalSize);
    GuiWindow(const std::string& consoleVariable, const std::string& name);
    void Draw() override;
    std::string GetName();

  protected:
    void SetVisibility(bool visible) override;
    void BeginGroupPanel(const char* name, const ImVec2& size);
    void EndGroupPanel(float minHeight);
    void SyncVisibilityConsoleVariable();

  private:
    std::string mName;
    std::string mVisibilityConsoleVariable;
    ImVector<ImRect> mGroupPanelLabelStack;
    ImVec2 mOriginalSize;
    uint32_t mWindowFlags;
};
} // namespace Ship
