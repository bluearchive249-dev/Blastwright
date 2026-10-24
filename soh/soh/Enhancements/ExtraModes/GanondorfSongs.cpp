// Play as Ganondorf: a song player for the ocarina (his pipe organ). Songs are MIDI files in a "ganondorf_songs"
// folder next to the game (or in mods/ganondorf_songs). While the ocarina is out, the D-pad controls it:
//   D-Up: play / stop the selected song    D-Left / D-Right: previous / next song
// A MIDI file is turned into one melody line (the ocarina plays one note at a time: the highest note sounding at
// any moment, drums left out), shifted by octaves into the ocarina's range, and played through the game's own
// ocarina playback, so it sounds on whatever instrument the ocarina is.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <imgui.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/Notification/Notification.h"
#include "soh/SohGui/UIWidgets.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#include "GanondorfSongs.h"

#define CVAR_SONGS(name) CVAR_ENHANCEMENT("PlayAsGanondorf.Songs." name)

namespace {

struct Song {
    std::string name;
    std::string path;
};

std::vector<Song> sSongs;
bool sScanned = false;
std::vector<OcarinaNote> sNotes; // the song being played; must stay put while it plays
bool sPlaying = false;

// ---- MIDI -----------------------------------------------------------------------------------------------------

struct NoteEvent {
    uint32_t tick;
    bool on;
    uint8_t note;
    uint8_t velocity;
};

struct TempoEvent {
    uint32_t tick;
    uint32_t usPerQuarter;
};

struct Reader {
    const std::vector<uint8_t>& d;
    size_t p;
    size_t end;

    bool Has(size_t n) const {
        return p + n <= end;
    }
    uint8_t U8() {
        return d[p++];
    }
    uint32_t Be(int n) {
        uint32_t v = 0;
        for (int i = 0; i < n; i++) {
            v = (v << 8) | d[p++];
        }
        return v;
    }
    uint32_t Var() {
        uint32_t v = 0;
        for (int i = 0; i < 4 && p < end; i++) {
            uint8_t b = d[p++];
            v = (v << 7) | (b & 0x7F);
            if (!(b & 0x80)) {
                break;
            }
        }
        return v;
    }
};

bool ReadMidi(const std::string& path, std::vector<NoteEvent>& notes, std::vector<TempoEvent>& tempos,
              uint16_t& division) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Reader r{ d, 0, d.size() };

    if (!r.Has(14) || (r.Be(4) != 0x4D546864)) { // "MThd"
        return false;
    }
    uint32_t headerLen = r.Be(4);
    r.Be(2); // format
    uint16_t trackCount = (uint16_t)r.Be(2);
    division = (uint16_t)r.Be(2);
    r.p = 8 + headerLen;
    if (division & 0x8000) {
        division = 480; // SMPTE timing: rare, treat as a plain resolution
    }

    for (uint16_t t = 0; t < trackCount && r.Has(8); t++) {
        uint32_t id = r.Be(4);
        uint32_t len = r.Be(4);
        size_t trackEnd = std::min(r.p + len, d.size());
        if (id != 0x4D54726B) { // "MTrk"
            r.p = trackEnd;
            continue;
        }
        Reader tr{ d, r.p, trackEnd };
        uint32_t tick = 0;
        uint8_t status = 0;

        while (tr.p < trackEnd) {
            tick += tr.Var();
            if (tr.p >= trackEnd) {
                break;
            }
            uint8_t b = tr.d[tr.p];
            if (b & 0x80) {
                status = b;
                tr.p++;
            }
            if (status == 0xFF) { // meta
                if (!tr.Has(1)) {
                    break;
                }
                uint8_t type = tr.U8();
                uint32_t mlen = tr.Var();
                if (!tr.Has(mlen)) {
                    break;
                }
                if ((type == 0x51) && (mlen == 3)) {
                    tempos.push_back({ tick, tr.Be(3) });
                } else {
                    tr.p += mlen;
                }
                if (type == 0x2F) {
                    break;
                }
                continue;
            }
            if ((status == 0xF0) || (status == 0xF7)) { // sysex
                uint32_t slen = tr.Var();
                tr.p += slen;
                continue;
            }
            uint8_t kind = status & 0xF0;
            uint8_t channel = status & 0x0F;
            int dataLen = ((kind == 0xC0) || (kind == 0xD0)) ? 1 : 2;
            if (!tr.Has(dataLen)) {
                break;
            }
            uint8_t a = tr.U8();
            uint8_t v = (dataLen == 2) ? tr.U8() : (uint8_t)0;
            if (channel == 9) {
                continue; // drums
            }
            if ((kind == 0x90) && (v > 0)) {
                notes.push_back({ tick, true, a, v });
            } else if ((kind == 0x80) || (kind == 0x90)) {
                notes.push_back({ tick, false, a, 0 });
            }
        }
        r.p = trackEnd;
    }
    return !notes.empty();
}

