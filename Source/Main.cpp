#include "Types.h"
#include "Assets.h"
#include "GameLogic.h"
#include "Scenes.h"

#include <Gorgon/Main.h>
#include <Gorgon/Scene.h>
#include <Gorgon/UI.h>
#include <Gorgon/Filesystem.h>
#include <Gorgon/Encoding/PNG.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

int RunSelfTest() {
    using namespace NeonVortex;
    int passed = 0;
    int failed = 0;
    auto check = [&](const char *name, bool cond) {
        if(cond) {
            std::cout << "  [PASS] " << name << "\n";
            passed++;
        } else {
            std::cerr << "  [FAIL] " << name << "\n";
            failed++;
        }
    };

    std::cout << "=== Neon Vortex — Gorgon Feature Self-Test ===\n";
    Gorgon::Initialize("NeonVortexTest");

    // 1. Procedural CGI + ImageProcessing Gaussian Convolution
    auto &assets = GameAssets::Get();
    assets.GenerateSprites();
    check("CGI sprites generated (playerShip 40x40)",
          assets.playerShip.HasData() && assets.playerShip.GetWidth() == 40 && assets.playerShip.GetHeight() == 40);
    check("CGI animated spinner (4 frames, 440ms)",
          assets.spinnerAnim.GetCount() == 4 && assets.spinnerAnim.GetDuration() == 440);
    check("ImageProcessing Gaussian glowHalo (64x64 non-empty)",
          assets.glowHalo.HasData() && assets.glowHalo.GetWidth() == 64 &&
          assets.glowHalo.GetData().GetRGBAAt(32, 32).A > 50 &&
          assets.glowHalo.GetData().GetRGBAAt(20, 32).A > 0);

    // 2. PNG Export & Import round-trip + Filesystem
    const std::string testPng = "nv_selftest_icon.png";
    bool pngExported = assets.windowIcon.ExportPNG(testPng);
    Gorgon::Graphics::Bitmap reimported;
    bool pngImported = pngExported && reimported.Import(testPng);
    check("PNG Export/Import + Filesystem round-trip",
          pngExported && pngImported && Gorgon::Filesystem::IsFile(testPng) && reimported.GetWidth() == 40);
    if(Gorgon::Filesystem::IsFile(testPng)) Gorgon::Filesystem::Delete(testPng);

    // 3. Procedural Audio Wave synthesis + WAV Export/Import
    assets.GenerateAudio();
    auto &shootWave = assets.rawWaves[static_cast<size_t>(SfxId::Shoot)];
    const std::string testWav = "nv_selftest_shoot.wav";
    bool wavExported = shootWave.ExportWav(testWav);
    Gorgon::Containers::Wave loadedWave;
    bool wavImported = wavExported && loadedWave.ImportWav(testWav);
    check("Containers::Wave synthesis + WAV export/import",
          shootWave.GetSize() > 3000 && wavExported && wavImported && loadedWave.GetSize() == shootWave.GetSize());
    if(Gorgon::Filesystem::IsFile(testWav)) Gorgon::Filesystem::Delete(testWav);

    // 4. JSON Reflection (DefineStructMembers) + Schema Validation + Save/Load
    const std::string testJson = "nv_selftest_save.json";
    SaveData orig;
    orig.settings.playerName = "AceTester";
    orig.settings.difficulty = 2;
    orig.settings.volume = 64;
    orig.settings.glowEnabled = false;
    SaveManager::RecordScore(orig, {"AceTester", 4200, 5, 2});
    SaveManager::RecordScore(orig, {"Rival", 1900, 3, 1});
    bool saved = SaveManager::Save(testJson, orig);
    SaveData loaded = SaveManager::Load(testJson);
    check("JSON struct reflection + schema validation + Save/Load",
          saved && loaded.settings.playerName == "AceTester" && loaded.settings.volume == 64 &&
          loaded.settings.glowEnabled == false && loaded.highScores.size() == 2 &&
          loaded.highScores[0].score == 4200);
    if(Gorgon::Filesystem::IsFile(testJson)) Gorgon::Filesystem::Delete(testJson);

    // 5. GameEngine deterministic simulation + Events + Containers::Hashmap
    GameEngine sim({960, 640}, 1337);
    int shootEvents = 0, killEvents = 0, pickupEvents = 0, dashEvents = 0, novaEvents = 0;
    sim.OnShoot.Register([&]() { shootEvents++; });
    sim.OnEnemyKilled.Register([&](EnemyKind) { killEvents++; });
    sim.OnPickup.Register([&](PowerupKind) { pickupEvents++; });
    sim.OnDash.Register([&]() { dashEvents++; });
    sim.OnNova.Register([&]() { novaEvents++; });

    // Place enemy directly ahead of player and fire
    sim.SpawnEnemyDirect(EnemyKind::Chaser, {480.f, 220.f});
    sim.SetAim({480.f, 220.f});
    sim.Player().firing = true;
    for(int i = 0; i < 60; ++i) sim.Update(16);
    check("Shooting, bullet collision, enemy kill & Events",
          shootEvents >= 3 && killEvents >= 1 && sim.GetScore() >= 100 && sim.GetKillCount("Chaser") >= 1);

    // Test Dash & Nova
    bool dashed = sim.TriggerDash();
    sim.Player().energy = 100.f;
    sim.SpawnEnemyDirect(EnemyKind::Spinner, {520.f, 320.f});
    bool novaed = sim.TriggerNova();
    sim.Update(16);
    check("Dash & Nova abilities + Events", dashed && novaed && dashEvents == 1 && novaEvents == 1);

    // Test Powerup pickup
    sim.Player().hp = 50;
    sim.SpawnPowerupDirect(PowerupKind::Heal, sim.GetPlayer().pos);
    sim.Update(16);
    check("Powerup pickup heals player & fires OnPickup",
          pickupEvents == 1 && sim.GetPlayer().hp == 80);

    // Test Death Sequence: explode, camera detaches, enemies stop for 1s, then separate, then Game Over
    int diedEvents = 0;
    int gameOverScore = -1;
    sim.OnPlayerDied.Register([&]() { diedEvents++; });
    sim.OnGameOver.Register([&](int s) { gameOverScore = s; });
    sim.SpawnEnemyDirect(EnemyKind::Chaser, sim.GetPlayer().pos + Gorgon::Geometry::Pointf{40.f, 0.f});
    sim.SpawnEnemyDirect(EnemyKind::Chaser, sim.GetPlayer().pos + Gorgon::Geometry::Pointf{46.f, 4.f});
    sim.Player().hp = 0;
    bool stillAlive = sim.Update(16);
    size_t enemyCount = sim.GetEnemies().size();
    auto e0AtDeath = sim.GetEnemies()[enemyCount - 2].pos;
    auto e1AtDeath = sim.GetEnemies()[enemyCount - 1].pos;
    auto camAtDeath = sim.GetCameraPos();
    check("Player death explodes & detaches camera",
          stillAlive && sim.IsDying() && diedEvents == 1 && sim.IsCameraDetached() &&
          sim.GetParticles().size() >= 80 && sim.GetDeathWaveRadius() > 0.f);

    // Advance 0.8s (within 1.0s freeze): enemies must stay stopped, camera drifts free
    for(int i = 0; i < 50; ++i) sim.Update(16);
    auto e0Frozen = sim.GetEnemies()[enemyCount - 2].pos;
    auto e1Frozen = sim.GetEnemies()[enemyCount - 1].pos;
    auto camDrifted = sim.GetCameraPos();
    float camDriftDist = std::hypot(camDrifted.X - camAtDeath.X, camDrifted.Y - camAtDeath.Y);
    check("Enemies stop for 1s & camera drifts free",
          std::hypot(e0Frozen.X - e0AtDeath.X, e0Frozen.Y - e0AtDeath.Y) < 0.01f &&
          std::hypot(e1Frozen.X - e1AtDeath.X, e1Frozen.Y - e1AtDeath.Y) < 0.01f &&
          camDriftDist > 10.f);

    // Advance past 1.0s freeze into separation, then to Game Over
    float initialPairDist = std::hypot(e0AtDeath.X - e1AtDeath.X, e0AtDeath.Y - e1AtDeath.Y);
    for(int i = 0; i < 60; ++i) sim.Update(16); // ~1.76s total
    auto e0Sep = sim.GetEnemies()[enemyCount - 2].pos;
    auto e1Sep = sim.GetEnemies()[enemyCount - 1].pos;
    float separatedPairDist = std::hypot(e0Sep.X - e1Sep.X, e0Sep.Y - e1Sep.Y);
    check("Enemies separate after 1s pause", separatedPairDist > initialPairDist + 20.f);

    while(stillAlive && sim.GetDeathTimer() < 5.f) {
        stillAlive = sim.Update(16);
    }
    check("Game Over transition & OnGameOver after death sequence", !stillAlive && gameOverScore == sim.GetScore());

    std::cout << "=== Result: " << passed << "/" << (passed + failed) << " passed ===\n";
    return failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
    bool selftest = false;
    bool smoke = false;
    for(int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if(a == "--selftest") selftest = true;
        if(a == "--smoke") smoke = true;
    }

    if(selftest) {
        return RunSelfTest();
    }

    using namespace NeonVortex;
    Gorgon::Initialize("NeonVortex");

    auto &ctx = AppContext::Get();
    ctx.smokeMode = smoke;
    ctx.saveData = SaveManager::Load(ctx.savePath);

    Gorgon::SceneManager manager({960, 640}, "Neon Vortex — Gorgon Showcase");
    Gorgon::UI::Initialize(6);

    manager.NewScene<MainMenuScene>(SCENE_MENU);
    manager.NewScene<GameScene>(SCENE_GAME);
    manager.NewScene<GameOverScene>(SCENE_GAMEOVER);
    manager.SwitchScene(SCENE_MENU);

    manager.Run();

    if(smoke) {
        std::cout << "[PASS] Vulkan SceneManager smoke test completed (" << ctx.smokeFrames << " frames across Menu -> Game -> GameOver)\n";
    }
    return 0;
}
