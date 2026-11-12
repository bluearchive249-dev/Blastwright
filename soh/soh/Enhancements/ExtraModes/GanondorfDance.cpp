// Play as Ganondorf: the dance emote's song, and the strut's (strut.wav, see GanondorfStrut_Strutting). Any .wav in the ganondorf_songs folder (a file called dance.wav first)
// is read on the first dance, converted to the game's 32 kHz stereo, and mixed into its sound output while he dances.
// The file stays the player's own: nothing of it is shipped.
#include "GanondorfDance.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <ship/Context.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/cvar_prefixes.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/audio/AudioSettings.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "functions.h"
extern PlayState* gPlayState;
}

#define GDF_DANCE_RATE 32000

namespace {

std::vector<int16_t> sSong; // stereo, 32 kHz
std::atomic<int> sState{ 0 }; // 0 not loaded, 1 loading, 2 ready, 3 none / unreadable
std::atomic<bool> sPlaying{ false };
std::atomic<uint32_t> sPos{ 0 };    // in frames
std::atomic<float> sFade{ 0.0f };   // 0..1, eased toward sPlaying
std::atomic<float> sMusicVol{ 1.0f };
bool sRequest = false;
std::atomic<bool> sHeld{ false }; // paused with the game (the pause menu): the song waits
int sSinceHeard = 0;              // game frames since he last asked where the song is

void Notify(const std::string& prefix, const std::string& message) {
    Notification::Emit({ .prefix = prefix, .message = message, .remainingTime = 4.0f, .mute = true });
}

std::string FindSong(const char* only = nullptr) {
    std::string first;
    for (const std::string& dir : { Ship::Context::GetPathRelativeToAppDirectory("ganondorf_songs"),
                                    Ship::Context::GetPathRelativeToAppDirectory("mods/ganondorf_songs") }) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) {
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".wav") {
                continue;
            }
            std::string stem = entry.path().stem().string();
            std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);
            if (only != nullptr) {
                if (stem == only) {
                    return entry.path().string();
                }
                continue;
            }
            if (stem == "dance") {
                return entry.path().string();
            }
            if ((stem != "strut") && first.empty()) {
                first = entry.path().string();
            }
        }
    }
    return first;
}

// PCM 16/24/32-bit or 32-bit float, mono or stereo, any rate
bool LoadWav(const std::string& path, std::vector<int16_t>& out) {
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if ((b.size() < 12) || memcmp(b.data(), "RIFF", 4) || memcmp(b.data() + 8, "WAVE", 4)) {
        return false;
    }
    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t dataLen = 0;
    for (size_t o = 12; o + 8 <= b.size();) {
        uint32_t len;
        memcpy(&len, &b[o + 4], 4);
        size_t body = o + 8;
        if (!memcmp(&b[o], "fmt ", 4) && (len >= 16) && (body + 16 <= b.size())) {
            memcpy(&fmt, &b[body], 2);
            memcpy(&ch, &b[body + 2], 2);
            memcpy(&rate, &b[body + 4], 4);
            memcpy(&bits, &b[body + 14], 2);
            if ((fmt == 0xFFFE) && (len >= 26)) { // extensible: the real format in its sub-format
                memcpy(&fmt, &b[body + 24], 2);
            }
        } else if (!memcmp(&b[o], "data", 4)) {
            data = &b[body];
            dataLen = std::min<size_t>(len, b.size() - body);
            break;
        }
        o = body + len + (len & 1);
    }
    if (!data || (ch < 1) || (rate == 0) || !((fmt == 1 && (bits == 16 || bits == 24 || bits == 32)) ||
                                             (fmt == 3 && bits == 32))) {
        return false;
    }
    size_t step = (bits / 8) * ch;
    size_t n = dataLen / step;
    auto sample = [&](size_t i, int c) -> float {
        const uint8_t* p = data + i * step + (bits / 8) * std::min<int>(c, ch - 1);
        if (fmt == 3) {
            float v;
            memcpy(&v, p, 4);
            return v;
        }
        if (bits == 16) {
            int16_t v;
            memcpy(&v, p, 2);
            return v / 32768.0f;
        }
        if (bits == 24) {
            int32_t v = (p[0] << 8) | (p[1] << 16) | (p[2] << 24);
            return (v >> 8) / 8388608.0f;
        }
        int32_t v;
        memcpy(&v, p, 4);
        return v / 2147483648.0f;
    };
    size_t outN = (size_t)((double)n * GDF_DANCE_RATE / rate);
    out.resize(outN * 2);
    for (size_t i = 0; i < outN; i++) {
        double src = (double)i * rate / GDF_DANCE_RATE;
        size_t a = (size_t)src;
        size_t c = std::min(a + 1, n - 1);
        float w = (float)(src - a);
        for (int k = 0; k < 2; k++) {
            float v = sample(a, k) * (1.0f - w) + sample(c, k) * w;
            out[i * 2 + k] = (int16_t)std::clamp(v * 32767.0f, -32768.0f, 32767.0f);
        }
    }
    return !out.empty();
}

