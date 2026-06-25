#pragma once

namespace Ship::Switch::Keyboard {
/// Creates inline swkbd applet overlay
void Create();
/// Closes the swkbd applet, if open
void Close();
/// Handles updating swkbd state, this should be called periodically.
void Update();
/// Gets the current vertical offset needed to shift the active text box into view.
float GetYOffset();
} // namespace Ship::Switch::Keyboard
