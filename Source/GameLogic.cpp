#include "GameLogic.h"

#include <algorithm>
#include <cmath>

namespace NeonVortex {

using Gorgon::Geometry::Pointf;
using Gorgon::Geometry::Size;
using Gorgon::Graphics::RGBA;

namespace {
float DistSq(Pointf a, Pointf b) {
    float dx = a.X - b.X;
    float dy = a.Y - b.Y;
    return dx * dx + dy * dy;
}
Pointf Normalize(Pointf v) {
    float len = std::sqrt(v.X * v.X + v.Y * v.Y);
    if(len < 1e-4f) return {0.f, -1.f};
    return {v.X / len, v.Y / len};
}
std::string EnemyKindName(EnemyKind k) {
    switch(k) {
    case EnemyKind::Chaser:  return "Chaser";
    case EnemyKind::Spinner: return "Spinner";
    case EnemyKind::Tank:    return "Tank";
    }
    return "Unknown";
}
} // namespace

GameEngine::GameEngine(Size arenaSize, unsigned seed)
    : arena(arenaSize), viewport(arenaSize), rng(seed)
{
    Reset(1, seed);
}

void GameEngine::Reset(int diff, unsigned seed) {
    difficulty = std::clamp(diff, 0, 2);
    if(seed != 0) rng.seed(seed);

    player = PlayerState{};
    player.pos = {arena.Width * 0.5f, arena.Height * 0.5f};
    player.aim = {arena.Width * 0.5f, arena.Height * 0.25f};
    player.maxHp = (difficulty == 0) ? 140 : (difficulty == 1 ? 100 : 80);
    player.hp = player.maxHp;

    bullets.clear();
    enemies.clear();
    powerups.clear();
    particles.clear();

    killsByType.Destroy();
    killsByType.Add("Chaser",  new int(0));
    killsByType.Add("Spinner", new int(0));
    killsByType.Add("Tank",    new int(0));

    score = 0;
    wave = 1;
    combo = 1;
    comboTimer = 0.f;
    totalKills = 0;
    waveKills = 0;
    waveTarget = 6 + difficulty * 2;
    spawnTimer = 0.5f;
    screenShake = 0.f;
    novaWaveRadius = -1.f;
    gameOverFired = false;

    cameraPos = player.pos;
    cameraVel = {0.f, 0.f};
    cameraDetached = false;
    dying = false;
    deathTimer = 0.f;
    deathWaveRadius = -1.f;
    deathPos = player.pos;

    stars.clear();
    std::uniform_real_distribution<float> rx(0.f, float(arena.Width));
    std::uniform_real_distribution<float> ry(0.f, float(arena.Height));
    std::uniform_real_distribution<float> rs(18.f, 75.f);
    for(int i = 0; i < 70; ++i) {
        float sp = rs(rng);
        stars.push_back({{rx(rng), ry(rng)}, sp, sp / 75.f});
    }

    OnWaveStarted(wave);
}

int GameEngine::GetKillCount(const std::string &kind) const {
    if(!killsByType.Exists(kind)) return 0;
    return killsByType[kind];
}

void GameEngine::recordKill(EnemyKind kind) {
    std::string key = EnemyKindName(kind);
    if(!killsByType.Exists(key)) {
        killsByType.Add(key, new int(0));
    }
    killsByType[key] += 1;
}

void GameEngine::emitParticles(Pointf pos, RGBA color, int count, float speed, float minLife, float maxLife, float size) {
    std::uniform_real_distribution<float> angDist(0.f, 2.f * Gorgon::PI);
    std::uniform_real_distribution<float> spdDist(speed * 0.35f, speed);
    std::uniform_real_distribution<float> lifeDist(minLife, maxLife);
    for(int i = 0; i < count; ++i) {
        float a = angDist(rng);
        float s = spdDist(rng);
        float l = lifeDist(rng);
        particles.push_back({pos, {std::cos(a) * s, std::sin(a) * s}, color, l, l, size});
    }
}

void GameEngine::beginPlayerDeath() {
    if(dying) return;
    dying = true;
    deathTimer = 0.f;
    deathPos = player.pos;
    player.firing = false;
    player.vel = {0.f, 0.f};

    // Let the camera detach from the character and drift freely
    cameraDetached = true;
    float camSpd = std::sqrt(cameraVel.X * cameraVel.X + cameraVel.Y * cameraVel.Y);
    if(camSpd < 45.f) {
        std::uniform_real_distribution<float> angDist(0.f, 2.f * Gorgon::PI);
        float a = angDist(rng);
        cameraVel = {std::cos(a) * 55.f, std::sin(a) * 55.f};
    } else if(camSpd > 160.f) {
        cameraVel = cameraVel * (160.f / camSpd);
    }

    // Big multi-layered character explosion
    screenShake = 18.f;
    deathWaveRadius = 12.f;
    emitParticles(deathPos, {220, 255, 255, 255}, 36, 460.f, 0.45f, 0.95f, 1.25f);
    emitParticles(deathPos, {60,  235, 255, 255}, 28, 310.f, 0.85f, 1.70f, 1.85f);
    emitParticles(deathPos, {255, 145, 45,  255}, 28, 280.f, 0.90f, 1.85f, 1.70f);
    emitParticles(deathPos, {255, 65,  130, 255}, 22, 170.f, 1.20f, 2.20f, 1.20f);

    // Freeze all enemies immediately for the 1-second pause
    for(auto &e : enemies) {
        e.vel = {0.f, 0.f};
    }

    OnPlayerDied();
}

void GameEngine::updateCamera(float dt) {
    if(dt <= 0.f) return;
    if(!cameraDetached) {
        Pointf lead = (player.aim - player.pos) * 0.08f;
        float leadLen = std::sqrt(lead.X * lead.X + lead.Y * lead.Y);
        if(leadLen > 45.f) lead = lead * (45.f / leadLen);
        Pointf target = player.pos + lead;

        float halfW = viewport.Width * 0.5f;
        float halfH = viewport.Height * 0.5f;
        if(arena.Width > viewport.Width) {
            target.X = std::clamp(target.X, halfW, float(arena.Width) - halfW);
        } else {
            target.X = arena.Width * 0.5f;
        }
        if(arena.Height > viewport.Height) {
            target.Y = std::clamp(target.Y, halfH, float(arena.Height) - halfH);
        } else {
            target.Y = arena.Height * 0.5f;
        }

        float follow = 1.f - std::exp(-7.5f * dt);
        Pointf nextPos = cameraPos + (target - cameraPos) * follow;
        cameraVel = (nextPos - cameraPos) * (1.f / dt);
        cameraPos = nextPos;
    } else {
        // Detached: coast smoothly and let go of the character
        cameraPos = cameraPos + cameraVel * dt;
        cameraVel = cameraVel * std::exp(-1.15f * dt);

        float margin = 90.f;
        float halfW = viewport.Width * 0.5f;
        float halfH = viewport.Height * 0.5f;
        float minX = (arena.Width > viewport.Width) ? (halfW - margin) : (arena.Width * 0.5f - margin);
        float maxX = (arena.Width > viewport.Width) ? (float(arena.Width) - halfW + margin) : (arena.Width * 0.5f + margin);
        float minY = (arena.Height > viewport.Height) ? (halfH - margin) : (arena.Height * 0.5f - margin);
        float maxY = (arena.Height > viewport.Height) ? (float(arena.Height) - halfH + margin) : (arena.Height * 0.5f + margin);
        cameraPos.X = std::clamp(cameraPos.X, minX, maxX);
        cameraPos.Y = std::clamp(cameraPos.Y, minY, maxY);
    }
}

bool GameEngine::TriggerDash() {
    if(dying || player.energy < 35.f || player.dashTimer > 0.f) return false;
    player.energy -= 35.f;
    player.dashTimer = 0.22f;
    player.invulnTimer = std::max(player.invulnTimer, 0.28f);
    Pointf dir{float(player.moveRight) - float(player.moveLeft),
               float(player.moveDown) - float(player.moveUp)};
    if(dir.X == 0.f && dir.Y == 0.f) {
        dir = Normalize(player.aim - player.pos);
    } else {
        dir = Normalize(dir);
    }
    player.vel = dir * 680.f;
    emitParticles(player.pos, {80, 240, 255, 255}, 14, 180.f);
    OnDash();
    return true;
}

bool GameEngine::TriggerNova() {
    if(dying || player.energy < 65.f) return false;
    player.energy -= 65.f;
    novaWaveRadius = 10.f;
    screenShake = 8.f;

    // Clear enemy bullets within 220px
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [&](const Bullet &b) {
        return !b.fromPlayer && DistSq(b.pos, player.pos) <= 220.f * 220.f;
    }), bullets.end());

    // Damage all enemies within 220px
    for(auto &e : enemies) {
        if(DistSq(e.pos, player.pos) <= 220.f * 220.f) {
            e.hp -= 55;
            e.hitFlash = 0.2f;
            Pointf push = Normalize(e.pos - player.pos);
            e.pos = e.pos + push * 35.f;
        }
    }
    emitParticles(player.pos, {150, 255, 255, 255}, 28, 300.f);
    OnNova();
    return true;
}

