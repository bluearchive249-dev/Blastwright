// Copyright © 2026 Grimsfrost. All rights reserved.
// Play as Ganondorf: look tuning from ganondorf_tuning.json (see GanondorfTuning.h)

#include "GanondorfTuning.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <iterator>
#include <string>
#include <ship/Context.h>
#include <nlohmann/json.hpp>

namespace {

nlohmann::json sTune = nlohmann::json::object();
std::filesystem::file_time_type sTuneTime{};
uintmax_t sTuneSize = 0;
std::string sTunePath;
std::chrono::steady_clock::time_point sLastCheck{};
std::map<std::string, nlohmann::json> sUsed; // every key asked for, with the value in use
bool sUsedDirty = false;
std::chrono::steady_clock::time_point sLastDump{};

void Refresh() {
    auto now = std::chrono::steady_clock::now();

    if (now - sLastCheck < std::chrono::milliseconds(500)) {
        return;
    }
    sLastCheck = now;
    try {
        std::string found;
        for (const char* name : { "ganondorf_tuning.json", "mods/ganondorf_tuning.json" }) {
            std::string p = Ship::Context::GetPathRelativeToAppDirectory(name);
            if (std::filesystem::exists(p)) {
                found = p;
                break;
            }
        }
        if (found.empty()) {
            if (!sTunePath.empty()) {
                sTune = nlohmann::json::object(); // (the file was removed: back to the defaults)
                sTunePath.clear();
            }
        } else {
            auto t = std::filesystem::last_write_time(found);
            auto size = std::filesystem::file_size(found);
            if ((found != sTunePath) || (t != sTuneTime) || (size != sTuneSize)) {
                std::ifstream in(found, std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                nlohmann::json j = nlohmann::json::parse(text, nullptr, false, true); // (comments allowed)
                // Only taken (and the file marked as read) once it parses: a file caught half written, while it's
                // being saved or copied in, is read again on the next check instead of being skipped for good
                if (j.is_object()) {
                    sTune = j;
                    sTunePath = found;
                    sTuneTime = t;
                    sTuneSize = size;
                }
            }
        }
        // The list of keys, for whoever edits the file
        if (sUsedDirty && (now - sLastDump > std::chrono::seconds(5))) {
            nlohmann::json out = nlohmann::json::object();
            for (auto& [k, v] : sUsed) {
                out[k] = v;
            }
            std::ofstream o(Ship::Context::GetPathRelativeToAppDirectory("ganondorf_tuning_keys.json"));
            o << out.dump(2);
            sUsedDirty = false;
            sLastDump = now;
        }
    } catch (...) {
        // (a half-written or locked file: keep what was loaded, try again shortly)
    }
}

void Note(const std::string& key, const nlohmann::json& v) {
    auto it = sUsed.find(key);
    if ((it == sUsed.end()) || (it->second != v)) {
        sUsed[key] = v;
        sUsedDirty = true;
    }
}

} // namespace

extern "C" float GdfTune(const char* key, float def) {
    Refresh();
    float v = def;
    auto it = sTune.find(key);
    if ((it != sTune.end()) && it->is_number()) {
        v = it->get<float>();
    }
    Note(key, v);
    return v;
}

extern "C" float GdfTuneAt(const char* key, int index, float def) {
    Refresh();
    float v = def;
    auto it = sTune.find(key);
    if ((it != sTune.end()) && it->is_array() && (index >= 0) && (index < (int)it->size()) && (*it)[index].is_number()) {
        v = (*it)[index].get<float>();
    }
    auto used = sUsed.find(key);
    nlohmann::json arr = ((used != sUsed.end()) && used->second.is_array()) ? used->second : nlohmann::json::array();
    while ((int)arr.size() <= index) {
        arr.push_back(0);
    }
    arr[index] = v;
    Note(key, arr);
    return v;
}