void BeginLoad() {
    int expect = 0;
    if (!sState.compare_exchange_strong(expect, 1)) {
        return;
    }
    std::string path = FindSong();
    if (path.empty()) {
        sState = 3;
        Notify("No dance song", "Put a .wav in the ganondorf_songs folder");
        return;
    }
    std::thread([path]() {
        std::vector<int16_t> song;
        bool ok = LoadWav(path, song);
        if (ok) {
            sSong.swap(song);
        }
        sState = ok ? 2 : 3;
        if (!ok) {
            Notify("Dance song", "Couldn't read " + std::filesystem::path(path).filename().string());
        }
    }).detach();
}

// The strut's song: strut.wav, played while he struts (the "Strut Walk" walk), picked up where it left off each time
// he struts again, faded in and out over a moment so short stops don't chop it
std::vector<int16_t> sStrutSong;
std::atomic<int> sStrutState{ 0 }; // as sState
std::atomic<bool> sStrutOn{ false };
std::atomic<uint32_t> sStrutPos{ 0 };
std::atomic<float> sStrutFade{ 0.0f };
int sStrutSince = 0;

void BeginStrutLoad() {
    int expect = 0;
    if (!sStrutState.compare_exchange_strong(expect, 1)) {
        return;
    }
    std::string path = FindSong("strut");
    if (path.empty()) {
        sStrutState = 3;
        return; // (no strut song: he struts in silence)
    }
    std::thread([path]() {
        std::vector<int16_t> song;
        bool ok = LoadWav(path, song);
        if (ok) {
            sStrutSong.swap(song);
        }
        sStrutState = ok ? 2 : 3;
    }).detach();
}

} // namespace

// He is strutting this frame (called each frame while he struts; when the calls stop, the song fades out)
extern "C" void GanondorfStrut_Strutting(void) {
    BeginStrutLoad();
    sStrutSince = 0;
    if (!sStrutOn) {
        sStrutOn = true;
        sMusicVol = CVarGetInteger(CVAR_SETTING("Volume.MainMusic"), 100) / 100.0f;
        Audio_SetVolScale(SEQ_PLAYER_BGM_MAIN, 1, 0, 10);
    }
}

extern "C" void GanondorfDance_Request(void) {
    sRequest = true;
}

extern "C" int32_t GanondorfDance_TakeRequest(void) {
    bool r = sRequest;
    sRequest = false;
    return r;
}

extern "C" void GanondorfDance_Start(void) {
    if (sState == 3) {
        sState = 0; // look again: the file may have been added since
    }
    BeginLoad();
    sMusicVol = CVarGetInteger(CVAR_SETTING("Volume.MainMusic"), 100) / 100.0f;
    sPos = 0;
    sPlaying = true;
    // The area music fades down under it
    Audio_SetVolScale(SEQ_PLAYER_BGM_MAIN, 1, 0, 10);
}

