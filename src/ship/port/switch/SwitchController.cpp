#ifdef __SWITCH__
#include "SwitchController.h"
#include <algorithm>
#include <unordered_map>
#include "ship/utils/StringHelper.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

namespace Ship {

static constexpr uint64_t CONTROLLER_MASK = 1UL;
static std::unordered_map<int32_t, int32_t> sInstanceIdToDeviceSlot;
static std::unordered_map<int32_t, std::string> sInstanceIdToSerial;

void SwitchController::RegisterDevice(int32_t instanceId, int32_t slot, const std::string& serial) {
    sInstanceIdToDeviceSlot[instanceId] = slot;
    sInstanceIdToSerial[instanceId] = serial;
}

void SwitchController::ClearDeviceSlots() {
    sInstanceIdToDeviceSlot.clear();
    sInstanceIdToSerial.clear();
}

int32_t SwitchController::GetDeviceSlot(int32_t instanceId) {
    auto it = sInstanceIdToDeviceSlot.find(instanceId);
    return (it != sInstanceIdToDeviceSlot.end()) ? it->second : -1;
}

std::string SwitchController::GetDeviceSerial(int32_t instanceId) {
    auto it = sInstanceIdToSerial.find(instanceId);
    return (it != sInstanceIdToSerial.end()) ? it->second : "";
}

void SwitchController::Update() {
    // Called every frame.
    // We only poll for controller connection changes every 60 frames.
    static int sFrameCounter = 0;
    if (++sFrameCounter < 60) {
        return;
    }
    sFrameCounter = 0;

    // Check for connection changes
    static bool sLastConnected[4] = {};
    bool changed = false;
    for (uint8_t i = 0; i < 4; i++) {
        bool connected = GetInstance().IsNpadConnected(i);
        if (connected != sLastConnected[i]) {
            sLastConnected[i] = connected;
            changed = true;
        }
    }
    if (changed) {
        auto context = Context::GetRawInstance();
        if (context && context->GetControlDeck()) {
            context->GetControlDeck()->GetConnectedPhysicalDeviceManager()->HandlePhysicalDeviceConnect(0);
        }
    }
}

SwitchController& SwitchController::GetInstance() {
    static SwitchController instance;
    return instance;
}

HidNpadIdType SwitchController::GetNpadId(uint8_t portIndex) const {
    const uint8_t clampedIndex = std::min<uint8_t>(portIndex, 7);
    return static_cast<HidNpadIdType>(HidNpadIdType_No1 + clampedIndex);
}

bool SwitchController::EnsureInitialized(uint8_t portIndex) {
    if (portIndex >= mControllers.size()) {
        return false;
    }

    auto& controller = mControllers[portIndex];
    if (controller.Initialized) {
        return true;
    }

    const auto npadId = GetNpadId(portIndex);
    const uint64_t padMask = (CONTROLLER_MASK << npadId) | (CONTROLLER_MASK << HidNpadIdType_Handheld);

    padInitializeWithMask(&controller.State, padMask);
    padUpdate(&controller.State);

    hidInitializeVibrationDevices(controller.Handles[0], 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    hidInitializeVibrationDevices(controller.Handles[1], 2, npadId, HidNpadStyleTag_NpadJoyDual);

    hidGetSixAxisSensorHandles(&controller.Sensors[0], 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    hidGetSixAxisSensorHandles(&controller.Sensors[1], 1, npadId, HidNpadStyleTag_NpadFullKey);
    hidGetSixAxisSensorHandles(&controller.Sensors[2], 2, npadId, HidNpadStyleTag_NpadJoyDual);

    for (auto& sensor : controller.Sensors) {
        hidStartSixAxisSensor(sensor);
    }

    SPDLOG_INFO("Initialized controller for port {}: npadId={}, padMask={:#x}, styleSet={:#x}, deviceType={:#x}",
                portIndex, static_cast<int>(npadId), padMask, padGetStyleSet(&controller.State),
                hidGetNpadDeviceType(npadId));

    controller.Initialized = true;
    return true;
}

bool SwitchController::ReadSixAxisState(uint8_t portIndex, HidSixAxisSensorState& state) {
    if (!EnsureInitialized(portIndex)) {
        return false;
    }

    auto& controller = mControllers[portIndex];
    padUpdate(&controller.State);
    const uint64_t styleSet = padGetStyleSet(&controller.State);

    if (styleSet & HidNpadStyleTag_NpadJoyDual) {
        const uint64_t attributes = padGetAttributes(&controller.State);
        if (attributes & HidNpadAttribute_IsLeftConnected) {
            hidGetSixAxisSensorStates(controller.Sensors[2], &state, 1);
            return state.delta_time > 0;
        }
        if (attributes & HidNpadAttribute_IsRightConnected) {
            hidGetSixAxisSensorStates(controller.Sensors[3], &state, 1);
            return state.delta_time > 0;
        }
    }

    if (styleSet & HidNpadStyleTag_NpadHandheld) {
        hidGetSixAxisSensorStates(controller.Sensors[0], &state, 1);
        return state.delta_time > 0;
    }

    if (styleSet & HidNpadStyleTag_NpadFullKey) {
        hidGetSixAxisSensorStates(controller.Sensors[1], &state, 1);
        return state.delta_time > 0;
    }

    return false;
}

bool SwitchController::ReadGyro(uint8_t portIndex, float& pitch, float& yaw, float& roll) {
    HidSixAxisSensorState sixAxisState = {};
    if (!ReadSixAxisState(portIndex, sixAxisState)) {
        pitch = 0.0f;
        yaw = 0.0f;
        roll = 0.0f;
        return false;
    }

    pitch = sixAxisState.angular_velocity.x * 8.0f;
    yaw = sixAxisState.angular_velocity.y * 8.0f;
    roll = sixAxisState.angular_velocity.z * 8.0f;
    return true;
}

void SwitchController::SendRumble(uint8_t portIndex, float lowFrequencyAmplitude, float highFrequencyAmplitude) {
    if (!EnsureInitialized(portIndex)) {
        return;
    }

    auto& controller = mControllers[portIndex];
    padUpdate(&controller.State);
    const uint64_t styleSet = padGetStyleSet(&controller.State);

    uint64_t externalStyle = 0;
    if (styleSet & HidNpadStyleTag_NpadFullKey) {
        externalStyle = HidNpadStyleTag_NpadFullKey;
    } else if (styleSet & HidNpadStyleTag_NpadJoyDual) {
        externalStyle = HidNpadStyleTag_NpadJoyDual;
    } else if (styleSet & HidNpadStyleTag_NpadGc) {
        externalStyle = HidNpadStyleTag_NpadGc;
    } else if (styleSet & HidNpadStyleTag_NpadLucia) {
        externalStyle = HidNpadStyleTag_NpadLucia;
    } else if (styleSet & HidNpadStyleTag_NpadLagon) {
        externalStyle = HidNpadStyleTag_NpadLagon;
    } else if (styleSet & HidNpadStyleTag_NpadLark) {
        externalStyle = HidNpadStyleTag_NpadLark;
    } else if (styleSet & HidNpadStyleTag_NpadLager) {
        externalStyle = HidNpadStyleTag_NpadLager;
    }

    if (externalStyle != 0 && controller.LastExternalRumbleStyle != externalStyle) {
        hidInitializeVibrationDevices(controller.Handles[1], 2, GetNpadId(portIndex),
                                      static_cast<HidNpadStyleTag>(externalStyle));
        controller.LastExternalRumbleStyle = externalStyle;
    }

    HidVibrationValue vibrationValues[2] = {};
    for (auto& value : vibrationValues) {
        value.amp_low = std::clamp(lowFrequencyAmplitude, 0.0f, 1.0f);
        value.amp_high = std::clamp(highFrequencyAmplitude, 0.0f, 1.0f);
        value.freq_low = 160.0f;
        value.freq_high = 320.0f;
    }

    if (padIsHandheld(&controller.State)) {
        hidSendVibrationValues(controller.Handles[0], vibrationValues, 2);
    } else if (externalStyle != 0) {
        hidSendVibrationValues(controller.Handles[1], vibrationValues, 2);
    }
}

bool SwitchController::IsNpadConnected(uint8_t portIndex) const {
    if (portIndex >= mControllers.size()) {
        return false;
    }
    PadState pad = {};
    const auto npadId = GetNpadId(portIndex);
    if (portIndex == 0) {
        padInitializeWithMask(&pad, (CONTROLLER_MASK << npadId) | (CONTROLLER_MASK << HidNpadIdType_Handheld));
    } else {
        padInitializeWithMask(&pad, CONTROLLER_MASK << npadId);
    }
    padUpdate(&pad);
    return padIsConnected(&pad);
}

std::string SwitchController::GetControllerName(uint8_t portIndex) {
    if (!EnsureInitialized(portIndex)) {
        return "Controller";
    }

    auto& controller = mControllers[portIndex];
    padUpdate(&controller.State);
    const uint32_t styleSet = padGetStyleSet(&controller.State);
    const uint32_t deviceType = hidGetNpadDeviceType(GetNpadId(portIndex));

    if (styleSet & HidNpadStyleTag_NpadHandheld) {
        return "Handheld";
    }
    if (styleSet & HidNpadStyleTag_NpadGc) {
        return "GameCube Controller";
    }

    // Style sets are not precise enough for the other types
    if (deviceType & HidDeviceTypeBits_FullKey) {
        return "Pro Controller";
    }
    if (deviceType & HidDeviceTypeBits_DebugPad) {
        return "DebugPad";
    }
    if (deviceType & HidDeviceTypeBits_Palma) {
        return "Poke Ball Plus";
    }
    if (deviceType & HidDeviceTypeBits_Lucia) {
        return "SNES Controller";
    }
    if (deviceType & HidDeviceTypeBits_Lagon) {
        return "N64 Controller";
    }
    if (deviceType & HidDeviceTypeBits_Lager) {
        return "Genesis Controller";
    }
    if (deviceType & HidDeviceTypeBits_System) {
        return "Generic Controller";
    }
#define DETECT_LR_CONTROLLER(deviceType, name, leftBits, rightBits)      \
    if ((deviceType) & ((leftBits) | (rightBits))) {                     \
        bool l = (deviceType) & (leftBits);                              \
        bool r = (deviceType) & (rightBits);                             \
        return (l && r) ? name " (L+R)" : l ? name " (L)" : name " (R)"; \
    }

    DETECT_LR_CONTROLLER(deviceType, "Joy-Con", HidDeviceTypeBits_JoyLeft, HidDeviceTypeBits_JoyRight)
    DETECT_LR_CONTROLLER(deviceType, "Famicom Controller",
                         HidDeviceTypeBits_LarkHvcLeft | HidDeviceTypeBits_HandheldLarkHvcLeft,
                         HidDeviceTypeBits_LarkHvcRight | HidDeviceTypeBits_HandheldLarkHvcRight)
    DETECT_LR_CONTROLLER(deviceType, "NES Controller",
                         HidDeviceTypeBits_LarkNesLeft | HidDeviceTypeBits_HandheldLarkNesLeft,
                         HidDeviceTypeBits_LarkNesRight | HidDeviceTypeBits_HandheldLarkNesRight)

    return "Unknown Controller";
}

std::string SwitchController::GetControllerSerial(uint8_t npadIndex) {
    if (npadIndex >= mControllers.size()) {
        return StringHelper::Sprintf("NPAD%d", npadIndex);
    }

    if (!EnsureInitialized(npadIndex)) {
        return StringHelper::Sprintf("NPAD%d", npadIndex);
    }

    auto& controller = mControllers[npadIndex];
    padUpdate(&controller.State);

    HidNpadIdType queryIds[2];
    int queryCount;
    if (npadIndex == 0) {
        queryIds[0] = GetNpadId(0);
        queryIds[1] = HidNpadIdType_Handheld;
        queryCount = 2;
    } else {
        queryIds[0] = GetNpadId(npadIndex);
        queryCount = 1;
    }

    for (int q = 0; q < queryCount; q++) {
        HidsysUniquePadId uniquePadIds[2] = {};
        s32 total = 0;
        Result rc = hidsysGetUniquePadsFromNpad(queryIds[q], uniquePadIds, 2, &total);
        if (R_FAILED(rc) || total <= 0) {
            continue;
        }

        HidsysUniquePadSerialNumber serial = {};
        rc = hidsysGetUniquePadSerialNumber(uniquePadIds[0], &serial);
        if (R_SUCCEEDED(rc) && serial.serial_number[0] != '\0') {
            return serial.serial_number;
        }
    }

    return StringHelper::Sprintf("NPAD%d", npadIndex);
}

} // namespace Ship
#endif