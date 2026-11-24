#include "Assets.h"
#include "Types.h"

#include <Gorgon/CGI/Circle.h>
#include <Gorgon/CGI/Polygon.h>
#include <Gorgon/CGI/Line.h>
#include <Gorgon/ImageProcessing/Filters.h>
#include <Gorgon/ImageProcessing/Kernel.h>
#include <Gorgon/Geometry/PointList.h>

#include <cmath>
#include <cstdlib>

namespace NeonVortex {

using namespace Gorgon;
using Gorgon::Graphics::Bitmap;
using Gorgon::Graphics::RGBA;
using Gorgon::Geometry::Pointf;
using Gorgon::Geometry::PointList;

namespace {

Bitmap MakeBlankBitmap(int w, int h) {
    Bitmap bmp({w, h}, Graphics::ColorMode::RGBA);
    bmp.Clear();
    return bmp;
}

PointList<Pointf> MakeRegularPolygon(Pointf center, float radius, int sides, float angleOffset) {
    PointList<Pointf> pts;
    for(int i = 0; i < sides; ++i) {
        float a = angleOffset + (2.0f * PI * i) / sides;
        pts.Push({center.X + std::cos(a) * radius, center.Y + std::sin(a) * radius});
    }
    return pts;
}

PointList<Pointf> MakeStarPolygon(Pointf center, float outerR, float innerR, int points, float angleOffset) {
    PointList<Pointf> pts;
    for(int i = 0; i < points * 2; ++i) {
        float r = (i % 2 == 0) ? outerR : innerR;
        float a = angleOffset + (PI * i) / points;
        pts.Push({center.X + std::cos(a) * r, center.Y + std::sin(a) * r});
    }
    return pts;
}

Containers::Wave SynthesizeSweep(float durationSec, float f0, float f1, float amp, bool noiseMix = false) {
    const unsigned rate = 44100;
    const unsigned samples = static_cast<unsigned>(durationSec * rate);
    Containers::Wave w(samples, rate, {Audio::Channel::Mono});
    float phase = 0.0f;
    unsigned rng = 0x1234567u;
    for(unsigned i = 0; i < samples; ++i) {
        float t = float(i) / float(samples);
        float freq = f0 + (f1 - f0) * t;
        phase += 2.0f * PI * freq / float(rate);
        float env = (1.0f - t) * (1.0f - t);
        float s = std::sin(phase);
        if(noiseMix) {
            rng = rng * 1664525u + 1013904223u;
            float n = (float(int(rng >> 16) - 32768) / 32768.0f);
            s = 0.45f * s + 0.55f * n;
        }
        w(i, 0) = s * env * amp;
    }
    return w;
}

Containers::Wave SynthesizeChime(const std::vector<float> &freqs, float noteDur, float amp) {
    const unsigned rate = 44100;
    const unsigned perNote = static_cast<unsigned>(noteDur * rate);
    const unsigned total = perNote * static_cast<unsigned>(freqs.size());
    Containers::Wave w(total, rate, {Audio::Channel::Mono});
    for(size_t n = 0; n < freqs.size(); ++n) {
        float f = freqs[n];
        for(unsigned i = 0; i < perNote; ++i) {
            float t = float(i) / float(perNote);
            float env = std::sin(PI * std::min(1.0f, t * 8.0f)) * (1.0f - t * 0.7f);
            float s = 0.7f * std::sin(2.0f * PI * f * i / rate) +
                      0.3f * std::sin(4.0f * PI * f * i / rate);
            w(static_cast<unsigned long>(n * perNote + i), 0) = s * env * amp;
        }
    }
    return w;
}

} // anonymous namespace

GameAssets &GameAssets::Get() {
    static GameAssets instance;
    return instance;
}

void GameAssets::GenerateSprites() {
    if(spritesGenerated) return;

    // 1. Player ship: crisp cyan delta-wing with glowing core (CGI::Polyfill + CGI::Circle)
    playerShip = MakeBlankBitmap(40, 40);
    {
        PointList<Pointf> hull = {{20, 3}, {35, 34}, {20, 27}, {5, 34}};
        CGI::Polyfill(playerShip, hull, CGI::SolidFill<RGBA>({0, 220, 255, 255}));
        PointList<Pointf> cockpit = {{20, 10}, {26, 25}, {20, 22}, {14, 25}};
        CGI::Polyfill(playerShip, cockpit, CGI::SolidFill<RGBA>({190, 250, 255, 255}));
        CGI::Circle(playerShip, Pointf{20, 20}, 4.5f, CGI::SolidFill<RGBA>({255, 255, 255, 255}));
    }

    // 2. Player shield ring
    playerShield = MakeBlankBitmap(56, 56);
    CGI::Circle(playerShield, Pointf{28, 28}, 22.0f, 3.0f, CGI::SolidFill<RGBA>({80, 235, 255, 185}));

    // 3. Chaser enemy: sharp crimson triangle
    enemyChaser = MakeBlankBitmap(32, 32);
    {
        auto tri = MakeRegularPolygon({16, 16}, 13.0f, 3, -PI / 2.0f);
        CGI::Polyfill(enemyChaser, tri, CGI::SolidFill<RGBA>({255, 55, 95, 255}));
        CGI::Circle(enemyChaser, Pointf{16, 16}, 4.0f, CGI::SolidFill<RGBA>({255, 210, 220, 255}));
    }

    // 4. Tank enemy: armored violet hexagon with inner ring
    enemyTank = MakeBlankBitmap(48, 48);
    {
        auto hex = MakeRegularPolygon({24, 24}, 21.0f, 6, 0.0f);
        CGI::Polyfill(enemyTank, hex, CGI::SolidFill<RGBA>({170, 60, 255, 255}));
        auto inner = MakeRegularPolygon({24, 24}, 13.0f, 6, PI / 6.0f);
        CGI::Polyfill(enemyTank, inner, CGI::SolidFill<RGBA>({95, 25, 165, 255}));
        CGI::Circle(enemyTank, Pointf{24, 24}, 6.0f, CGI::SolidFill<RGBA>({255, 130, 255, 255}));
    }

    // 5. Animated Spinner enemy (4-frame BitmapAnimationProvider)
    spinnerAnim.Clear();
    for(int f = 0; f < 4; ++f) {
        Bitmap frame({36, 36}, Graphics::ColorMode::RGBA);
        frame.Clear();
        float rot = f * (PI / 8.0f);
        auto star = MakeStarPolygon({18, 18}, 15.0f, 7.5f, 4, rot);
        CGI::Polyfill(frame, star, CGI::SolidFill<RGBA>({255, 165, 30, 255}));
        CGI::Circle(frame, Pointf{18, 18}, 4.5f, CGI::SolidFill<RGBA>({255, 245, 180, 255}));
        spinnerAnim.Add(std::move(frame), 110); // 110ms per frame
    }

    // 6. Projectiles
    bulletPlayer = MakeBlankBitmap(12, 12);
    CGI::Circle(bulletPlayer, Pointf{6, 6}, 4.8f, CGI::SolidFill<RGBA>({120, 255, 245, 255}));
    CGI::Circle(bulletPlayer, Pointf{6, 6}, 2.2f, CGI::SolidFill<RGBA>({255, 255, 255, 255}));

    bulletEnemy = MakeBlankBitmap(14, 14);
    CGI::Circle(bulletEnemy, Pointf{7, 7}, 5.5f, CGI::SolidFill<RGBA>({255, 90, 40, 255}));
    CGI::Circle(bulletEnemy, Pointf{7, 7}, 2.5f, CGI::SolidFill<RGBA>({255, 230, 140, 255}));

    // 7. Powerups
    powerupHeal = MakeBlankBitmap(24, 24);
    CGI::Circle(powerupHeal, Pointf{12, 12}, 10.0f, CGI::SolidFill<RGBA>({40, 220, 110, 255}));
    CGI::Rectangle(powerupHeal, Geometry::Rectanglef{9.5f, 5.0f, 5.0f, 14.0f}, CGI::SolidFill<RGBA>({255, 255, 255, 255}));
    CGI::Rectangle(powerupHeal, Geometry::Rectanglef{5.0f, 9.5f, 14.0f, 5.0f}, CGI::SolidFill<RGBA>({255, 255, 255, 255}));

    powerupRapid = MakeBlankBitmap(24, 24);
    CGI::Polyfill(powerupRapid, MakeStarPolygon({12, 12}, 10.5f, 5.0f, 5, -PI / 2.0f),
                  CGI::SolidFill<RGBA>({255, 220, 45, 255}));

    powerupShield = MakeBlankBitmap(24, 24);
    CGI::Circle(powerupShield, Pointf{12, 12}, 9.5f, CGI::SolidFill<RGBA>({55, 195, 255, 255}));
    CGI::Circle(powerupShield, Pointf{12, 12}, 5.5f, 2.0f, CGI::SolidFill<RGBA>({230, 250, 255, 255}));

    // 8. Particle dot
    particleDot = MakeBlankBitmap(8, 8);
    CGI::Circle(particleDot, Pointf{4, 4}, 3.2f, CGI::SolidFill<RGBA>({255, 255, 255, 255}));

    // 9. Glow halo generated with ImageProcessing Gaussian Convolution!
    {
        Bitmap rawHalo = MakeBlankBitmap(64, 64);
        CGI::Circle(rawHalo, Pointf{32, 32}, 14.0f, CGI::SolidFill<RGBA>({255, 255, 255, 210}));
        auto kx = ImageProcessing::Kernel::GaussianFilter(4.5f, Axis::X, 2.0f);
        auto ky = ImageProcessing::Kernel::GaussianFilter(4.5f, Axis::Y, 2.0f);
        auto pass1 = ImageProcessing::Convolution(rawHalo.GetData(), kx,
            ImageProcessing::OutOfBoundsPolicy::FixedColor,
            ImageProcessing::OutOfRangePolicy::Clamp, {0, 0, 0, 0}, false, true);
        auto pass2 = ImageProcessing::Convolution(pass1, ky,
            ImageProcessing::OutOfBoundsPolicy::FixedColor,
            ImageProcessing::OutOfRangePolicy::Clamp, {0, 0, 0, 0}, false, true);
        glowHalo = Bitmap(std::move(pass2));
    }

    // 10. Crosshair reticle
    crosshair = MakeBlankBitmap(26, 26);
    CGI::Circle(crosshair, Pointf{13, 13}, 7.5f, 1.8f, CGI::SolidFill<RGBA>({120, 255, 240, 230}));
    CGI::Rectangle(crosshair, Geometry::Rectanglef{12.0f, 1.0f, 2.0f, 5.0f}, CGI::SolidFill<RGBA>({120, 255, 240, 230}));
    CGI::Rectangle(crosshair, Geometry::Rectanglef{12.0f, 20.0f, 2.0f, 5.0f}, CGI::SolidFill<RGBA>({120, 255, 240, 230}));
    CGI::Rectangle(crosshair, Geometry::Rectanglef{1.0f, 12.0f, 5.0f, 2.0f}, CGI::SolidFill<RGBA>({120, 255, 240, 230}));
    CGI::Rectangle(crosshair, Geometry::Rectanglef{20.0f, 12.0f, 5.0f, 2.0f}, CGI::SolidFill<RGBA>({120, 255, 240, 230}));

    // 11. Window icon
    windowIcon = playerShip.Duplicate();

    spritesGenerated = true;
}

void GameAssets::PrepareGPU() {
    if(!spritesGenerated) GenerateSprites();
    if(gpuPrepared) return;

    playerShip.Prepare();
    playerShield.Prepare();
    enemyChaser.Prepare();
    enemyTank.Prepare();
    bulletPlayer.Prepare();
    bulletEnemy.Prepare();
    powerupHeal.Prepare();
    powerupRapid.Prepare();
    powerupShield.Prepare();
    particleDot.Prepare();
    glowHalo.Prepare();
    crosshair.Prepare();

    spinnerAnim.Prepare();
    spinnerInstance = spinnerAnim.CreateAnimation();

    gpuPrepared = true;
}

void GameAssets::GenerateAudio() {
    if(audioGenerated) return;

    rawWaves[static_cast<size_t>(SfxId::Shoot)]    = SynthesizeSweep(0.09f, 920.0f, 260.0f, 0.30f, false);
    rawWaves[static_cast<size_t>(SfxId::Hit)]      = SynthesizeSweep(0.08f, 340.0f, 120.0f, 0.35f, true);
    rawWaves[static_cast<size_t>(SfxId::Explode)]  = SynthesizeSweep(0.28f, 180.0f, 38.0f,  0.48f, true);
    rawWaves[static_cast<size_t>(SfxId::Pickup)]   = SynthesizeChime({523.25f, 659.25f, 783.99f}, 0.065f, 0.35f);
    rawWaves[static_cast<size_t>(SfxId::Dash)]     = SynthesizeSweep(0.16f, 220.0f, 680.0f, 0.32f, true);
    rawWaves[static_cast<size_t>(SfxId::Nova)]     = SynthesizeSweep(0.38f, 480.0f, 55.0f,  0.50f, true);
    rawWaves[static_cast<size_t>(SfxId::GameOver)] = SynthesizeChime({392.0f, 349.23f, 311.13f, 261.63f}, 0.14f, 0.35f);

    audioGenerated = true;
}

void GameAssets::InitAudioPlayback(float volume01, bool enabled) {
    if(!audioGenerated) GenerateAudio();
    masterVolume = std::clamp(volume01, 0.0f, 1.0f);
    audioEnabled = enabled;

#ifdef GORGON_AUDIO_SUPPORT
    if(!Audio::IsAvailable()) {
        try {
            Audio::Initialize();
        }
        catch(const std::exception &e) {
            GameLog() << "Audio init skipped: " << e.what();
            return;
        }
    }
    if(!Audio::IsAvailable()) return;

    Audio::SetVolume(audioEnabled ? masterVolume : 0.0f);

    for(size_t i = 0; i < static_cast<size_t>(SfxId::Count); ++i) {
        if(!soundWaves[i].HasData()) {
            soundWaves[i].Assign(rawWaves[i]);
        }
    }
    for(auto &v : voices) {
        if(!v) {
            v = std::make_unique<Audio::BasicController>();
        }
    }
#endif
}

void GameAssets::SetAudioVolume(float volume01, bool enabled) {
    masterVolume = std::clamp(volume01, 0.0f, 1.0f);
    audioEnabled = enabled;
#ifdef GORGON_AUDIO_SUPPORT
    if(Audio::IsAvailable()) {
        Audio::SetVolume(audioEnabled ? masterVolume : 0.0f);
    }
#endif
}

void GameAssets::PlaySfx(SfxId id, float gain) {
#ifdef GORGON_AUDIO_SUPPORT
    if(!audioEnabled || !Audio::IsAvailable()) return;
    size_t idx = static_cast<size_t>(id);
    if(idx >= soundWaves.size() || !soundWaves[idx].HasData()) return;

    auto &voice = voices[nextVoice];
    nextVoice = (nextVoice + 1) % VOICE_POOL_SIZE;
    if(!voice) return;

    voice->Pause();
    voice->SetData(soundWaves[idx]);
    voice->SetVolume(std::clamp(gain, 0.0f, 1.0f));
    voice->Reset();
    voice->Play();
#else
    (void)id; (void)gain;
#endif
}

} // namespace NeonVortex