extern "C" void GanondorfDance_Stop(void) {
    if (!sPlaying) {
        return;
    }
    sPlaying = false;
    Audio_SetVolScale(SEQ_PLAYER_BGM_MAIN, 1, 0x7F, 20);
}

extern "C" float GanondorfDance_Time(void) {
    sSinceHeard = 0;
    sHeld = false;
    if (!sPlaying || (sState != 2)) {
        return -1.0f;
    }
    // (the song as heard: what's mixed now comes out a little later; close enough at 20 fps)
    return (float)sPos.load() / GDF_DANCE_RATE;
}

static void MixStrut(int16_t* samples, uint32_t frames) {
    float fade = sStrutFade;
    bool on = sStrutOn;

    if ((sStrutState != 2) || (!on && (fade <= 0.0f)) || sHeld) {
        return;
    }
    const size_t len = sStrutSong.size() / 2;
    uint32_t pos = sStrutPos;
    float vol = AudioSettings_GetMasterVolume() * sMusicVol;
    float target = on ? 1.0f : 0.0f;
    float fadeStep = 1.0f / (0.4f * GDF_DANCE_RATE);

    for (uint32_t i = 0; i < frames; i++) {
        if (fade < target) {
            fade = std::min(target, fade + fadeStep);
        } else if (fade > target) {
            fade = std::max(target, fade - fadeStep);
        }
        if (pos >= len) {
            pos = 0;
        }
        for (int k = 0; k < 2; k++) {
            int32_t v = samples[i * 2 + k] + (int32_t)(sStrutSong[pos * 2 + k] * vol * fade);
            samples[i * 2 + k] = (int16_t)std::clamp(v, -32768, 32767);
        }
        if (fade > 0.0f) {
            pos++; // (it only moves on while heard: the next strut picks up where this one stopped)
        }
    }
    sStrutFade = fade;
    sStrutPos = pos;
}

extern "C" void GanondorfDance_Mix(int16_t* samples, uint32_t frames) {
    MixStrut(samples, frames);
    if (sState != 2) {
        return;
    }
    float fade = sFade;
    if (sHeld || (!sPlaying && (fade <= 0.0f))) {
        return;
    }
    const size_t len = sSong.size() / 2;
    uint32_t pos = sPos;
    float vol = AudioSettings_GetMasterVolume() * sMusicVol;
    float target = sPlaying ? 1.0f : 0.0f;
    float fadeStep = 1.0f / (0.25f * GDF_DANCE_RATE); // a quarter of a second in or out

    for (uint32_t i = 0; i < frames; i++) {
        if (fade < target) {
            fade = std::min(target, fade + fadeStep);
        } else if (fade > target) {
            fade = std::max(target, fade - fadeStep);
        }
        if (pos >= len) {
            pos = 0; // the dance goes on: so does the song
        }
        for (int k = 0; k < 2; k++) {
            int32_t v = samples[i * 2 + k] + (int32_t)(sSong[pos * 2 + k] * vol * fade);
            samples[i * 2 + k] = (int16_t)std::clamp(v, -32768, 32767);
        }
        pos++;
    }
    sFade = fade;
    sPos = pos;
}

// He stopped dancing without saying so (a new scene, the mode turned off): the song stops and the area music comes
// back. On the pause screen the song only waits.
static void OnFrame() {
    // The strut: stopped a moment after he stops strutting (a quarter second's grace: steps between stick pushes)
    if (sStrutOn && (++sStrutSince > 5)) {
        sStrutOn = false;
        if (!sPlaying) {
            Audio_SetVolScale(SEQ_PLAYER_BGM_MAIN, 1, 0x7F, 20);
        }
    }
    if (!sPlaying) {
        return;
    }
    if ((gPlayState != NULL) && (gPlayState->pauseCtx.state != 0)) {
        sHeld = true;
        return;
    }
    if (++sSinceHeard > 3) {
        GanondorfDance_Stop();
    }
}

static void RegisterDance() {
    COND_HOOK(OnGameFrameUpdate, true, OnFrame);
}

static RegisterShipInitFunc initFunc(RegisterDance, {});