// Seconds at each tick, from the tempo changes
double TickToSeconds(uint32_t tick, const std::vector<TempoEvent>& tempos, uint16_t division) {
    double seconds = 0.0;
    uint32_t lastTick = 0;
    uint32_t us = 500000; // 120 bpm until told otherwise
    for (const TempoEvent& t : tempos) {
        if (t.tick >= tick) {
            break;
        }
        seconds += (double)(t.tick - lastTick) * us / (division * 1000000.0);
        lastTick = t.tick;
        us = t.usPerQuarter;
    }
    return seconds + (double)(tick - lastTick) * us / (division * 1000000.0);
}

// The melody (highest sounding note) as ocarina notes. Lengths are in the ocarina playback's ticks (60 a second,
// scaled by the tempo setting).
bool BuildSong(const std::string& path, std::vector<OcarinaNote>& out) {
    std::vector<NoteEvent> events;
    std::vector<TempoEvent> tempos;
    uint16_t division = 480;

    if (!ReadMidi(path, events, tempos, division) || (division == 0)) {
        return false;
    }
    std::stable_sort(events.begin(), events.end(), [](const NoteEvent& a, const NoteEvent& b) {
        return (a.tick != b.tick) ? (a.tick < b.tick) : (!a.on && b.on); // note offs first
    });
    std::stable_sort(tempos.begin(), tempos.end(),
                     [](const TempoEvent& a, const TempoEvent& b) { return a.tick < b.tick; });

    struct Segment {
        double start;
        int note; // -1: rest
        uint8_t velocity;
    };
    std::vector<Segment> segments;
    std::map<int, int> active; // note -> how many times it's held
    std::map<int, uint8_t> velocities;
    size_t i = 0;

    while (i < events.size()) {
        uint32_t tick = events[i].tick;
        for (; (i < events.size()) && (events[i].tick == tick); i++) {
            const NoteEvent& e = events[i];
            if (e.on) {
                active[e.note]++;
                velocities[e.note] = e.velocity;
            } else if (active.count(e.note) && (--active[e.note] <= 0)) {
                active.erase(e.note);
            }
        }
        int top = active.empty() ? -1 : active.rbegin()->first;
        if (segments.empty() || (segments.back().note != top)) {
            segments.push_back({ TickToSeconds(tick, tempos, division), top, top >= 0 ? velocities[top] : (uint8_t)0 });
        }
    }
    if (segments.size() < 2) {
        return false;
    }

    // Shift by octaves into the ocarina's range (C4 to D#9, 64 semitones)
    int lowest = 127;
    int highest = 0;
    for (const Segment& s : segments) {
        if (s.note >= 0) {
            lowest = std::min(lowest, s.note);
            highest = std::max(highest, s.note);
        }
    }
    int shift = 0;
    while (lowest + shift < 60) {
        shift += 12;
    }
    while ((highest + shift > 60 + 63) && (lowest + shift - 12 >= 60)) {
        shift -= 12;
    }

    float tempo = CVarGetFloat(CVAR_SONGS("Tempo"), 1.0f);
    double ticksPerSecond = 60.0 / std::max(0.25f, tempo);

    out.clear();
    out.push_back({ OCARINA_PITCH_NONE, 0, 100, 0, 0, 0 }); // never played (the playback looks one note back)
    for (size_t k = 0; k + 1 < segments.size(); k++) {
        double length = (segments[k + 1].start - segments[k].start) * ticksPerSecond;
        uint16_t frames = (uint16_t)std::clamp(length + 0.5, 1.0, 60000.0);
        OcarinaNote n = { OCARINA_PITCH_NONE, frames, 100, 0, 0, 0 };
        if (segments[k].note >= 0) {
            n.pitch = (uint8_t)std::clamp(segments[k].note + shift - 60, 0, 63);
            n.volume = (uint8_t)std::clamp(60 + segments[k].velocity / 2, 60, 127);
        }
        out.push_back(n);
    }
    out.push_back({ OCARINA_PITCH_NONE, 0, 100, 0, 0, 0 }); // the end
    return true;
}

// ---- Songs folder ---------------------------------------------------------------------------------------------

