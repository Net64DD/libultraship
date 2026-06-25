#include "libultraship/libultraship.h"
#include <SDL2/SDL.h>
#include <ratio>

// Establish a chrono duration for the N64 46.875MHz clock rate
typedef std::ratio<3000, 64> n64ClockRatio;
typedef std::ratio_divide<std::micro, n64ClockRatio> n64CycleRate;
typedef std::chrono::duration<long long, n64CycleRate> n64CycleRateDuration;

extern "C" {
uint8_t __osMaxControllers = MAXCONTROLLERS;
uint64_t __osCurrentTime = 0;

int32_t osContInit(OSMesgQueue* mq, uint8_t* controllerBits, OSContStatus* status) {
    *controllerBits = 0;
    status->status |= 1;

#ifndef __SWITCH__
    std::string controllerDb = Ship::Context::LocateFileAcrossAppDirs("gamecontrollerdb.txt");
    int mappingsAdded = SDL_GameControllerAddMappingsFromFile(controllerDb.c_str());
    if (mappingsAdded >= 0) {
        SPDLOG_INFO("Added SDL game controllers from \"{}\" ({})", controllerDb, mappingsAdded);
    } else {
        SPDLOG_ERROR("Failed add SDL game controller mappings from \"{}\" ({})", controllerDb, SDL_GetError());
    }
#else
    // devkitPro's switch-sdl2 ships a built-in game controller mapping for the "Switch Controller" GUID that follows
    // SDL's positional convention (A = south, B = east, X = west, Y = north).  The Switch's physical face buttons use
    // the Nintendo layout (A = east, B = south, X = north, Y = west), so the positional mapping reports physical B as
    // SDL_CONTROLLER_BUTTON_A, physical A as SDL_CONTROLLER_BUTTON_B, physical Y as SDL_CONTROLLER_BUTTON_X, and
    // physical X as SDL_CONTROLLER_BUTTON_Y.  That swap propagates to both the default N64 button bindings and ImGui
    // menu navigation (which activates on BUTTON_A), so the physical B button selects and physical A backs out.
    //
    // Re-register the same GUID with a label-matched mapping (only a/b and x/y are swapped relative to the built-in;
    // all other fields are unchanged) so the SDL_CONTROLLER_BUTTON_* enums line up with the printed button labels.
    // SDL_GameControllerAddMapping adds at SDL_CONTROLLER_MAPPING_PRIORITY_API, which overrides the built-in
    // SDL_CONTROLLER_MAPPING_PRIORITY_DEFAULT entry for this GUID.  This runs before SDL_Init/before any controller is
    // opened, so it is the active mapping from the first poll (all Switch pad styles report the same GUID, so a single
    // override covers handheld, Pro, and dual Joy-Con).
    const auto switchMappingAdded =
        SDL_GameControllerAddMapping("000038f853776974636820436f6e7400,Switch Controller,"
                                     "a:b0,b:b1,x:b2,y:b3,"
                                     "back:b11,start:b10,"
                                     "leftshoulder:b6,rightshoulder:b7,leftstick:b4,rightstick:b5,"
                                     "lefttrigger:b8,righttrigger:b9,"
                                     "dpup:b13,dpdown:b15,dpleft:b12,dpright:b14,"
                                     "leftx:a0,lefty:a1,rightx:a2,righty:a3,");
    if (switchMappingAdded >= 0) {
        SPDLOG_INFO("Registered label-matched Switch controller mapping ({})", switchMappingAdded);
    } else {
        SPDLOG_ERROR("Failed to register Switch controller mapping ({})", SDL_GetError());
    }
#endif

    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        SPDLOG_ERROR("Failed to initialize SDL game controllers ({})", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    Ship::Context::GetRawInstance()->GetControlDeck()->Init(controllerBits);

    return 0;
}

int32_t osContStartReadData(OSMesgQueue* mesg) {
    return 0;
}

void osContGetReadData(OSContPad* pad) {
    memset(pad, 0, sizeof(OSContPad) * __osMaxControllers);

    Ship::Context::GetRawInstance()->GetControlDeck()->WriteToPad(pad);
}

void osSetTime(OSTime time) {
    __osCurrentTime =
        std::chrono::duration_cast<n64CycleRateDuration>(std::chrono::steady_clock::now().time_since_epoch()).count() +
        time;
}

// Returns the OS time matching the N64 46.875MHz cycle rate
uint64_t osGetTime() {
    return std::chrono::duration_cast<n64CycleRateDuration>(std::chrono::steady_clock::now().time_since_epoch())
               .count() -
           __osCurrentTime;
}

// Returns the CPU clock count matching the N64 46.875Mhz cycle rate
uint32_t osGetCount() {
    return std::chrono::duration_cast<n64CycleRateDuration>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

OSPiHandle* osCartRomInit() {
    return NULL;
}

int osSetTimer(OSTimer* t, OSTime countdown, OSTime interval, OSMesgQueue* mq, OSMesg msg) {
    return 0;
}

int32_t osEPiStartDma(OSPiHandle* pihandle, OSIoMesg* mb, int32_t direction) {
    return 0;
}

uint32_t osAiGetLength() {
    // TODO: Implement
    return 0;
}

int32_t osAiSetNextBuffer(void* buff, size_t len) {
    // TODO: Implement
    return 0;
}

int32_t __osMotorAccess(OSPfs* pfs, uint32_t vibrate) {
    auto io = Ship::Context::GetRawInstance()->GetControlDeck()->GetControllerByPort(pfs->channel)->GetRumble();
    if (vibrate) {
        io->StartRumble();
    } else {
        io->StopRumble();
    }

    return 0;
}

int32_t osMotorInit(OSMesgQueue* ctrlrqueue, OSPfs* pfs, int32_t channel) {
    pfs->channel = channel;
    return 0;
}
}
