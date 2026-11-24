#pragma once

#include "Types.h"
#include "Assets.h"
#include "GameLogic.h"

#include <Gorgon/Scene.h>
#include <Gorgon/Widgets/Button.h>
#include <Gorgon/Widgets/Label.h>
#include <Gorgon/Widgets/Checkbox.h>
#include <Gorgon/Widgets/Textbox.h>
#include <Gorgon/Widgets/Numberbox.h>
#include <Gorgon/Widgets/Progressbar.h>
#include <Gorgon/Widgets/DialogWindow.h>
#include <Gorgon/UI/Organizers/Flow.h>

#include <memory>
#include <string>

namespace NeonVortex {

struct AppContext {
    std::string savePath = "neon_vortex_save.json";
    SaveData saveData;
    int lastScore = 0;
    int lastWave = 1;
    int lastTotalKills = 0;
    int lastChaserKills = 0;
    int lastSpinnerKills = 0;
    int lastTankKills = 0;
    bool smokeMode = false;
    int smokeFrames = 0;

    static AppContext &Get() {
        static AppContext ctx;
        return ctx;
    }
};

class MainMenuScene : public Gorgon::Scene {
public:
    MainMenuScene(Gorgon::SceneManager &parent, Gorgon::SceneID id);
    bool RequiresKeyInput() const override { return true; }

protected:
    void first_activation() override;
    void activate() override;
    void doframe(unsigned delta) override;
    void render() override;

private:
    void openSettings();
    void applySettings();

    Gorgon::Widgets::Button btnPlay;
    Gorgon::Widgets::Button btnSettings;
    Gorgon::Widgets::Button btnQuit;

    // Settings DialogWindow using UI::Organizers::Flow
    Gorgon::Widgets::DialogWindow settingsDlg;
    Gorgon::UI::Organizers::Flow settingsFlow;
    Gorgon::Widgets::Textbox txtName;
    Gorgon::Widgets::Integerbox numVolume;
    Gorgon::Widgets::Checkbox chkGlow;
    Gorgon::Widgets::Checkbox chkAudio;
    Gorgon::Widgets::Button btnDiffEasy;
    Gorgon::Widgets::Button btnDiffNorm;
    Gorgon::Widgets::Button btnDiffHard;
    int pendingDiff = 1;

    float animTime = 0.f;
};

class GameScene : public Gorgon::Scene {
public:
    GameScene(Gorgon::SceneManager &parent, Gorgon::SceneID id);
    bool RequiresKeyInput() const override { return true; }
    void KeyEvent(Gorgon::Input::Key key, float state) override;
    void StartFresh();

    static GameScene *Current() { return current; }

protected:
    void first_activation() override;
    void activate() override;
    void deactivate() override;
    void doframe(unsigned delta) override;
    void render() override;

private:
    void wireEvents();
    void updateAimFromMouse();

    std::unique_ptr<GameEngine> engine;
    Gorgon::Widgets::Progressbar hpBar;
    Gorgon::Widgets::Progressbar energyBar;
    Gorgon::Geometry::Pointf mouseScreenPos{480.f, 200.f};
    bool paused = false;
    float waveBannerTimer = 0.f;
    static GameScene *current;
};

class GameOverScene : public Gorgon::Scene {
public:
    GameOverScene(Gorgon::SceneManager &parent, Gorgon::SceneID id);
    bool RequiresKeyInput() const override { return true; }

protected:
    void first_activation() override;
    void activate() override;
    void doframe(unsigned delta) override;
    void render() override;

private:
    Gorgon::Widgets::Button btnRetry;
    Gorgon::Widgets::Button btnMenu;
    float animTime = 0.f;
};

} // namespace NeonVortex