void Scan() {
    sSongs.clear();
    sScanned = true;
    for (const std::string& dir : { Ship::Context::GetPathRelativeToAppDirectory("ganondorf_songs"),
                                    Ship::Context::GetPathRelativeToAppDirectory("mods/ganondorf_songs") }) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) {
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](char c) { return (char)std::tolower((unsigned char)c); });
            if (entry.is_regular_file() && ((ext == ".mid") || (ext == ".midi"))) {
                sSongs.push_back({ entry.path().stem().string(), entry.path().string() });
            }
        }
    }
    std::sort(sSongs.begin(), sSongs.end(), [](const Song& a, const Song& b) { return a.name < b.name; });
}

int Selected() {
    if (sSongs.empty()) {
        return -1;
    }
    int index = CVarGetInteger(CVAR_SONGS("Selected"), 0);
    return std::clamp(index, 0, (int)sSongs.size() - 1);
}

void Notify(const std::string& prefix, const std::string& message) {
    Notification::Emit({ .prefix = prefix, .message = message, .remainingTime = 3.0f, .mute = true });
}

void Stop() {
    if (sPlaying) {
        AudioOcarina_PlayCustomSong(NULL);
        sPlaying = false;
    }
}

void Play() {
    int index = Selected();
    Stop();
    if (index < 0) {
        Notify("No songs", "Put MIDI files in the ganondorf_songs folder");
        return;
    }
    if (!BuildSong(sSongs[index].path, sNotes)) {
        Notify("Can't play", sSongs[index].name);
        return;
    }
    AudioOcarina_PlayCustomSong(sNotes.data());
    sPlaying = true;
    Notify("Playing", sSongs[index].name);
}

void Select(int step) {
    if (sSongs.empty()) {
        return;
    }
    int index = (Selected() + step + (int)sSongs.size()) % (int)sSongs.size();
    CVarSetInteger(CVAR_SONGS("Selected"), index);
    if (sPlaying) {
        Play();
    } else {
        Notify("Song", sSongs[index].name);
    }
}

void OnFrame() {
    PlayState* play = gPlayState;
    if ((play == NULL) || !CVarGetInteger(CVAR_SONGS("Enabled"), 1)) {
        Stop();
        return;
    }
    Player* player = GET_PLAYER(play);
    bool ocarinaOut = (player != NULL) && (player->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING) &&
                      (play->msgCtx.msgMode == MSGMODE_OCARINA_PLAYING);
    if (!ocarinaOut) {
        Stop();
        return;
    }
    if (!sScanned) {
        Scan();
    }

    // Read before the D-pad goes anywhere else this frame
    Input* input = &play->state.input[0];
    if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        if (sPlaying) {
            Stop();
            Notify("Stopped", "");
        } else {
            Play();
        }
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        Select(1);
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        Select(-1);
    }

    // At the end: again, or stop
    if (sPlaying && !AudioOcarina_IsPlayingBack()) {
        if (CVarGetInteger(CVAR_SONGS("Loop"), 1)) {
            AudioOcarina_PlayCustomSong(sNotes.data());
        } else {
            sPlaying = false;
        }
    }
}

void RegisterSongs() {
    COND_HOOK(OnGameFrameUpdate, true, OnFrame);
}

RegisterShipInitFunc initFunc(RegisterSongs, {});

} // namespace

// While a song plays, the D-pad belongs to the player (Play as Ganondorf leaves it alone)
extern "C" s32 GanondorfSongs_IsOcarinaOut(void) {
    return gPlayState != NULL && CVarGetInteger(CVAR_SONGS("Enabled"), 1) && GET_PLAYER(gPlayState) != NULL &&
           (GET_PLAYER(gPlayState)->stateFlags2 & PLAYER_STATE2_OCARINA_PLAYING);
}

// Menu: the song list
void GanondorfSongs_DrawMenu() {
    if (!sScanned) {
        Scan();
    }
    if (sSongs.empty()) {
        ImGui::TextWrapped("No songs found. Put MIDI files (.mid) in a folder called ganondorf_songs next to the "
                           "game, then press Rescan.");
    } else {
        int index = Selected();
        ImGui::Text("Song:");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##GanondorfSong", sSongs[index].name.c_str())) {
            for (int i = 0; i < (int)sSongs.size(); i++) {
                if (ImGui::Selectable(sSongs[i].name.c_str(), i == index)) {
                    CVarSetInteger(CVAR_SONGS("Selected"), i);
                    Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
                }
            }
            ImGui::EndCombo();
        }
    }
    if (ImGui::Button("Rescan Songs")) {
        Scan();
    }
}
