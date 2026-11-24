#include "Scenes.h"

#include <Gorgon/Widgets/Registry.h>
#include <Gorgon/Input/Keyboard.h>
#include <Gorgon/Input/Mouse.h>
#include <Gorgon/Graphics/Color.h>
#include <Gorgon/WindowManager.h>

#include <cmath>
#include <sstream>

namespace NeonVortex {

using namespace Gorgon;
namespace W = Gorgon::Widgets;
namespace Color = Gorgon::Graphics::Color;
using Gorgon::Graphics::RGBA;
using Gorgon::Graphics::RGBAf;
using Gorgon::Geometry::Point;
using Gorgon::Geometry::Pointf;
using Gorgon::Geometry::Size;

namespace {
const char *DiffLabel(int d) {
    if(d == 0) return "Easy";
    if(d == 2) return "Hard";
    return "Normal";
}
} // namespace

// =============================================================================
// MainMenuScene
// =============================================================================
MainMenuScene::MainMenuScene(SceneManager &parent, SceneID id)
    : Scene(parent, id),
      btnPlay("Play  [Enter]"),
      btnSettings("Settings"),
      btnQuit("Quit  [Esc]"),
      settingsDlg("Neon Vortex Settings"),
      chkGlow("Gaussian Glow Halos", true),
      chkAudio("Procedural Audio SFX", true),
      btnDiffEasy("Easy"),
      btnDiffNorm("Normal"),
      btnDiffHard("Hard")
{
    btnPlay.Resize(8, 1);
    btnSettings.Resize(8, 1);
    btnQuit.Resize(8, 1);

    btnPlay.Move(2, 6);
    btnSettings.Move(2, 8);
    btnQuit.Move(2, 10);

    btnPlay.ClickEvent.Register([this]() {
        if(auto *gs = GameScene::Current()) gs->StartFresh();
        this->parent->SwitchScene(SCENE_GAME);
    });
    btnSettings.ClickEvent.Register([this]() { openSettings(); });
    btnQuit.ClickEvent.Register([this]() { this->parent->Quit(); });

    ui.Add(btnPlay);
    ui.Add(btnSettings);
    ui.Add(btnQuit);

    // Build Settings DialogWindow with Flow organizer
    settingsDlg.ResizeInterior({16, 10});
    settingsDlg.AttachOrganizer(settingsFlow);
    btnDiffEasy.SetWidth(4);
    btnDiffNorm.SetWidth(4);
    btnDiffHard.SetWidth(4);
    btnDiffEasy.ClickEvent.Register([this]() { pendingDiff = 0; });
    btnDiffNorm.ClickEvent.Register([this]() { pendingDiff = 1; });
    btnDiffHard.ClickEvent.Register([this]() { pendingDiff = 2; });

    settingsFlow
        << 5 << "Pilot Name:" << 10 << txtName << settingsFlow.Break
        << 5 << "Volume %:"   << 4  << numVolume << settingsFlow.Break
        << 5 << "Difficulty:" << btnDiffEasy << btnDiffNorm << btnDiffHard << settingsFlow.Break
        << settingsFlow.Break
        << 14 << chkGlow << settingsFlow.Break
        << 14 << chkAudio << settingsFlow.Break;

    settingsDlg.AddButton("Save", [this]() {
        applySettings();
        settingsDlg.Hide();
    });
    auto &cancelBtn = settingsDlg.AddButton("Cancel", [this]() {
        settingsDlg.Hide();
    });
    settingsDlg.SetCancel(cancelBtn);
    settingsDlg.Center();
    settingsDlg.Hide();

    // Scene-scoped Shortcuts
    Shortcuts.Set(Input::Keyboard::Keycodes::Enter, [this]() {
        if(!settingsDlg.IsVisible()) {
            if(auto *gs = GameScene::Current()) gs->StartFresh();
            this->parent->SwitchScene(SCENE_GAME);
        }
    }, Color::Green);
    Shortcuts.Set(Input::Keyboard::Keycodes::Escape, [this]() {
        if(settingsDlg.IsVisible()) settingsDlg.Hide();
        else this->parent->Quit();
    }, Color::Red);
}

void MainMenuScene::first_activation() {
    auto &assets = GameAssets::Get();
    assets.PrepareGPU();
    const auto &s = AppContext::Get().saveData.settings;
    assets.InitAudioPlayback(s.volume / 100.0f, s.audioEnabled);

    // Set procedural window icon
    if(assets.windowIcon.HasData()) {
        WindowManager::Icon ico(assets.windowIcon.GetData());
        parent->SetIcon(ico);
    }
}

void MainMenuScene::activate() {
    animTime = 0.f;
}

void MainMenuScene::openSettings() {
    const auto &s = AppContext::Get().saveData.settings;
    txtName = s.playerName;
    numVolume = s.volume;
    chkGlow = s.glowEnabled;
    chkAudio = s.audioEnabled;
    pendingDiff = s.difficulty;
    settingsDlg.Center();
    settingsDlg.Show();
}

void MainMenuScene::applySettings() {
    auto &ctx = AppContext::Get();
    auto &s = ctx.saveData.settings;
    std::string name = txtName;
    if(!name.empty()) s.playerName = name;
    s.volume = std::clamp(int(numVolume), 0, 100);
    s.glowEnabled = bool(chkGlow);
    s.audioEnabled = bool(chkAudio);
    s.difficulty = pendingDiff;
    GameAssets::Get().SetAudioVolume(s.volume / 100.0f, s.audioEnabled);
    SaveManager::Save(ctx.savePath, ctx.saveData);
}

void MainMenuScene::doframe(unsigned delta) {
    animTime += delta / 1000.0f;
    auto &ctx = AppContext::Get();
    if(ctx.smokeMode) {
        ctx.smokeFrames++;
        if(ctx.smokeFrames >= 12) {
            if(auto *gs = GameScene::Current()) gs->StartFresh();
            parent->SwitchScene(SCENE_GAME);
        }
    }
}

void MainMenuScene::render() {
    graphics.Clear();
    auto &assets = GameAssets::Get();
    auto sz = graphics.GetCalculatedSize();
    assets.bgFill.DrawIn(graphics);

    // Animated neon grid
    int step = 48;
    int offset = int(animTime * 28.f) % step;
    for(int x = 0; x < sz.Width; x += step)
        assets.gridLine.DrawIn(graphics, x, 0, 1, sz.Height);
    for(int y = offset; y < sz.Height; y += step)
        assets.gridLine.DrawIn(graphics, 0, y, sz.Width, 1);

    // Showcase procedural sprites & animated spinner on the right
    Pointf demoCenter{sz.Width * 0.72f, 150.f};
    if(AppContext::Get().saveData.settings.glowEnabled) {
        assets.glowHalo.Draw(graphics, demoCenter - Pointf{32.f, 32.f}, RGBAf(0.15f, 0.85f, 1.0f, 0.55f));
    }
    assets.playerShip.Draw(graphics, demoCenter - Pointf{20.f, 20.f});
    assets.spinnerInstance.Draw(graphics, Pointf{demoCenter.X + 70.f, demoCenter.Y - 18.f});
    assets.enemyChaser.Draw(graphics, Pointf{demoCenter.X - 95.f, demoCenter.Y - 16.f});
    assets.enemyTank.Draw(graphics, Pointf{demoCenter.X - 24.f, demoCenter.Y + 48.f});

    auto h1 = W::Registry::Active().Printer(Graphics::NamedFont::H1);
    auto h2 = W::Registry::Active().Printer(Graphics::NamedFont::H2);
    auto reg = W::Registry::Active().Printer(Graphics::NamedFont::Regular);

    h1.SetColor(RGBA{90, 240, 255, 255});
    h1.Print(graphics, "NEON VORTEX", 48, 36);

    reg.SetColor(RGBA{175, 200, 235, 255});
    const auto &s = AppContext::Get().saveData.settings;
    reg.Print(graphics,
        "Gorgon C++17 Showcase  |  Pilot: " + s.playerName +
        "  |  Difficulty: " + DiffLabel(s.difficulty),
        48, 92);

    reg.SetColor(RGBA{150, 180, 215, 255});
    reg.Print(graphics,
        "Controls: WASD Move | Mouse Aim & LMB Shoot | RMB/Q Nova | Space/Shift Dash | P Pause | M Mute",
        48, sz.Height - 42);

    // High scores table on the right
    h2.SetColor(RGBA{255, 215, 85, 255});
    h2.Print(graphics, "HIGH SCORES", int(sz.Width * 0.55f), 260);

    const auto &scores = AppContext::Get().saveData.highScores;
    int y = 305;
    if(scores.empty()) {
        reg.SetColor(RGBA{130, 145, 175, 255});
        reg.Print(graphics, "No runs recorded yet. Hit Play!", int(sz.Width * 0.55f), y);
    } else {
        int rank = 1;
        for(const auto &e : scores) {
            reg.SetColor(rank == 1 ? RGBA{255, 230, 110, 255} : RGBA{210, 225, 245, 255});
            std::ostringstream line;
            line << rank << ". " << e.name << "  -  " << e.score << " pts (Wave " << e.wave << ", " << DiffLabel(e.difficulty) << ")";
            reg.Print(graphics, line.str(), int(sz.Width * 0.55f), y);
            y += 28;
            if(++rank > 7) break;
        }
    }
}

// =============================================================================
// GameScene
// =============================================================================
GameScene *GameScene::current = nullptr;

GameScene::GameScene(SceneManager &parent, SceneID id)
    : Scene(parent, id),
      hpBar(100, 100),
      energyBar(100, 100)
{
    current = this;
    engine = std::make_unique<GameEngine>(Size{1600, 1100});
    engine->SetViewportSize({960, 640});
    wireEvents();

    hpBar.Resize(8, 1);
    hpBar.Move(1, 0);
    energyBar.Resize(8, 1);
    energyBar.Move(10, 0);
    ui.Add(hpBar);
    ui.Add(energyBar);

    // Mouse Input::Layer for aiming, firing, and Nova
    mouse.SetMove([this](Point p) {
        mouseScreenPos = {float(p.X), float(p.Y)};
        updateAimFromMouse();
    });
    mouse.SetDown([this](Point p, Input::Mouse::Button btn) {
        if(!engine || paused || engine->IsDying()) return;
        mouseScreenPos = {float(p.X), float(p.Y)};
        updateAimFromMouse();
        if(btn == Input::Mouse::Button::Left) {
            engine->Player().firing = true;
        } else if(btn == Input::Mouse::Button::Right) {
            engine->TriggerNova();
        }
    });
    mouse.SetUp([this](Point, Input::Mouse::Button btn) {
        if(!engine) return;
        if(btn == Input::Mouse::Button::Left) {
            engine->Player().firing = false;
        }
    });

    // Scene-scoped Shortcuts
    Shortcuts.Set(Input::Keyboard::Keycodes::P, [this]() {
        paused = !paused;
    }, Color::Yellow);
    Shortcuts.Set(Input::Keyboard::Keycodes::M, [this]() {
        auto &s = AppContext::Get().saveData.settings;
        s.audioEnabled = !s.audioEnabled;
        GameAssets::Get().SetAudioVolume(s.volume / 100.0f, s.audioEnabled);
    }, Color::Cyan);
    Shortcuts.Set(Input::Keyboard::Keycodes::R, [this]() {
        StartFresh();
    }, Color::Orange);
    Shortcuts.Set(Input::Keyboard::Keycodes::Escape, [this]() {
        this->parent->SwitchScene(SCENE_MENU);
    }, Color::Red);
}

void GameScene::wireEvents() {
    engine->OnShoot.Register([]() { GameAssets::Get().PlaySfx(SfxId::Shoot, 0.55f); });
    engine->OnEnemyHit.Register([]() { GameAssets::Get().PlaySfx(SfxId::Hit, 0.5f); });
    engine->OnEnemyKilled.Register([](EnemyKind k) {
        GameAssets::Get().PlaySfx(SfxId::Explode, k == EnemyKind::Tank ? 0.95f : 0.7f);
    });
    engine->OnPickup.Register([](PowerupKind) { GameAssets::Get().PlaySfx(SfxId::Pickup, 0.8f); });
    engine->OnDash.Register([]() { GameAssets::Get().PlaySfx(SfxId::Dash, 0.75f); });
    engine->OnNova.Register([]() { GameAssets::Get().PlaySfx(SfxId::Nova, 0.95f); });
    engine->OnPlayerDamaged.Register([](int) { GameAssets::Get().PlaySfx(SfxId::Hit, 0.9f); });
    engine->OnPlayerDied.Register([]() {
        GameAssets::Get().PlaySfx(SfxId::Explode, 1.0f);
        GameAssets::Get().PlaySfx(SfxId::Nova, 0.65f);
    });
    engine->OnWaveStarted.Register([this](int w) {
        waveBannerTimer = 2.0f;
        GameLog() << "Wave " << w << " started";
    });
    engine->OnGameOver.Register([this](int finalScore) {
        GameAssets::Get().PlaySfx(SfxId::GameOver, 0.85f);
        auto &ctx = AppContext::Get();
        ctx.lastScore = finalScore;
        ctx.lastWave = engine->GetWave();
        ctx.lastTotalKills = engine->GetTotalKills();
        ctx.lastChaserKills = engine->GetKillCount("Chaser");
        ctx.lastSpinnerKills = engine->GetKillCount("Spinner");
        ctx.lastTankKills = engine->GetKillCount("Tank");
        SaveManager::RecordScore(ctx.saveData, {
            ctx.saveData.settings.playerName,
            ctx.lastScore,
            ctx.lastWave,
            ctx.saveData.settings.difficulty
        });
        SaveManager::Save(ctx.savePath, ctx.saveData);
    });
}

void GameScene::updateAimFromMouse() {
    if(!engine) return;
    auto sz = graphics.GetCalculatedSize();
    if(sz.Width <= 0 || sz.Height <= 0) sz = engine->GetViewportSize();
    Pointf camTopLeft = engine->GetCameraPos() - Pointf{sz.Width * 0.5f, sz.Height * 0.5f};
    engine->SetAim(mouseScreenPos + camTopLeft);
}

void GameScene::StartFresh() {
    paused = false;
    int diff = AppContext::Get().saveData.settings.difficulty;
    engine->Reset(diff, static_cast<unsigned>(Time::GetTime()));
    hpBar.SetMaximum(engine->GetPlayer().maxHp);
    hpBar = engine->GetPlayer().hp;
    energyBar.SetMaximum(100);
    energyBar = 100;
}

void GameScene::first_activation() {
    GameAssets::Get().PrepareGPU();
    StartFresh();
}

void GameScene::activate() {
    paused = false;
    if(engine) {
        engine->Player().firing = false;
        if(parent) engine->SetViewportSize(parent->GetInteriorSize());
    }
    // Window::Add(Layer&) inserts into contentslayer, which sits UNDER UI::Window::widgetlayer
    // (where Scene::ui's full-window Panel lives and swallows all mouse events).
    // Attach mouse directly to the top-level Layer stack so it sits above widgetlayer.
    if(parent) {
        ui.Resize(UI::Pixels(parent->GetInteriorSize().Width, 44));
        parent->Gorgon::Layer::Add(mouse);
    }
}

void GameScene::deactivate() {
    if(engine) {
        engine->Player().firing = false;
    }
    if(mouse.HasParent()) {
        mouse.GetParent().Remove(mouse);
    }
}

void GameScene::KeyEvent(Input::Key key, float state) {
    using namespace Input::Keyboard;
    bool down = (state > 0.5f);
    auto &p = engine->Player();
    if(key == Keycodes::W || key == Keycodes::Up)    p.moveUp    = down;
    if(key == Keycodes::S || key == Keycodes::Down)  p.moveDown  = down;
    if(key == Keycodes::A || key == Keycodes::Left)  p.moveLeft  = down;
    if(key == Keycodes::D || key == Keycodes::Right) p.moveRight = down;
    if(down && (key == Keycodes::Space || key == Keycodes::Shift)) {
        if(!paused && !engine->IsDying()) engine->TriggerDash();
    }
    if(down && key == Keycodes::Q) {
        if(!paused && !engine->IsDying()) engine->TriggerNova();
    }
}

void GameScene::doframe(unsigned delta) {
    auto &ctx = AppContext::Get();
    if(ctx.smokeMode) {
        engine->Player().firing = true;
        engine->Player().moveRight = true;
        mouseScreenPos = {620.f, 260.f};
        if(ctx.smokeFrames == 20) engine->TriggerDash();
        if(ctx.smokeFrames == 30) engine->TriggerNova();
        ctx.smokeFrames++;
        if(ctx.smokeFrames >= 45) {
            ctx.lastScore = engine->GetScore();
            ctx.lastWave = engine->GetWave();
            parent->SwitchScene(SCENE_GAMEOVER);
            return;
        }
    }

    if(paused) return;

    auto curSz = graphics.GetCalculatedSize();
    if(curSz.Width > 0 && curSz.Height > 0) {
        engine->SetViewportSize(curSz);
    }
    updateAimFromMouse();

    if(waveBannerTimer > 0.f) {
        waveBannerTimer = std::max(0.f, waveBannerTimer - delta / 1000.0f);
    }

    bool alive = engine->Update(delta);
    hpBar = engine->GetPlayer().hp;
    energyBar = int(engine->GetPlayer().energy);

    if(!alive) {
        parent->SwitchScene(SCENE_GAMEOVER);
    }
}

void GameScene::render() {
    graphics.Clear();
    auto &assets = GameAssets::Get();
    const bool glow = AppContext::Get().saveData.settings.glowEnabled;
    auto sz = graphics.GetCalculatedSize();

    // Screen shake offset
    Pointf shake{0.f, 0.f};
    if(engine->GetScreenShake() > 0.1f) {
        float s = engine->GetScreenShake();
        shake.X = ((std::rand() % 100) / 50.f - 1.f) * s;
        shake.Y = ((std::rand() % 100) / 50.f - 1.f) * s;
    }

    // Camera top-left in world space (with screen shake)
    Pointf camTopLeft = engine->GetCameraPos() - Pointf{sz.Width * 0.5f, sz.Height * 0.5f} - shake;
    auto toScreen = [&](Pointf w) -> Pointf { return w - camTopLeft; };

    // Background + parallax scrolling stars
    assets.bgFill.DrawIn(graphics);
    for(const auto &st : engine->GetStars()) {
        float parallax = 0.25f + 0.35f * st.brightness;
        float sx = std::fmod(st.pos.X - camTopLeft.X * parallax, float(sz.Width));
        float sy = std::fmod(st.pos.Y - camTopLeft.Y * parallax, float(sz.Height));
        if(sx < 0.f) sx += sz.Width;
        if(sy < 0.f) sy += sz.Height;
        assets.whitePixel.Draw(graphics, Pointf{sx, sy}, RGBAf(0.6f, 0.8f, 1.0f, st.brightness * 0.7f));
    }

    // World-space arena grid & neon arena border
    auto arenaSz = engine->GetArenaSize();
    int startX = std::max(0, (int(camTopLeft.X) / 64) * 64);
    int endX   = std::min(arenaSz.Width, int(camTopLeft.X) + sz.Width + 64);
    int startY = std::max(0, (int(camTopLeft.Y) / 64) * 64);
    int endY   = std::min(arenaSz.Height, int(camTopLeft.Y) + sz.Height + 64);
    float gridTop = std::max(46.f, -camTopLeft.Y);
    float gridBot = std::min(float(sz.Height), float(arenaSz.Height) - camTopLeft.Y);
    if(gridBot > gridTop) {
        for(int x = startX; x <= endX; x += 64) {
            assets.gridLine.DrawIn(graphics, int(x - camTopLeft.X), int(gridTop), 1, int(gridBot - gridTop));
        }
    }
    float gridLeft  = std::max(0.f, -camTopLeft.X);
    float gridRight = std::min(float(sz.Width), float(arenaSz.Width) - camTopLeft.X);
    if(gridRight > gridLeft) {
        for(int y = startY; y <= endY; y += 64) {
            int sy = int(y - camTopLeft.Y);
            if(sy >= 46 && sy <= sz.Height) {
                assets.gridLine.DrawIn(graphics, int(gridLeft), sy, int(gridRight - gridLeft), 1);
            }
        }
    }
    // Arena boundary outline
    Pointf bTL = toScreen({0.f, 0.f});
    Pointf bBR = toScreen({float(arenaSz.Width), float(arenaSz.Height)});
    RGBAf borderCol(0.25f, 0.85f, 1.0f, 0.55f);
    assets.whitePixel.DrawStretched(graphics, bTL.X, bTL.Y, bBR.X - bTL.X, 2.f, borderCol);
    assets.whitePixel.DrawStretched(graphics, bTL.X, bBR.Y - 2.f, bBR.X - bTL.X, 2.f, borderCol);
    assets.whitePixel.DrawStretched(graphics, bTL.X, bTL.Y, 2.f, bBR.Y - bTL.Y, borderCol);
    assets.whitePixel.DrawStretched(graphics, bBR.X - 2.f, bTL.Y, 2.f, bBR.Y - bTL.Y, borderCol);

    // Nova shockwave ring
    if(engine->GetNovaWaveRadius() > 0.f && glow) {
        float r = engine->GetNovaWaveRadius();
        Pointf np = toScreen(engine->GetPlayer().pos);
        assets.glowHalo.DrawStretched(graphics,
            np.X - r, np.Y - r, r * 2.f, r * 2.f,
            RGBAf(0.3f, 0.95f, 1.0f, std::max(0.f, 1.f - r / 240.f)));
    }

    // Player death explosion shockwave & flash
    if(engine->GetDeathWaveRadius() > 0.f) {
        float r = engine->GetDeathWaveRadius();
        Pointf dp = toScreen(engine->GetDeathPos());
        float alpha = std::max(0.f, 1.f - r / 420.f);
        assets.glowHalo.DrawStretched(graphics,
            dp.X - r, dp.Y - r, r * 2.f, r * 2.f,
            RGBAf(1.0f, 0.55f, 0.2f, alpha * 0.9f));
        float coreR = r * 0.45f;
        assets.glowHalo.DrawStretched(graphics,
            dp.X - coreR, dp.Y - coreR, coreR * 2.f, coreR * 2.f,
            RGBAf(0.85f, 0.98f, 1.0f, alpha * alpha));
    }

    // Powerups
    for(const auto &p : engine->GetPowerups()) {
        Pointf sp = toScreen(p.pos);
        auto &bmp = (p.kind == PowerupKind::Heal) ? assets.powerupHeal
                  : (p.kind == PowerupKind::RapidFire ? assets.powerupRapid : assets.powerupShield);
        if(glow) {
            assets.glowHalo.Draw(graphics, sp - Pointf{32.f, 32.f}, RGBAf(0.4f, 1.0f, 0.6f, 0.38f));
        }
        bmp.Draw(graphics, sp - Pointf{12.f, 12.f});
    }

    // Bullets
    for(const auto &b : engine->GetBullets()) {
        Pointf sp = toScreen(b.pos);
        auto &bmp = b.fromPlayer ? assets.bulletPlayer : assets.bulletEnemy;
        auto [bw, bh] = bmp.GetSize();
        bmp.Draw(graphics, sp - Pointf{bw * 0.5f, bh * 0.5f});
    }

    // Enemies
    for(const auto &e : engine->GetEnemies()) {
        Pointf sp = toScreen(e.pos);
        RGBAf tint = (e.hitFlash > 0.f) ? RGBAf(1.f, 1.f, 1.f, 1.f) : RGBAf(1.f, 1.f, 1.f, 1.f);
        if(glow) {
            RGBAf gc = (e.kind == EnemyKind::Tank) ? RGBAf(0.75f, 0.25f, 1.f, 0.35f)
                     : (e.kind == EnemyKind::Spinner ? RGBAf(1.f, 0.6f, 0.15f, 0.35f)
                                                     : RGBAf(1.f, 0.2f, 0.35f, 0.35f));
            assets.glowHalo.Draw(graphics, sp - Pointf{32.f, 32.f}, gc);
        }
        if(e.kind == EnemyKind::Chaser) {
            assets.enemyChaser.Draw(graphics, sp - Pointf{16.f, 16.f}, tint);
        } else if(e.kind == EnemyKind::Spinner) {
            assets.spinnerInstance.Draw(graphics, sp - Pointf{18.f, 18.f}, tint);
        } else {
            assets.enemyTank.Draw(graphics, sp - Pointf{24.f, 24.f}, tint);
        }
    }

    // Particles
    for(const auto &pt : engine->GetParticles()) {
        Pointf sp = toScreen(pt.pos);
        float a = std::clamp(pt.life / pt.maxLife, 0.f, 1.f);
        RGBAf col(pt.color.R / 255.f, pt.color.G / 255.f, pt.color.B / 255.f, a);
        if(pt.size != 1.0f) {
            float d = 8.f * pt.size;
            assets.particleDot.DrawStretched(graphics, sp.X - d * 0.5f, sp.Y - d * 0.5f, d, d, col);
        } else {
            assets.particleDot.Draw(graphics, sp - Pointf{4.f, 4.f}, col);
        }
    }

    // Player ship + optional shield + glow (hidden once exploded)
    const auto &pl = engine->GetPlayer();
    if(!engine->IsDying()) {
        Pointf psp = toScreen(pl.pos);
        if(glow) {
            assets.glowHalo.Draw(graphics, psp - Pointf{32.f, 32.f}, RGBAf(0.1f, 0.85f, 1.f, 0.45f));
        }
        float shipAlpha = (pl.invulnTimer > 0.f && int(pl.invulnTimer * 20) % 2 == 0) ? 0.4f : 1.0f;
        assets.playerShip.Draw(graphics, psp - Pointf{20.f, 20.f}, RGBAf(1.f, 1.f, 1.f, shipAlpha));
        if(pl.shieldTimer > 0.f) {
            assets.playerShield.Draw(graphics, psp - Pointf{28.f, 28.f});
        }

        // Aim crosshair (screen space)
        assets.crosshair.Draw(graphics, mouseScreenPos - Pointf{13.f, 13.f});
    }

    // Top HUD bar backdrop + text
    assets.hudBack.DrawIn(graphics, 0, 0, sz.Width, 44);
    auto reg = W::Registry::Active().Printer(Graphics::NamedFont::Regular);
    auto h1  = W::Registry::Active().Printer(Graphics::NamedFont::H1);

    reg.SetColor(RGBA{230, 245, 255, 255});
    std::ostringstream hud;
    hud << "SCORE: " << engine->GetScore()
        << "   WAVE: " << engine->GetWave()
        << "   COMBO: x" << engine->GetCombo();
    if(pl.rapidTimer > 0.f) hud << "   [RAPID]";
    if(pl.shieldTimer > 0.f) hud << "   [SHIELD]";
    reg.Print(graphics, hud.str(), 12, 12, sz.Width - 24, Graphics::TextAlignment::Right);

    if(waveBannerTimer > 0.f) {
        h1.SetColor(RGBAf(0.4f, 0.95f, 1.f, std::min(1.f, waveBannerTimer)));
        h1.Print(graphics, "WAVE " + std::to_string(engine->GetWave()),
                 0, sz.Height / 2 - 40, sz.Width, Graphics::TextAlignment::Center);
    }

    if(paused) {
        assets.pauseShade.DrawIn(graphics);
        h1.SetColor(RGBA{255, 230, 100, 255});
        h1.Print(graphics, "PAUSED", 0, sz.Height / 2 - 36, sz.Width, Graphics::TextAlignment::Center);
        reg.SetColor(Color::White);
        reg.Print(graphics, "Press P to Resume  |  R to Restart  |  Esc for Menu",
                  0, sz.Height / 2 + 16, sz.Width, Graphics::TextAlignment::Center);
    }
}

// =============================================================================
// GameOverScene
// =============================================================================
GameOverScene::GameOverScene(SceneManager &parent, SceneID id)
    : Scene(parent, id),
      btnRetry("Play Again [Enter]"),
      btnMenu("Main Menu [Esc]")
{
    btnRetry.Resize(9, 1);
    btnMenu.Resize(9, 1);
    btnRetry.Move(10, 11);
    btnMenu.Move(10, 13);

    btnRetry.ClickEvent.Register([this]() {
        if(auto *gs = GameScene::Current()) gs->StartFresh();
        this->parent->SwitchScene(SCENE_GAME);
    });
    btnMenu.ClickEvent.Register([this]() {
        this->parent->SwitchScene(SCENE_MENU);
    });

    ui.Add(btnRetry);
    ui.Add(btnMenu);

    Shortcuts.Set(Input::Keyboard::Keycodes::Enter, [this]() {
        if(auto *gs = GameScene::Current()) gs->StartFresh();
        this->parent->SwitchScene(SCENE_GAME);
    }, Color::Green);
    Shortcuts.Set(Input::Keyboard::Keycodes::Escape, [this]() {
        this->parent->SwitchScene(SCENE_MENU);
    }, Color::Red);
}

void GameOverScene::first_activation() {
    GameAssets::Get().PrepareGPU();
}

void GameOverScene::activate() {
    animTime = 0.f;
}

void GameOverScene::doframe(unsigned delta) {
    animTime += delta / 1000.0f;
    auto &ctx = AppContext::Get();
    if(ctx.smokeMode) {
        ctx.smokeFrames++;
        if(ctx.smokeFrames >= 58) {
            parent->Quit();
        }
    }
}

void GameOverScene::render() {
    graphics.Clear();
    auto &assets = GameAssets::Get();
    auto sz = graphics.GetCalculatedSize();
    assets.bgFill.DrawIn(graphics);

    auto h1  = W::Registry::Active().Printer(Graphics::NamedFont::H1);
    auto h2  = W::Registry::Active().Printer(Graphics::NamedFont::H2);
    auto reg = W::Registry::Active().Printer(Graphics::NamedFont::Regular);

    const auto &ctx = AppContext::Get();
    h1.SetColor(RGBA{255, 80, 110, 255});
    h1.Print(graphics, "GAME OVER", 0, 56, sz.Width, Graphics::TextAlignment::Center);

    h2.SetColor(RGBA{120, 245, 255, 255});
    h2.Print(graphics, "Final Score: " + std::to_string(ctx.lastScore) + "  (Wave " + std::to_string(ctx.lastWave) + ")",
             0, 126, sz.Width, Graphics::TextAlignment::Center);

    std::ostringstream stats;
    stats << "Total Kills: " << ctx.lastTotalKills
          << "   (Chasers: " << ctx.lastChaserKills
          << ", Spinners: " << ctx.lastSpinnerKills
          << ", Tanks: " << ctx.lastTankKills << ")";
    reg.SetColor(RGBA{210, 225, 245, 255});
    reg.Print(graphics, stats.str(), 0, 178, sz.Width, Graphics::TextAlignment::Center);
}

} // namespace NeonVortex