void GameEngine::SpawnEnemyDirect(EnemyKind kind, Pointf pos) {
    Enemy e;
    e.kind = kind;
    e.pos = pos;
    float diffScale = 1.0f + difficulty * 0.25f + (wave - 1) * 0.12f;
    if(kind == EnemyKind::Chaser) {
        e.hp = e.maxHp = int(28 * diffScale);
        e.radius = 14.f;
        e.speed = 115.f + wave * 6.f;
    } else if(kind == EnemyKind::Spinner) {
        e.hp = e.maxHp = int(42 * diffScale);
        e.radius = 16.f;
        e.speed = 82.f + wave * 4.f;
        e.shootCooldown = 1.6f;
    } else {
        e.hp = e.maxHp = int(95 * diffScale);
        e.radius = 22.f;
        e.speed = 55.f + wave * 3.f;
    }
    enemies.push_back(e);
}

void GameEngine::SpawnPowerupDirect(PowerupKind kind, Pointf pos) {
    powerups.push_back({kind, pos, 11.f});
}

void GameEngine::spawnWaveEnemy() {
    std::uniform_int_distribution<int> edgeDist(0, 3);
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    Pointf pos;
    int edge = edgeDist(rng);
    if(edge == 0)      pos = {u01(rng) * arena.Width, -20.f};
    else if(edge == 1) pos = {u01(rng) * arena.Width, arena.Height + 20.f};
    else if(edge == 2) pos = {-20.f, u01(rng) * arena.Height};
    else               pos = {arena.Width + 20.f, u01(rng) * arena.Height};

    int roll = int(u01(rng) * 100);
    EnemyKind kind = EnemyKind::Chaser;
    if(wave >= 2 && roll > 60 && roll <= 88) kind = EnemyKind::Spinner;
    else if(wave >= 3 && roll > 88)          kind = EnemyKind::Tank;
    else if(roll > 75)                       kind = EnemyKind::Spinner;

    SpawnEnemyDirect(kind, pos);
}

