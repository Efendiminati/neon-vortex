#pragma once

#include <Gorgon/Geometry/Point.h>
#include <Gorgon/Geometry/Size.h>
#include <Gorgon/Geometry/Bounds.h>
#include <Gorgon/Graphics/Color.h>
#include <Gorgon/Containers/Hashmap.h>
#include <Gorgon/Event.h>

#include <random>
#include <string>
#include <vector>

namespace NeonVortex {

enum class EnemyKind { Chaser, Spinner, Tank };
enum class PowerupKind { Heal, RapidFire, Shield };

struct PlayerState {
    Gorgon::Geometry::Pointf pos{480.f, 320.f};
    Gorgon::Geometry::Pointf vel{0.f, 0.f};
    Gorgon::Geometry::Pointf aim{480.f, 200.f};
    int hp           = 100;
    int maxHp        = 100;
    float energy     = 100.f; // Used for Dash (35) and Nova (65)
    float maxEnergy  = 100.f;
    float fireTimer  = 0.f;
    float dashTimer  = 0.f;
    float invulnTimer = 0.f;
    float rapidTimer = 0.f;
    float shieldTimer = 0.f;
    bool moveUp      = false;
    bool moveDown    = false;
    bool moveLeft    = false;
    bool moveRight   = false;
    bool firing      = false;
};

struct Bullet {
    Gorgon::Geometry::Pointf pos;
    Gorgon::Geometry::Pointf vel;
    float life = 2.0f;
    bool fromPlayer = true;
    int damage = 20;
};

struct Enemy {
    EnemyKind kind = EnemyKind::Chaser;
    Gorgon::Geometry::Pointf pos;
    Gorgon::Geometry::Pointf vel;
    int hp = 30;
    int maxHp = 30;
    float radius = 14.f;
    float speed = 110.f;
    float shootCooldown = 1.8f;
    float hitFlash = 0.f;
};

struct Powerup {
    PowerupKind kind = PowerupKind::Heal;
    Gorgon::Geometry::Pointf pos;
    float life = 10.f;
};

struct Particle {
    Gorgon::Geometry::Pointf pos;
    Gorgon::Geometry::Pointf vel;
    Gorgon::Graphics::RGBA color;
    float life = 0.5f;
    float maxLife = 0.5f;
    float size = 1.0f;
};

struct Star {
    Gorgon::Geometry::Pointf pos;
    float speed;
    float brightness;
};

class GameEngine {
public:
    explicit GameEngine(Gorgon::Geometry::Size arenaSize = {960, 640}, unsigned seed = 42);

    void Reset(int difficulty, unsigned seed = 0);
    bool Update(unsigned deltaMs); // returns false when player dies

    bool TriggerDash();
    bool TriggerNova();
    void SetAim(Gorgon::Geometry::Pointf p) { player.aim = p; }
    void SetArenaSize(Gorgon::Geometry::Size s) {
        if(s.Width > 0 && s.Height > 0) arena = s;
    }
    void SetViewportSize(Gorgon::Geometry::Size s) {
        if(s.Width > 0 && s.Height > 0) viewport = s;
    }

    // Public read-only accessors for renderer & tests
    const PlayerState &GetPlayer() const { return player; }
    PlayerState &Player() { return player; }
    const std::vector<Bullet> &GetBullets() const { return bullets; }
    const std::vector<Enemy> &GetEnemies() const { return enemies; }
    const std::vector<Powerup> &GetPowerups() const { return powerups; }
    const std::vector<Particle> &GetParticles() const { return particles; }
    const std::vector<Star> &GetStars() const { return stars; }
    Gorgon::Geometry::Size GetArenaSize() const { return arena; }
    Gorgon::Geometry::Size GetViewportSize() const { return viewport; }
    Gorgon::Geometry::Pointf GetCameraPos() const { return cameraPos; }
    bool IsCameraDetached() const { return cameraDetached; }
    bool IsDying() const { return dying; }
    float GetDeathTimer() const { return deathTimer; }
    float GetDeathWaveRadius() const { return deathWaveRadius; }
    Gorgon::Geometry::Pointf GetDeathPos() const { return deathPos; }

    int GetScore() const { return score; }
    int GetWave() const { return wave; }
    int GetCombo() const { return combo; }
    int GetDifficulty() const { return difficulty; }
    float GetScreenShake() const { return screenShake; }
    float GetNovaWaveRadius() const { return novaWaveRadius; }
    int GetKillCount(const std::string &kind) const;
    int GetTotalKills() const { return totalKills; }

    // Test helpers
    void SpawnEnemyDirect(EnemyKind kind, Gorgon::Geometry::Pointf pos);
    void SpawnPowerupDirect(PowerupKind kind, Gorgon::Geometry::Pointf pos);

    // Gorgon Events for decoupled audio / logging / UI
    Gorgon::Event<> OnShoot;
    Gorgon::Event<> OnEnemyHit;
    Gorgon::Event<void, EnemyKind> OnEnemyKilled;
    Gorgon::Event<void, PowerupKind> OnPickup;
    Gorgon::Event<> OnDash;
    Gorgon::Event<> OnNova;
    Gorgon::Event<void, int> OnPlayerDamaged;
    Gorgon::Event<> OnPlayerDied;
    Gorgon::Event<void, int> OnWaveStarted;
    Gorgon::Event<void, int> OnGameOver;

    static constexpr float DEATH_FREEZE_DURATION = 1.0f;
    static constexpr float DEATH_TOTAL_DURATION  = 2.8f;

private:
    void spawnWaveEnemy();
    void emitParticles(Gorgon::Geometry::Pointf pos, Gorgon::Graphics::RGBA color, int count, float speed, float minLife = 0.22f, float maxLife = 0.55f, float size = 1.0f);
    void recordKill(EnemyKind kind);
    void beginPlayerDeath();
    void updateCamera(float dt);

    Gorgon::Geometry::Size arena;
    Gorgon::Geometry::Size viewport;
    std::mt19937 rng;
    PlayerState player;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    std::vector<Powerup> powerups;
    std::vector<Particle> particles;
    std::vector<Star> stars;

    Gorgon::Containers::Hashmap<std::string, int> killsByType;

    int difficulty   = 1;
    int score        = 0;
    int wave         = 1;
    int combo        = 1;
    float comboTimer = 0.f;
    int totalKills   = 0;
    int waveKills    = 0;
    int waveTarget   = 8;
    float spawnTimer = 0.8f;
    float screenShake = 0.f;
    float novaWaveRadius = -1.f;
    bool gameOverFired = false;

    // Camera & death sequence state
    Gorgon::Geometry::Pointf cameraPos{480.f, 320.f};
    Gorgon::Geometry::Pointf cameraVel{0.f, 0.f};
    bool cameraDetached = false;
    bool dying = false;
    float deathTimer = 0.f;
    float deathWaveRadius = -1.f;
    Gorgon::Geometry::Pointf deathPos{480.f, 320.f};
};

} // namespace NeonVortex
