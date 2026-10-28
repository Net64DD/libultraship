#include "ship/controller/physicaldevice/ConnectedPhysicalDeviceManager.h"
#include <spdlog/spdlog.h>

#ifdef __SWITCH__
#include "ship/port/switch/SwitchController.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include "ship/utils/StringHelper.h"

static std::string GetIgnoreCvarKey(uint8_t port, const std::string& serial) {
    return StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.Dev_%s.Ignored", port, serial.c_str());
}
#endif

namespace Ship {
ConnectedPhysicalDeviceManager::ConnectedPhysicalDeviceManager() {
}

ConnectedPhysicalDeviceManager::~ConnectedPhysicalDeviceManager() {
}

std::unordered_map<int32_t, SDL_GameController*>
ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadsForPort(uint8_t portIndex) {
    std::unordered_map<int32_t, SDL_GameController*> result;

    for (const auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        if (!PortIsIgnoringInstanceId(portIndex, instanceId)) {
            result[instanceId] = gamepad;
        }
    }

    return result;
}

std::unordered_map<int32_t, std::string> ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadNames() {
    return mConnectedSDLGamepadNames;
}

std::unordered_set<int32_t> ConnectedPhysicalDeviceManager::GetIgnoredInstanceIdsForPort(uint8_t portIndex) {
    return mIgnoredInstanceIds[portIndex];
}

bool ConnectedPhysicalDeviceManager::PortIsIgnoringInstanceId(uint8_t portIndex, int32_t instanceId) {
    return GetIgnoredInstanceIdsForPort(portIndex).contains(instanceId);
}

void ConnectedPhysicalDeviceManager::IgnoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].insert(instanceId);
#ifdef __SWITCH__
    std::string serial = SwitchController::GetDeviceSerial(instanceId);
    if (!serial.empty()) {
        CVarSetInteger(GetIgnoreCvarKey(portIndex, serial).c_str(), 1);
        CVarSave();
    }
#endif
}

void ConnectedPhysicalDeviceManager::UnignoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].erase(instanceId);
#ifdef __SWITCH__
    std::string serial = SwitchController::GetDeviceSerial(instanceId);
    if (!serial.empty()) {
        CVarSetInteger(GetIgnoreCvarKey(portIndex, serial).c_str(), 0);
        CVarSave();
    }
#endif
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceConnect(int32_t sdlDeviceIndex) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceDisconnect(int32_t sdlJoystickInstanceId) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::RefreshConnectedSDLGamepads() {
    mConnectedSDLGamepads.clear();
    mConnectedSDLGamepadNames.clear();
#ifdef __SWITCH__
    SwitchController::ClearDeviceSlots();
#endif
    static SDL_JoystickGUID sZeroGuid;

    for (int32_t i = 0; i < SDL_NumJoysticks(); i++) {

        SDL_JoystickGUID deviceGUID = SDL_JoystickGetDeviceGUID(i);
        if (SDL_memcmp(&deviceGUID, &sZeroGuid, sizeof(deviceGUID)) == 0) {
            SPDLOG_WARN(
                "Calling SDL JoystickGetDeviceGUID with index ({:d}) returned zero GUID. This is likely due to an "
                "invalid index. Refer to https://wiki.libsdl.org/SDL2/SDL_JoystickGetDeviceGUID for more information.",
                i);
            continue;
        }

        char deviceGuidCStr[33] = "";
        SDL_JoystickGetGUIDString(deviceGUID, deviceGuidCStr, sizeof(deviceGuidCStr));

        if (!SDL_IsGameController(i)) {
            SPDLOG_WARN("SDL Joystick (GUID: {}) not recognized as gamepad."
                        "This is likely due to a missing mapping string in gamecontrollerdb.txt."
                        "Refer to https://github.com/mdqinc/SDL_GameControllerDB for more information.",
                        deviceGuidCStr);
            continue;
        }

#ifdef __SWITCH__
        if (!SwitchController::GetInstance().IsNpadConnected(static_cast<uint8_t>(i))) {
            continue;
        }
#endif

        auto gamepad = SDL_GameControllerOpen(i);
        if (gamepad == nullptr) {
            SPDLOG_ERROR("SDL GameControllerOpen error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        auto instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad));
        if (instanceId < 0) {
            SPDLOG_ERROR("SDL JoystickInstanceID error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        std::string gamepadName;
        auto name = SDL_GameControllerName(gamepad);
        if (name == nullptr) {
            gamepadName = deviceGuidCStr;
            SPDLOG_WARN("SDL_GameControllerName returned null. Setting name to GUID \"{}\" instead.", gamepadName);
        } else {
            gamepadName = name;
        }

#ifdef __SWITCH__
        gamepadName = SwitchController::GetInstance().GetControllerName(static_cast<uint8_t>(i));
#endif

        mConnectedSDLGamepads[instanceId] = gamepad;
        mConnectedSDLGamepadNames[instanceId] = gamepadName;

#ifdef __SWITCH__
        std::string serial = SwitchController::GetInstance().GetControllerSerial(static_cast<uint8_t>(i));
        SwitchController::RegisterDevice(instanceId, i, serial);
        for (uint8_t port = 0; port < 4; port++) {
            int32_t defaultIgnored = (port > 0) ? 1 : 0;
            if (CVarGetInteger(GetIgnoreCvarKey(port, serial).c_str(), defaultIgnored)) {
                mIgnoredInstanceIds[port].insert(instanceId);
            }
        }
#else
        for (uint8_t port = 1; port < 4; port++) {
            mIgnoredInstanceIds[port].insert(instanceId);
        }
#endif
    }
}
} // namespace Ship