bool GameEngine::Update(unsigned deltaMs) {
    float dt = std::min(deltaMs, 50u) / 1000.0f;
    if(dt <= 0.f) return player.hp > 0;

    // Scroll starfield
    for(auto &s : stars) {
        s.pos.Y += s.speed * dt;
        if(s.pos.Y > arena.Height) {
            s.pos.Y -= arena.Height;
        }
    }

    if(player.hp <= 0 && !dying) {
        beginPlayerDeath();
    }

    if(dying) {
        deathTimer += dt;
        screenShake = std::max(0.f, screenShake - 16.f * dt);
        if(deathWaveRadius >= 0.f) {
            deathWaveRadius += 500.f * dt;
            if(deathWaveRadius > 420.f) deathWaveRadius = -1.f;
        }
        if(novaWaveRadius >= 0.f) {
            novaWaveRadius += 780.f * dt;
            if(novaWaveRadius > 240.f) novaWaveRadius = -1.f;
        }

        updateCamera(dt);

        // Enemies stop for 1 second, then separate away from the blast and each other
        if(deathTimer < DEATH_FREEZE_DURATION) {
            for(auto &e : enemies) {
                e.hitFlash = std::max(0.f, e.hitFlash - dt);
                e.vel = {0.f, 0.f};
            }
        } else {
            float ramp = std::min(1.f, (deathTimer - DEATH_FREEZE_DURATION) / 0.25f);
            std::vector<Pointf> nextVels(enemies.size(), {0.f, 0.f});
            for(size_t i = 0; i < enemies.size(); ++i) {
                auto &e = enemies[i];
                e.hitFlash = std::max(0.f, e.hitFlash - dt);

                Pointf awayDeath;
                if(DistSq(e.pos, deathPos) > 1.f) {
                    awayDeath = Normalize(e.pos - deathPos);
                } else {
                    float a = float(i) * 2.39996f;
                    awayDeath = {std::cos(a), std::sin(a)};
                }

                Pointf repulse{0.f, 0.f};
                for(size_t j = 0; j < enemies.size(); ++j) {
                    if(i == j) continue;
                    float dsq = DistSq(e.pos, enemies[j].pos);
                    if(dsq < 220.f * 220.f) {
                        float d = std::sqrt(dsq);
                        Pointf push;
                        if(d > 0.5f) {
                            push = {(e.pos.X - enemies[j].pos.X) / d, (e.pos.Y - enemies[j].pos.Y) / d};
                        } else {
                            float a = float(i) * 2.39996f;
                            push = {std::cos(a), std::sin(a)};
                        }
                        repulse = repulse + push * (1.f - d / 220.f);
                    }
                }

                Pointf dir = Normalize(awayDeath + repulse * 1.45f);
                nextVels[i] = dir * (e.speed * 1.25f * ramp);
            }
            for(size_t i = 0; i < enemies.size(); ++i) {
                enemies[i].vel = nextVels[i];
                enemies[i].pos = enemies[i].pos + enemies[i].vel * dt;
            }
        }

        // Let existing bullets fly out and expire
        for(auto &b : bullets) {
            b.pos = b.pos + b.vel * dt;
            b.life -= dt;
        }
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [&](const Bullet &b) {
            return b.life <= 0.f || b.pos.X < -120.f || b.pos.X > arena.Width + 120.f
                                 || b.pos.Y < -120.f || b.pos.Y > arena.Height + 120.f;
        }), bullets.end());

        // Update explosion particles with gentle drag
        for(auto &pt : particles) {
            pt.pos = pt.pos + pt.vel * dt;
            pt.vel = pt.vel * std::max(0.f, 1.f - 2.2f * dt);
            pt.life -= dt;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Particle &p) { return p.life <= 0.f; }), particles.end());

        if(deathTimer >= DEATH_TOTAL_DURATION) {
            if(!gameOverFired) {
                gameOverFired = true;
                OnGameOver(score);
            }
            return false;
        }
        return true;
    }

    // Timers & energy regen
    player.energy      = std::min(player.maxEnergy, player.energy + 18.f * dt);
    player.fireTimer   = std::max(0.f, player.fireTimer - dt);
    player.dashTimer   = std::max(0.f, player.dashTimer - dt);
    player.invulnTimer = std::max(0.f, player.invulnTimer - dt);
    player.rapidTimer  = std::max(0.f, player.rapidTimer - dt);
    player.shieldTimer = std::max(0.f, player.shieldTimer - dt);
    screenShake        = std::max(0.f, screenShake - 24.f * dt);
    if(comboTimer > 0.f) {
        comboTimer -= dt;
        if(comboTimer <= 0.f) combo = 1;
    }
    if(novaWaveRadius >= 0.f) {
        novaWaveRadius += 780.f * dt;
        if(novaWaveRadius > 240.f) novaWaveRadius = -1.f;
    }

    // Player movement
    if(player.dashTimer <= 0.f) {
        Pointf input{float(player.moveRight) - float(player.moveLeft),
                     float(player.moveDown) - float(player.moveUp)};
        if(input.X != 0.f || input.Y != 0.f) {
            input = Normalize(input);
            player.vel = input * 285.f;
        } else {
            player.vel = player.vel * std::max(0.f, 1.f - 12.f * dt);
        }
    }
    player.pos = player.pos + player.vel * dt;
    player.pos.X = std::clamp(player.pos.X, 22.f, float(arena.Width - 22));
    player.pos.Y = std::clamp(player.pos.Y, 22.f, float(arena.Height - 22));

    // Shooting
    float fireInterval = (player.rapidTimer > 0.f) ? 0.085f : 0.165f;
    if(player.firing && player.fireTimer <= 0.f) {
        player.fireTimer = fireInterval;
        Pointf dir = Normalize(player.aim - player.pos);
        bullets.push_back({player.pos + dir * 20.f, dir * 640.f, 6.0f, true, 22});
        if(player.rapidTimer > 0.f) {
            // Twin side bolts during RapidFire
            Pointf perp{-dir.Y, dir.X};
            bullets.push_back({player.pos + dir * 16.f + perp * 9.f, dir * 640.f, 6.0f, true, 14});
            bullets.push_back({player.pos + dir * 16.f - perp * 9.f, dir * 640.f, 6.0f, true, 14});
        }
        OnShoot();
    }

    // Spawn enemies
    spawnTimer -= dt;
    size_t maxConcurrent = static_cast<size_t>(5 + wave * 2 + difficulty * 2);
    if(spawnTimer <= 0.f && enemies.size() < maxConcurrent) {
        spawnWaveEnemy();
        spawnTimer = std::max(0.32f, 1.35f - wave * 0.09f - difficulty * 0.14f);
    }

    // Update bullets
    for(auto &b : bullets) {
        b.pos = b.pos + b.vel * dt;
        b.life -= dt;
    }

    // Update enemies
    for(auto &e : enemies) {
        e.hitFlash = std::max(0.f, e.hitFlash - dt);
        Pointf toPlayer = Normalize(player.pos - e.pos);
        if(e.kind == EnemyKind::Spinner) {
            // Orbit slightly while approaching
            Pointf perp{-toPlayer.Y, toPlayer.X};
            e.vel = (toPlayer * 0.72f + perp * 0.55f) * e.speed;
            e.shootCooldown -= dt;
            if(e.shootCooldown <= 0.f) {
                e.shootCooldown = std::max(0.95f, 2.2f - wave * 0.08f);
                bullets.push_back({e.pos, toPlayer * (230.f + wave * 12.f), 10.0f, false, 12 + difficulty * 4});
            }
        } else {
            e.vel = toPlayer * e.speed;
        }
        e.pos = e.pos + e.vel * dt;
    }

    // Bullet vs Enemy & Bullet vs Player
    for(auto &b : bullets) {
        if(b.life <= 0.f) continue;
        if(b.fromPlayer) {
            for(auto &e : enemies) {
                if(e.hp <= 0) continue;
                float hitR = e.radius + 6.f;
                if(DistSq(b.pos, e.pos) <= hitR * hitR) {
                    e.hp -= b.damage;
                    e.hitFlash = 0.12f;
                    b.life = 0.f;
                    emitParticles(b.pos, {120, 255, 250, 255}, 5, 130.f);
                    OnEnemyHit();
                    break;
                }
            }
        } else {
            float hitR = 17.f;
            if(DistSq(b.pos, player.pos) <= hitR * hitR) {
                b.life = 0.f;
                if(player.invulnTimer <= 0.f && player.shieldTimer <= 0.f) {
                    player.hp = std::max(0, player.hp - b.damage);
                    player.invulnTimer = 0.55f;
                    combo = 1;
                    screenShake = 7.f;
                    emitParticles(player.pos, {255, 90, 90, 255}, 12, 200.f);
                    OnPlayerDamaged(b.damage);
                }
            }
        }
    }

    // Enemy vs Player contact & enemy deaths
    std::uniform_int_distribution<int> dropRoll(0, 99);
    for(auto &e : enemies) {
        if(e.hp <= 0) continue;
        float contactR = e.radius + 15.f;
        if(DistSq(e.pos, player.pos) <= contactR * contactR) {
            if(player.invulnTimer <= 0.f && player.shieldTimer <= 0.f) {
                int dmg = (e.kind == EnemyKind::Tank) ? 24 : 15;
                player.hp = std::max(0, player.hp - dmg);
                player.invulnTimer = 0.65f;
                combo = 1;
                screenShake = 9.f;
                emitParticles(player.pos, {255, 70, 90, 255}, 16, 220.f);
                OnPlayerDamaged(dmg);
            }
            // Push enemy back
            e.pos = e.pos + Normalize(e.pos - player.pos) * 28.f;
        }
    }

    // Cull dead enemies, award score, drop powerups
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [&](const Enemy &e) {
        if(e.hp > 0) return false;
        int pts = (e.kind == EnemyKind::Tank) ? 250 : (e.kind == EnemyKind::Spinner ? 140 : 100);
        score += pts * combo;
        combo = std::min(12, combo + 1);
        comboTimer = 3.2f;
        totalKills++;
        waveKills++;
        recordKill(e.kind);
        screenShake = std::max(screenShake, e.kind == EnemyKind::Tank ? 8.f : 4.f);
        RGBA burst = (e.kind == EnemyKind::Tank) ? RGBA{190, 90, 255, 255}
                   : (e.kind == EnemyKind::Spinner ? RGBA{255, 180, 50, 255}
                                                   : RGBA{255, 70, 110, 255});
        emitParticles(e.pos, burst, e.kind == EnemyKind::Tank ? 22 : 12, 230.f);
        OnEnemyKilled(e.kind);

        int r = dropRoll(rng);
        if(r < 18) {
            PowerupKind pk = (r < 6) ? PowerupKind::Heal
                           : (r < 12 ? PowerupKind::RapidFire : PowerupKind::Shield);
            powerups.push_back({pk, e.pos, 10.f});
        }
        return true;
    }), enemies.end());

    // Wave progression
    if(waveKills >= waveTarget) {
        wave++;
        waveKills = 0;
        waveTarget = 6 + wave * 3 + difficulty * 2;
        score += 300 * wave;
        player.energy = player.maxEnergy;
        OnWaveStarted(wave);
    }

    // Powerups
    for(auto &p : powerups) {
        p.life -= dt;
        if(DistSq(p.pos, player.pos) <= 26.f * 26.f) {
            p.life = 0.f;
            if(p.kind == PowerupKind::Heal) {
                player.hp = std::min(player.maxHp, player.hp + 30);
            } else if(p.kind == PowerupKind::RapidFire) {
                player.rapidTimer = 6.5f;
            } else {
                player.shieldTimer = 5.5f;
            }
            emitParticles(p.pos, {120, 255, 180, 255}, 12, 160.f);
            OnPickup(p.kind);
        }
    }
    powerups.erase(std::remove_if(powerups.begin(), powerups.end(),
        [](const Powerup &p) { return p.life <= 0.f; }), powerups.end());

    // Cull expired/offscreen bullets
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [&](const Bullet &b) {
        return b.life <= 0.f || b.pos.X < -120.f || b.pos.X > arena.Width + 120.f
                             || b.pos.Y < -120.f || b.pos.Y > arena.Height + 120.f;
    }), bullets.end());

    // Update particles
    for(auto &pt : particles) {
        pt.pos = pt.pos + pt.vel * dt;
        pt.vel = pt.vel * std::max(0.f, 1.f - 3.5f * dt);
        pt.life -= dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
        [](const Particle &p) { return p.life <= 0.f; }), particles.end());

    updateCamera(dt);

    if(player.hp <= 0 && !dying) {
        beginPlayerDeath();
    }
    return true;
}

} // namespace NeonVortex
