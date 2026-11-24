#pragma once

#include <Gorgon/Graphics/Bitmap.h>
#include <Gorgon/Graphics/BlankImage.h>
#include <Gorgon/Graphics/TextureAnimation.h>
#include <Gorgon/Containers/Wave.h>

#ifdef GORGON_AUDIO_SUPPORT
#include <Gorgon/Audio.h>
#include <Gorgon/Audio/Controllers.h>
#include <Gorgon/Multimedia/Wave.h>
#endif

#include <array>
#include <memory>
#include <string>

namespace NeonVortex {

enum class SfxId {
    Shoot = 0,
    Hit,
    Explode,
    Pickup,
    Dash,
    Nova,
    GameOver,
    Count
};

class GameAssets {
public:
    static GameAssets &Get();

    // Generate all procedural CGI + ImageProcessing sprites in CPU RAM (safe headless)
    void GenerateSprites();

    // Upload generated sprites & animations to GPU (requires active graphics context)
    void PrepareGPU();

    // Synthesize all procedural audio waves in RAM (safe headless)
    void GenerateAudio();

    // Bind synthesized waves to Audio::BasicController pool & set volume
    void InitAudioPlayback(float volume01, bool enabled);
    void SetAudioVolume(float volume01, bool enabled);
    void PlaySfx(SfxId id, float gain = 1.0f);

    bool IsGenerated() const { return spritesGenerated; }
    bool IsGPUPrepared() const { return gpuPrepared; }
    bool IsAudioGenerated() const { return audioGenerated; }

    // Sprites
    Gorgon::Graphics::Bitmap playerShip;
    Gorgon::Graphics::Bitmap playerShield;
    Gorgon::Graphics::Bitmap enemyChaser;
    Gorgon::Graphics::Bitmap enemyTank;
    Gorgon::Graphics::Bitmap bulletPlayer;
    Gorgon::Graphics::Bitmap bulletEnemy;
    Gorgon::Graphics::Bitmap powerupHeal;
    Gorgon::Graphics::Bitmap powerupRapid;
    Gorgon::Graphics::Bitmap powerupShield;
    Gorgon::Graphics::Bitmap particleDot;
    Gorgon::Graphics::Bitmap glowHalo;      // Created via ImageProcessing::Convolution (Gaussian)
    Gorgon::Graphics::Bitmap crosshair;
    Gorgon::Graphics::Bitmap windowIcon;

    // Animated 4-frame spinning diamond enemy
    Gorgon::Graphics::BitmapAnimationProvider spinnerAnim;
    Gorgon::Graphics::Instance spinnerInstance;

    // Solid color helpers
    Gorgon::Graphics::BlankImage bgFill{Gorgon::Graphics::RGBA(10, 12, 24, 255)};
    Gorgon::Graphics::BlankImage gridLine{Gorgon::Graphics::RGBA(28, 42, 74, 140)};
    Gorgon::Graphics::BlankImage hudBack{Gorgon::Graphics::RGBA(14, 18, 34, 220)};
    Gorgon::Graphics::BlankImage pauseShade{Gorgon::Graphics::RGBA(5, 8, 16, 175)};
    Gorgon::Graphics::BlankImage whitePixel{Gorgon::Graphics::RGBA(255, 255, 255, 255)};

    // Raw synthesized waves (always available for self-test & export)
    std::array<Gorgon::Containers::Wave, static_cast<size_t>(SfxId::Count)> rawWaves;

private:
    GameAssets() = default;

    bool spritesGenerated = false;
    bool gpuPrepared      = false;
    bool audioGenerated   = false;
    bool audioEnabled     = true;
    float masterVolume    = 0.75f;

#ifdef GORGON_AUDIO_SUPPORT
    std::array<Gorgon::Multimedia::Wave, static_cast<size_t>(SfxId::Count)> soundWaves;
    static constexpr size_t VOICE_POOL_SIZE = 12;
    std::array<std::unique_ptr<Gorgon::Audio::BasicController>, VOICE_POOL_SIZE> voices;
    size_t nextVoice = 0;
#endif
};

} // namespace NeonVortex
