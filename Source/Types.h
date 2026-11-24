#pragma once

#include <Gorgon/Scene.h>
#include <Gorgon/Struct.h>
#include <Gorgon/Encoding/JSON.h>
#include <Gorgon/Filesystem.h>
#include <Gorgon/Utils/Logging.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace NeonVortex {

enum SceneIDs : Gorgon::SceneID {
    SCENE_MENU     = 1,
    SCENE_GAME     = 2,
    SCENE_GAMEOVER = 3,
};

struct GameSettings {
    std::string playerName = "Pilot";
    int difficulty         = 1;     // 0 = Easy, 1 = Normal, 2 = Hard
    int volume             = 75;    // 0..100
    bool glowEnabled       = true;
    bool audioEnabled      = true;

    DefineStructMembers(GameSettings, playerName, difficulty, volume, glowEnabled, audioEnabled)
};

struct HighScoreEntry {
    std::string name = "Pilot";
    int score        = 0;
    int wave         = 1;
    int difficulty   = 1;

    DefineStructMembers(HighScoreEntry, name, score, wave, difficulty)
};

struct SaveData {
    GameSettings settings;
    std::vector<HighScoreEntry> highScores;

    DefineStructMembers(SaveData, settings, highScores)
};

inline Gorgon::Utils::Logger &GameLog() {
    static Gorgon::Utils::Logger logger("NeonVortex", true, false);
    return logger;
}

class SaveManager {
public:
    static Gorgon::Encoding::JSON::Schema SettingsSchema() {
        using Gorgon::Encoding::JSON;
        return {
            {"playerName",   {JSON::Type::String,  false, JSON::Value("Pilot")}},
            {"difficulty",   {JSON::Type::Integer, false, JSON::Value(1)}},
            {"volume",       {JSON::Type::Integer, false, JSON::Value(75)}},
            {"glowEnabled",  {JSON::Type::Bool,    false, JSON::Value(true)}},
            {"audioEnabled", {JSON::Type::Bool,    false, JSON::Value(true)}},
        };
    }

    static bool Save(const std::string &path, const SaveData &data) {
        try {
            using Gorgon::Encoding::JSON;
            JSON::Object root;
            root["settings"] = JSON::Value::FromStruct(data.settings);
            JSON::Array scores;
            scores.reserve(data.highScores.size());
            for(const auto &e : data.highScores) {
                scores.push_back(JSON::Value::FromStruct(e));
            }
            root["highScores"] = JSON::Value(std::move(scores));
            std::string encoded = Gorgon::Encoding::Json.Encode(JSON::Value(std::move(root)), 2);
            std::ofstream out(path, std::ios::trunc);
            if(!out) return false;
            out << encoded;
            return out.good();
        }
        catch(...) {
            return false;
        }
    }

    static SaveData Load(const std::string &path) {
        SaveData data;
        if(!Gorgon::Filesystem::IsFile(path)) {
            return data;
        }
        try {
            std::ifstream in(path);
            std::stringstream ss;
            ss << in.rdbuf();
            auto root = Gorgon::Encoding::Json.Parse(ss.str());
            if(root.IsObject() && root.Has("settings")) {
                auto validated = Gorgon::Encoding::Json.Validate(root["settings"], SettingsSchema());
                data.settings = validated.ToStruct<GameSettings>();
            }
            if(root.IsObject() && root.Has("highScores") && root["highScores"].IsArray()) {
                data.highScores.clear();
                const auto &arr = root["highScores"];
                for(int i = 0; i < arr.GetCount(); ++i) {
                    data.highScores.push_back(arr[i].ToStruct<HighScoreEntry>());
                }
            }
            data.settings.difficulty = std::clamp(data.settings.difficulty, 0, 2);
            data.settings.volume = std::clamp(data.settings.volume, 0, 100);
        }
        catch(const std::exception &e) {
            GameLog() << "Save load fallback: " << e.what();
        }
        return data;
    }

    static void RecordScore(SaveData &data, const HighScoreEntry &entry) {
        data.highScores.push_back(entry);
        std::sort(data.highScores.begin(), data.highScores.end(),
            [](const HighScoreEntry &a, const HighScoreEntry &b) {
                return a.score > b.score;
            });
        if(data.highScores.size() > 8) {
            data.highScores.resize(8);
        }
    }
};

} // namespace NeonVortex
