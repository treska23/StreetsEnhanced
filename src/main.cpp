#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr int LogicalWidth = 1920;
constexpr int LogicalHeight = 1080;
constexpr int WindowWidth = 1600;
constexpr int WindowHeight = 900;
constexpr float Pi = 3.14159265358979323846F;

constexpr float PlayerHeight = 430.0F;
constexpr float PlayerSpeedX = 415.0F;
constexpr float PlayerSpeedY = 260.0F;
constexpr float StageTop = 730.0F;
constexpr float StageBottom = 1012.0F;
constexpr float CameraAnchorLeft = 610.0F;
constexpr float CameraAnchorRight = 1040.0F;

const std::filesystem::path ProjectRoot{STREETS_SOURCE_DIR};
const std::filesystem::path AssetsRoot = ProjectRoot / "assets";
const std::filesystem::path BackgroundPath =
    AssetsRoot / "backgrounds" / "round-1-remaster.png";
const std::filesystem::path AxelIdlePath =
    AssetsRoot / "characters" / "axel" / "idle";
const std::filesystem::path AxelWalkPath =
    AssetsRoot / "characters" / "axel" / "walk";
const std::filesystem::path AxelPunchPath =
    AssetsRoot / "generated" / "axel-punch.png";
const std::filesystem::path AxelKickPath =
    AssetsRoot / "generated" / "axel-kick.png";
const std::filesystem::path GalsiaPath =
    AssetsRoot / "generated" / "galsia-idle.png";
const std::filesystem::path ElectraPath =
    AssetsRoot / "generated" / "electra-idle.png";
const std::filesystem::path ShivaPath =
    AssetsRoot / "generated" / "shiva-idle.png";
const std::filesystem::path AntonioPath =
    AssetsRoot / "generated" / "antonio-idle.png";

struct WindowDeleter
{
    void operator()(SDL_Window* value) const noexcept
    {
        SDL_DestroyWindow(value);
    }
};

struct RendererDeleter
{
    void operator()(SDL_Renderer* value) const noexcept
    {
        SDL_DestroyRenderer(value);
    }
};

struct TextureDeleter
{
    void operator()(SDL_Texture* value) const noexcept
    {
        SDL_DestroyTexture(value);
    }
};

using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, TextureDeleter>;

struct TextureAsset
{
    TexturePtr texture;
    float width = 0.0F;
    float height = 0.0F;

    [[nodiscard]] bool IsValid() const
    {
        return texture != nullptr && width > 0.0F && height > 0.0F;
    }
};

struct Animation
{
    std::vector<TextureAsset> frames;
    float frameDuration = 0.12F;

    [[nodiscard]] const TextureAsset* Frame(float time) const
    {
        if (frames.empty())
        {
            return nullptr;
        }

        const auto index = static_cast<std::size_t>(
            std::max(0.0F, time) / frameDuration) % frames.size();
        return &frames[index];
    }
};

enum class PlayerPose
{
    Idle,
    Walk,
    Punch,
    Kick
};

enum class EnemyKind
{
    Galsia,
    Signal,
    Electra,
    Shiva,
    Antonio
};

enum class EnemyState
{
    Entering,
    Chasing,
    Attacking,
    Hurt,
    KnockedOut
};

enum class GameMode
{
    Playing,
    Paused,
    GameOver,
    RoundClear
};

enum class PickupKind
{
    Apple,
    Beef,
    LeadPipe,
    Bottle,
    Pepper
};

enum class WeaponKind
{
    None,
    LeadPipe,
    Bottle,
    Pepper
};

struct Player
{
    float x = 480.0F;
    float feetY = 930.0F;
    float animationTime = 0.0F;
    float attackTimer = 0.0F;
    float invulnerabilityTimer = 0.0F;
    float hitStunTimer = 0.0F;
    float comboWindow = 0.0F;
    float jumpHeight = 0.0F;
    float jumpVelocity = 0.0F;
    int comboStep = 0;
    int attackId = 0;
    int health = 100;
    int lives = 2;
    int specials = 1;
    int score = 0;
    int weaponUses = 0;
    bool facingLeft = false;
    bool moving = false;
    bool jumping = false;
    bool attackConnected = false;
    bool weaponUseConsumed = false;
    WeaponKind weapon = WeaponKind::None;
    PlayerPose pose = PlayerPose::Idle;
};

struct Enemy
{
    EnemyKind kind = EnemyKind::Galsia;
    EnemyState state = EnemyState::Entering;
    float x = 0.0F;
    float feetY = 0.0F;
    float stateTimer = 0.0F;
    float attackCooldown = 0.0F;
    float bobPhase = 0.0F;
    float knockbackVelocity = 0.0F;
    float disappearTimer = 0.0F;
    int health = 40;
    int maximumHealth = 40;
    int lastHitAttackId = -1;
    int palette = 0;
    bool facingLeft = true;
    bool attackConnected = false;
    bool visible = true;
};

struct SpawnRequest
{
    EnemyKind kind = EnemyKind::Galsia;
    int palette = 0;
    float yOffset = 0.0F;
};

struct Wave
{
    float triggerX = 0.0F;
    std::vector<SpawnRequest> enemies;
    std::string title;
};

struct Pickup
{
    PickupKind kind = PickupKind::Apple;
    float x = 0.0F;
    float feetY = 0.0F;
    float phase = 0.0F;
    bool collected = false;
    bool revealed = false;
};

struct StreetContainer
{
    float x = 0.0F;
    float feetY = 0.0F;
    int lastHitAttackId = -1;
    bool broken = false;
};

struct Particle
{
    float x = 0.0F;
    float y = 0.0F;
    float velocityX = 0.0F;
    float velocityY = 0.0F;
    float life = 0.0F;
    float maximumLife = 0.0F;
    float size = 0.0F;
    SDL_Color color{};
};

struct Boomerang
{
    float originX = 0.0F;
    float originY = 0.0F;
    float time = 0.0F;
    float direction = -1.0F;
    bool active = false;
    bool hitPlayer = false;
};

TextureAsset LoadTexture(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    SDL_ScaleMode scaleMode = SDL_SCALEMODE_LINEAR)
{
    TextureAsset asset;
    SDL_Texture* rawTexture = IMG_LoadTexture(renderer, path.string().c_str());
    if (rawTexture == nullptr)
    {
        std::cerr << "No se pudo cargar " << path << ": "
                  << SDL_GetError() << '\n';
        return asset;
    }

    SDL_SetTextureScaleMode(rawTexture, scaleMode);
    SDL_SetTextureBlendMode(rawTexture, SDL_BLENDMODE_BLEND);
    SDL_GetTextureSize(rawTexture, &asset.width, &asset.height);
    asset.texture.reset(rawTexture);
    return asset;
}

int ExtractFrameNumber(const std::filesystem::path& path)
{
    int value = std::numeric_limits<int>::max();
    const std::string stem = path.stem().string();

    for (std::size_t index = 0; index < stem.size(); ++index)
    {
        if (stem[index] < '0' || stem[index] > '9')
        {
            continue;
        }

        value = 0;
        while (index < stem.size() &&
               stem[index] >= '0' && stem[index] <= '9')
        {
            value = (value * 10) + (stem[index] - '0');
            ++index;
        }
    }
    return value;
}

Animation LoadAnimation(
    SDL_Renderer* renderer,
    const std::filesystem::path& folder,
    float frameDuration)
{
    Animation animation;
    animation.frameDuration = frameDuration;
    std::vector<std::filesystem::path> files;

    if (std::filesystem::is_directory(folder))
    {
        for (const auto& entry : std::filesystem::directory_iterator(folder))
        {
            if (entry.is_regular_file() &&
                entry.path().extension() == ".png")
            {
                files.push_back(entry.path());
            }
        }
    }

    std::sort(
        files.begin(),
        files.end(),
        [](const auto& left, const auto& right)
        {
            const int leftFrame = ExtractFrameNumber(left);
            const int rightFrame = ExtractFrameNumber(right);
            return leftFrame == rightFrame
                ? left.filename().string() < right.filename().string()
                : leftFrame < rightFrame;
        });

    for (const auto& path : files)
    {
        TextureAsset frame = LoadTexture(renderer, path);
        if (frame.IsValid())
        {
            animation.frames.push_back(std::move(frame));
        }
    }

    return animation;
}

void SetDrawColor(SDL_Renderer* renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void FillRect(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color color)
{
    SetDrawColor(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

void StrokeRect(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color color)
{
    SetDrawColor(renderer, color);
    SDL_RenderRect(renderer, &rect);
}

void FillEllipse(
    SDL_Renderer* renderer,
    float centerX,
    float centerY,
    float radiusX,
    float radiusY,
    SDL_Color color)
{
    SetDrawColor(renderer, color);
    const int top = static_cast<int>(std::floor(centerY - radiusY));
    const int bottom = static_cast<int>(std::ceil(centerY + radiusY));

    for (int y = top; y <= bottom; ++y)
    {
        const float normalized =
            (static_cast<float>(y) - centerY) / radiusY;
        const float halfWidth =
            radiusX * std::sqrt(std::max(0.0F, 1.0F - normalized * normalized));
        SDL_RenderLine(
            renderer,
            centerX - halfWidth,
            static_cast<float>(y),
            centerX + halfWidth,
            static_cast<float>(y));
    }
}

void DrawText(
    SDL_Renderer* renderer,
    const std::string& text,
    float x,
    float y,
    float scale,
    SDL_Color color)
{
    SetDrawColor(renderer, color);
    SDL_SetRenderScale(renderer, scale, scale);
    SDL_RenderDebugText(renderer, x / scale, y / scale, text.c_str());
    SDL_SetRenderScale(renderer, 1.0F, 1.0F);
}

void DrawTextCentered(
    SDL_Renderer* renderer,
    const std::string& text,
    float centerX,
    float y,
    float scale,
    SDL_Color color)
{
    const float width = static_cast<float>(text.size()) * 8.0F * scale;
    DrawText(renderer, text, centerX - width * 0.5F, y, scale, color);
}

std::string PaddedScore(int value)
{
    std::ostringstream stream;
    stream << std::setw(7) << std::setfill('0') << value;
    return stream.str();
}

void RenderTextureAtFeet(
    SDL_Renderer* renderer,
    const TextureAsset& asset,
    float x,
    float feetY,
    float displayedHeight,
    bool flip,
    double angle = 0.0,
    SDL_Color modulation = {255, 255, 255, 255})
{
    if (!asset.IsValid())
    {
        return;
    }

    const float scale = displayedHeight / asset.height;
    const float width = asset.width * scale;
    const SDL_FRect destination{
        x - width * 0.5F,
        feetY - displayedHeight,
        width,
        displayedHeight};

    SDL_SetTextureColorMod(
        asset.texture.get(),
        modulation.r,
        modulation.g,
        modulation.b);
    SDL_SetTextureAlphaMod(asset.texture.get(), modulation.a);
    SDL_RenderTextureRotated(
        renderer,
        asset.texture.get(),
        nullptr,
        &destination,
        angle,
        nullptr,
        flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(asset.texture.get(), 255, 255, 255);
    SDL_SetTextureAlphaMod(asset.texture.get(), 255);
}

float DistanceSquared(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

class RoundOneDemo
{
public:
    explicit RoundOneDemo(SDL_Renderer* renderer)
        : renderer_(renderer),
          background_(LoadTexture(renderer, BackgroundPath)),
          axelIdle_(LoadAnimation(renderer, AxelIdlePath, 0.16F)),
          axelWalk_(LoadAnimation(renderer, AxelWalkPath, 0.095F)),
          axelPunch_(LoadTexture(renderer, AxelPunchPath)),
          axelKick_(LoadTexture(renderer, AxelKickPath)),
          galsia_(LoadTexture(renderer, GalsiaPath)),
          electra_(LoadTexture(renderer, ElectraPath)),
          shiva_(LoadTexture(renderer, ShivaPath)),
          antonio_(LoadTexture(renderer, AntonioPath))
    {
        BuildWaves();
        Reset();

        std::cout
            << "\nSTREETS ENHANCED - ROUND 1\n"
            << "Mover: WASD / flechas\n"
            << "Punetazo/combo: Z, J o espacio\n"
            << "Patada fuerte: X o K\n"
            << "Saltar / patada aerea: V o I\n"
            << "Especial: C o L\n"
            << "P: pausa  |  R: reiniciar  |  Esc: salir\n\n";
    }

    void Reset()
    {
        player_ = Player{};
        enemies_.clear();
        pendingSpawns_.clear();
        particles_.clear();
        boomerang_ = Boomerang{};
        pickups_ = {
            Pickup{PickupKind::Apple, 1420.0F, 900.0F, 0.0F, false, false},
            Pickup{PickupKind::Bottle, 1545.0F, 940.0F, 0.7F, false, false},
            Pickup{PickupKind::LeadPipe, 2070.0F, 875.0F, 1.4F, false, false},
            Pickup{PickupKind::Apple, 2420.0F, 945.0F, 2.1F, false, false},
            Pickup{PickupKind::Pepper, 2545.0F, 880.0F, 2.8F, false, false},
            Pickup{PickupKind::Beef, 2820.0F, 930.0F, 3.5F, false, false}};
        containers_ = {
            StreetContainer{1420.0F, 760.0F, -1, false},
            StreetContainer{1545.0F, 760.0F, -1, false},
            StreetContainer{2070.0F, 760.0F, -1, false},
            StreetContainer{2420.0F, 760.0F, -1, false},
            StreetContainer{2545.0F, 760.0F, -1, false},
            StreetContainer{2820.0F, 760.0F, -1, false}};
        currentWave_ = 0;
        waveActive_ = false;
        waveBannerTimer_ = 0.0F;
        introTimer_ = 3.6F;
        roundTimeRemaining_ = 180.0F;
        roundClearTimer_ = 0.0F;
        screenFlash_ = 0.0F;
        screenShake_ = 0.0F;
        cameraX_ = 0.0F;
        cameraLockX_ = 0.0F;
        animationClock_ = 0.0F;
        qaFreezeWaves_ = false;
        mode_ = GameMode::Playing;
    }

    [[nodiscard]] bool HandleEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            return false;
        }

        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
        {
            return true;
        }

        if (event.key.key == SDLK_ESCAPE)
        {
            return false;
        }
        if (event.key.key == SDLK_R)
        {
            Reset();
            return true;
        }
        if (event.key.key == SDLK_F2 && mode_ == GameMode::Playing)
        {
            qaFreezeWaves_ = false;
            introTimer_ = 0.0F;
            if (currentWave_ < waves_.size())
            {
                player_.x = waves_[currentWave_].triggerX + 8.0F;
            }
            return true;
        }
        if (event.key.key == SDLK_F3 && mode_ == GameMode::Playing)
        {
            qaFreezeWaves_ = false;
            introTimer_ = 0.0F;
            enemies_.clear();
            pendingSpawns_.clear();
            waveActive_ = false;
            currentWave_ = waves_.size() - 1;
            player_.x = waves_.back().triggerX + 8.0F;
            cameraX_ = std::max(0.0F, player_.x - CameraAnchorRight);
            return true;
        }
        if ((event.key.key == SDLK_F4 || event.key.key == SDLK_F5) &&
            mode_ == GameMode::Playing)
        {
            qaFreezeWaves_ = false;
            introTimer_ = 0.0F;
            enemies_.clear();
            pendingSpawns_.clear();
            waveActive_ = false;
            currentWave_ = event.key.key == SDLK_F4 ? 3 : 4;
            player_.x = waves_[currentWave_].triggerX + 8.0F;
            cameraX_ = std::clamp(
                player_.x - CameraAnchorRight,
                0.0F,
                std::max(0.0F, GetWorldWidth() - static_cast<float>(LogicalWidth)));
            if (event.key.key == SDLK_F5)
            {
                waveActive_ = true;
                cameraLockX_ = cameraX_;
                waveBannerTimer_ = 2.0F;
                pendingSpawns_ = {
                    SpawnRequest{EnemyKind::Electra, 0, -50.0F},
                    SpawnRequest{EnemyKind::Electra, 1, 55.0F}};
            }
            return true;
        }
        if (event.key.key == SDLK_F6 && mode_ == GameMode::Playing)
        {
            qaFreezeWaves_ = true;
            introTimer_ = 0.0F;
            enemies_.clear();
            pendingSpawns_.clear();
            waveActive_ = false;
            player_.x = containers_.front().x - 160.0F;
            player_.feetY = 860.0F;
            cameraX_ = std::max(0.0F, player_.x - CameraAnchorRight);
            return true;
        }
        if (event.key.key == SDLK_P)
        {
            if (mode_ == GameMode::Playing)
            {
                mode_ = GameMode::Paused;
            }
            else if (mode_ == GameMode::Paused)
            {
                mode_ = GameMode::Playing;
            }
            return true;
        }
        if ((mode_ == GameMode::GameOver || mode_ == GameMode::RoundClear) &&
            event.key.key == SDLK_RETURN)
        {
            Reset();
            return true;
        }
        if (mode_ != GameMode::Playing || introTimer_ > 0.45F)
        {
            return true;
        }

        if (event.key.key == SDLK_Z ||
            event.key.key == SDLK_J ||
            event.key.key == SDLK_SPACE)
        {
            BeginComboAttack();
        }
        else if (event.key.key == SDLK_X || event.key.key == SDLK_K)
        {
            BeginAttack(PlayerPose::Kick);
        }
        else if (event.key.key == SDLK_C || event.key.key == SDLK_L)
        {
            UseSpecial();
        }
        else if ((event.key.key == SDLK_V || event.key.key == SDLK_I) &&
                 !player_.jumping &&
                 player_.hitStunTimer <= 0.0F)
        {
            player_.jumping = true;
            player_.jumpVelocity = 670.0F;
            player_.jumpHeight = 1.0F;
        }

        return true;
    }

    void Update(float deltaSeconds)
    {
        if (mode_ != GameMode::Playing)
        {
            return;
        }

        animationClock_ += deltaSeconds;
        introTimer_ = std::max(0.0F, introTimer_ - deltaSeconds);
        waveBannerTimer_ = std::max(0.0F, waveBannerTimer_ - deltaSeconds);
        screenFlash_ = std::max(0.0F, screenFlash_ - deltaSeconds);
        screenShake_ = std::max(0.0F, screenShake_ - 24.0F * deltaSeconds);
        player_.invulnerabilityTimer =
            std::max(0.0F, player_.invulnerabilityTimer - deltaSeconds);
        player_.hitStunTimer =
            std::max(0.0F, player_.hitStunTimer - deltaSeconds);
        player_.comboWindow =
            std::max(0.0F, player_.comboWindow - deltaSeconds);

        if (introTimer_ <= 0.7F)
        {
            if (!qaFreezeWaves_)
            {
                roundTimeRemaining_ =
                    std::max(0.0F, roundTimeRemaining_ - deltaSeconds);
                if (roundTimeRemaining_ <= 0.0F)
                {
                    mode_ = GameMode::GameOver;
                    return;
                }
            }
            UpdatePlayer(deltaSeconds);
            if (!qaFreezeWaves_)
            {
                UpdateWaves();
            }
            UpdateEnemies(deltaSeconds);
            UpdateBoomerang(deltaSeconds);
            UpdatePickups(deltaSeconds);
        }

        UpdateParticles(deltaSeconds);
        UpdateCamera(deltaSeconds);

        if (!qaFreezeWaves_ &&
            currentWave_ >= waves_.size() &&
            !waveActive_ &&
            CountLivingEnemies() == 0 &&
            mode_ == GameMode::Playing)
        {
            player_.score +=
                20000 +
                static_cast<int>(roundTimeRemaining_) * 100 +
                player_.specials * 5000;
            mode_ = GameMode::RoundClear;
            roundClearTimer_ = 0.0F;
        }
    }

    void Render()
    {
        const float shakeX =
            screenShake_ > 0.0F
                ? std::sin(animationClock_ * 93.0F) * screenShake_
                : 0.0F;
        const float shakeY =
            screenShake_ > 0.0F
                ? std::cos(animationClock_ * 77.0F) * screenShake_ * 0.45F
                : 0.0F;

        SDL_SetRenderViewport(
            renderer_,
            nullptr);
        SDL_SetRenderDrawColor(renderer_, 3, 8, 15, 255);
        SDL_RenderClear(renderer_);

        RenderBackground(cameraX_ - shakeX, shakeY);
        RenderAtmosphere();
        RenderContainers(cameraX_ - shakeX, shakeY);
        RenderPickups(cameraX_ - shakeX, shakeY);
        RenderActorShadows(cameraX_ - shakeX, shakeY);
        RenderActors(cameraX_ - shakeX, shakeY);
        RenderBoomerang(cameraX_ - shakeX, shakeY);
        RenderParticles(cameraX_ - shakeX, shakeY);
        RenderHud();
        RenderOverlays();
    }

private:
    void BuildWaves()
    {
        waves_ = {
            Wave{
                1030.0F,
                {
                    {EnemyKind::Galsia, 0, -55.0F},
                    {EnemyKind::Galsia, 1, 55.0F},
                    {EnemyKind::Galsia, 0, 5.0F},
                    {EnemyKind::Signal, 0, -30.0F},
                },
                "FIRST CONTACT"},
            Wave{
                1420.0F,
                {
                    {EnemyKind::Signal, 0, -60.0F},
                    {EnemyKind::Signal, 1, 55.0F},
                },
                "SIGNAL CREW"},
            Wave{
                1780.0F,
                {
                    {EnemyKind::Galsia, 1, -65.0F},
                    {EnemyKind::Galsia, 0, 45.0F},
                    {EnemyKind::Galsia, 1, 0.0F},
                    {EnemyKind::Galsia, 0, -15.0F},
                    {EnemyKind::Signal, 1, -55.0F},
                    {EnemyKind::Signal, 0, 65.0F},
                },
                "STREET AMBUSH"},
            Wave{
                2110.0F,
                {
                    {EnemyKind::Shiva, 0, -45.0F},
                    {EnemyKind::Shiva, 1, 55.0F},
                },
                "SHIVA DUO"},
            Wave{
                2420.0F,
                {
                    {EnemyKind::Galsia, 0, -55.0F},
                    {EnemyKind::Galsia, 1, 55.0F},
                    {EnemyKind::Galsia, 0, 0.0F},
                    {EnemyKind::Electra, 0, -45.0F},
                    {EnemyKind::Electra, 1, 55.0F},
                },
                "WHIP AMBUSH"},
            Wave{
                2740.0F,
                {
                    {EnemyKind::Galsia, 0, -65.0F},
                    {EnemyKind::Galsia, 1, 55.0F},
                    {EnemyKind::Galsia, 0, -15.0F},
                    {EnemyKind::Galsia, 1, 40.0F},
                    {EnemyKind::Galsia, 0, -50.0F},
                    {EnemyKind::Galsia, 1, 65.0F},
                    {EnemyKind::Electra, 1, -30.0F},
                    {EnemyKind::Electra, 0, 45.0F},
                },
                "FINAL STREET STOP"},
            Wave{
                3030.0F,
                {
                    {EnemyKind::Antonio, 0, 0.0F},
                },
                "ANTONIO // BOOMER"}};
    }

    void BeginComboAttack()
    {
        if (player_.hitStunTimer > 0.0F || player_.attackTimer > 0.0F)
        {
            return;
        }

        if (player_.weapon == WeaponKind::Pepper)
        {
            UsePepper();
            return;
        }

        if (player_.jumping)
        {
            BeginAttack(PlayerPose::Kick);
            return;
        }

        if (player_.comboWindow <= 0.0F)
        {
            player_.comboStep = 0;
        }

        const PlayerPose pose =
            player_.comboStep == 2 ? PlayerPose::Kick : PlayerPose::Punch;
        BeginAttack(pose);
        player_.comboStep = (player_.comboStep + 1) % 3;
        player_.comboWindow = 0.62F;
    }

    void BeginAttack(PlayerPose pose)
    {
        if (player_.hitStunTimer > 0.0F || player_.attackTimer > 0.0F)
        {
            return;
        }

        player_.pose = pose;
        player_.attackTimer = pose == PlayerPose::Kick ? 0.52F : 0.34F;
        player_.attackConnected = false;
        player_.weaponUseConsumed = false;
        ++player_.attackId;
        player_.animationTime = 0.0F;
    }

    void UsePepper()
    {
        const float direction = player_.facingLeft ? -1.0F : 1.0F;
        BeginAttack(PlayerPose::Punch);
        player_.weapon = WeaponKind::None;
        player_.weaponUses = 0;

        for (auto& enemy : enemies_)
        {
            if (!enemy.visible || enemy.state == EnemyState::KnockedOut)
            {
                continue;
            }

            const float alongAttack = (enemy.x - player_.x) * direction;
            if (alongAttack >= 0.0F &&
                alongAttack <= 620.0F &&
                std::abs(enemy.feetY - player_.feetY) <= 155.0F)
            {
                enemy.state = EnemyState::Hurt;
                enemy.stateTimer = 1.35F;
                enemy.knockbackVelocity = 0.0F;
                player_.score += 75;
            }
        }

        for (int index = 0; index < 28; ++index)
        {
            const float spread =
                static_cast<float>((index * 43) % 100) / 100.0F - 0.5F;
            SpawnParticle(
                player_.x + direction * 95.0F,
                player_.feetY - 225.0F,
                direction * (210.0F + static_cast<float>(index % 8) * 34.0F),
                spread * 220.0F,
                0.5F,
                5.0F + static_cast<float>(index % 3) * 2.0F,
                SDL_Color{236, 222, 106, 210});
        }
    }

    void UseSpecial()
    {
        if (player_.specials <= 0 ||
            player_.attackTimer > 0.0F ||
            player_.hitStunTimer > 0.0F)
        {
            return;
        }

        --player_.specials;
        screenFlash_ = 0.48F;
        screenShake_ = 18.0F;

        for (auto& enemy : enemies_)
        {
            if (enemy.visible && enemy.state != EnemyState::KnockedOut)
            {
                DamageEnemy(enemy, enemy.kind == EnemyKind::Antonio ? 65 : 90);
            }
        }

        for (int index = 0; index < 44; ++index)
        {
            const float angle =
                static_cast<float>(index) / 44.0F * Pi * 2.0F;
            SpawnParticle(
                player_.x,
                player_.feetY - 210.0F,
                std::cos(angle) * (180.0F + static_cast<float>(index % 5) * 35.0F),
                std::sin(angle) * (180.0F + static_cast<float>(index % 7) * 24.0F),
                0.65F,
                8.0F,
                index % 2 == 0
                    ? SDL_Color{62, 232, 255, 240}
                    : SDL_Color{255, 91, 201, 240});
        }
    }

    void UpdatePlayer(float deltaSeconds)
    {
        player_.animationTime += deltaSeconds;
        if (player_.jumping)
        {
            player_.jumpHeight += player_.jumpVelocity * deltaSeconds;
            player_.jumpVelocity -= 1540.0F * deltaSeconds;
            if (player_.jumpHeight <= 0.0F)
            {
                player_.jumpHeight = 0.0F;
                player_.jumpVelocity = 0.0F;
                player_.jumping = false;
            }
        }

        if (player_.attackTimer > 0.0F)
        {
            player_.attackTimer =
                std::max(0.0F, player_.attackTimer - deltaSeconds);
            const float elapsed =
                player_.pose == PlayerPose::Kick
                    ? 0.52F - player_.attackTimer
                    : 0.34F - player_.attackTimer;
            const bool attackActive =
                player_.pose == PlayerPose::Kick
                    ? elapsed >= 0.18F && elapsed <= 0.36F
                    : elapsed >= 0.09F && elapsed <= 0.23F;

            if (attackActive)
            {
                ResolvePlayerAttack();
            }
            if (player_.attackTimer <= 0.0F)
            {
                player_.pose = PlayerPose::Idle;
            }
            return;
        }

        if (player_.hitStunTimer > 0.0F)
        {
            player_.moving = false;
            return;
        }

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        float horizontal = 0.0F;
        float vertical = 0.0F;

        if (keyboard[SDL_SCANCODE_LEFT] || keyboard[SDL_SCANCODE_A])
        {
            horizontal -= 1.0F;
        }
        if (keyboard[SDL_SCANCODE_RIGHT] || keyboard[SDL_SCANCODE_D])
        {
            horizontal += 1.0F;
        }
        if (keyboard[SDL_SCANCODE_UP] || keyboard[SDL_SCANCODE_W])
        {
            vertical -= 1.0F;
        }
        if (keyboard[SDL_SCANCODE_DOWN] || keyboard[SDL_SCANCODE_S])
        {
            vertical += 1.0F;
        }

        player_.moving = horizontal != 0.0F || vertical != 0.0F;
        if (player_.moving)
        {
            const float length =
                std::sqrt(horizontal * horizontal + vertical * vertical);
            horizontal /= length;
            vertical /= length;
            player_.x += horizontal * PlayerSpeedX * deltaSeconds;
            player_.feetY += vertical * PlayerSpeedY * deltaSeconds;
            player_.pose = PlayerPose::Walk;

            if (horizontal < 0.0F)
            {
                player_.facingLeft = true;
            }
            else if (horizontal > 0.0F)
            {
                player_.facingLeft = false;
            }
        }
        else
        {
            player_.pose = PlayerPose::Idle;
        }

        const float worldWidth = GetWorldWidth();
        player_.x = std::clamp(player_.x, 140.0F, worldWidth - 130.0F);
        player_.feetY = std::clamp(player_.feetY, StageTop, StageBottom);

        if (waveActive_)
        {
            player_.x = std::min(player_.x, cameraLockX_ + 1660.0F);
            player_.x = std::max(player_.x, cameraLockX_ + 170.0F);
        }
    }

    void ResolvePlayerAttack()
    {
        float range =
            player_.pose == PlayerPose::Kick ? 255.0F : 185.0F;
        int damage =
            player_.pose == PlayerPose::Kick ? 30 : 18;
        if (player_.jumping && player_.pose == PlayerPose::Kick)
        {
            range = 285.0F;
            damage = 38;
        }
        if (player_.pose == PlayerPose::Punch)
        {
            if (player_.weapon == WeaponKind::LeadPipe)
            {
                range = 285.0F;
                damage = 42;
            }
            else if (player_.weapon == WeaponKind::Bottle)
            {
                range = 225.0F;
                damage = 31;
            }
        }
        const float direction = player_.facingLeft ? -1.0F : 1.0F;
        bool hitSomething = ResolveContainerAttack(range, direction);

        for (auto& enemy : enemies_)
        {
            if (!enemy.visible ||
                enemy.state == EnemyState::KnockedOut ||
                enemy.lastHitAttackId == player_.attackId)
            {
                continue;
            }

            const float alongAttack =
                (enemy.x - player_.x) * direction;
            if (alongAttack >= -25.0F &&
                alongAttack <= range &&
                std::abs(enemy.feetY - player_.feetY) <= 92.0F)
            {
                enemy.lastHitAttackId = player_.attackId;
                DamageEnemy(enemy, damage);
                player_.attackConnected = true;
                hitSomething = true;
            }
        }

        if (hitSomething &&
            !player_.weaponUseConsumed &&
            player_.weapon != WeaponKind::None)
        {
            player_.weaponUseConsumed = true;
            if (player_.weapon != WeaponKind::LeadPipe)
            {
                --player_.weaponUses;
                if (player_.weaponUses <= 0)
                {
                    player_.weapon = WeaponKind::None;
                    player_.weaponUses = 0;
                }
            }
        }
    }

    bool ResolveContainerAttack(float range, float direction)
    {
        bool hit = false;
        for (std::size_t index = 0; index < containers_.size(); ++index)
        {
            StreetContainer& container = containers_[index];
            if (container.broken ||
                container.lastHitAttackId == player_.attackId)
            {
                continue;
            }

            const float alongAttack =
                (container.x - player_.x) * direction;
            if (alongAttack < -20.0F ||
                alongAttack > range ||
                std::abs(player_.feetY - 820.0F) > 190.0F)
            {
                continue;
            }

            container.lastHitAttackId = player_.attackId;
            container.broken = true;
            pickups_[index].revealed = true;
            player_.score += 100;
            hit = true;
            screenShake_ = std::max(screenShake_, 5.0F);

            for (int shard = 0; shard < 20; ++shard)
            {
                SpawnParticle(
                    container.x,
                    container.feetY - 95.0F,
                    static_cast<float>((shard * 53) % 300) - 150.0F,
                    -90.0F - static_cast<float>((shard * 37) % 260),
                    0.55F,
                    6.0F + static_cast<float>(shard % 4) * 2.0F,
                    shard % 2 == 0
                        ? SDL_Color{91, 220, 255, 230}
                        : SDL_Color{255, 78, 165, 230});
            }
        }
        return hit;
    }

    void DamageEnemy(Enemy& enemy, int damage)
    {
        if (enemy.state == EnemyState::KnockedOut)
        {
            return;
        }

        enemy.health -= damage;
        enemy.state = enemy.health <= 0
            ? EnemyState::KnockedOut
            : EnemyState::Hurt;
        enemy.stateTimer = enemy.health <= 0 ? 0.8F : 0.24F;
        enemy.knockbackVelocity =
            (enemy.x >= player_.x ? 1.0F : -1.0F) *
            (enemy.health <= 0 ? 430.0F : 170.0F);
        screenShake_ = enemy.health <= 0 ? 12.0F : 6.0F;
        player_.score += enemy.health <= 0
            ? (enemy.kind == EnemyKind::Antonio ? 5000 : 650)
            : 90;

        const SDL_Color impactColor =
            enemy.kind == EnemyKind::Antonio
                ? SDL_Color{255, 191, 68, 255}
                : SDL_Color{255, 75, 137, 255};
        for (int index = 0; index < 13; ++index)
        {
            const float spread =
                (static_cast<float>((index * 47) % 100) / 100.0F - 0.5F);
            SpawnParticle(
                enemy.x,
                enemy.feetY - 205.0F,
                enemy.knockbackVelocity * 0.35F + spread * 360.0F,
                -160.0F - static_cast<float>((index * 31) % 180),
                0.38F + static_cast<float>(index % 4) * 0.05F,
                7.0F + static_cast<float>(index % 3) * 3.0F,
                impactColor);
        }
    }

    void DamagePlayer(int damage, float sourceX)
    {
        if (player_.invulnerabilityTimer > 0.0F ||
            player_.jumpHeight > 70.0F ||
            mode_ != GameMode::Playing)
        {
            return;
        }

        player_.health -= damage;
        player_.invulnerabilityTimer = 1.05F;
        player_.hitStunTimer = 0.42F;
        player_.attackTimer = 0.0F;
        player_.pose = PlayerPose::Idle;
        player_.weapon = WeaponKind::None;
        player_.weaponUses = 0;
        player_.x += sourceX < player_.x ? 75.0F : -75.0F;
        screenShake_ = 13.0F;

        for (int index = 0; index < 16; ++index)
        {
            SpawnParticle(
                player_.x,
                player_.feetY - 210.0F,
                static_cast<float>((index * 67) % 340) - 170.0F,
                -110.0F - static_cast<float>((index * 29) % 210),
                0.48F,
                8.0F,
                SDL_Color{56, 218, 255, 255});
        }

        if (player_.health <= 0)
        {
            if (player_.lives > 0)
            {
                --player_.lives;
                player_.health = 100;
                player_.invulnerabilityTimer = 2.4F;
                player_.x = std::clamp(
                    cameraX_ + 620.0F,
                    180.0F,
                    GetWorldWidth() - 180.0F);
                player_.feetY = 920.0F;
            }
            else
            {
                player_.health = 0;
                mode_ = GameMode::GameOver;
            }
        }
    }

    void UpdateWaves()
    {
        if (!waveActive_ &&
            currentWave_ < waves_.size() &&
            player_.x >= waves_[currentWave_].triggerX)
        {
            waveActive_ = true;
            cameraLockX_ = cameraX_;
            waveBannerTimer_ = 2.0F;
            pendingSpawns_ = waves_[currentWave_].enemies;
        }

        if (!waveActive_)
        {
            return;
        }

        while (CountLivingEnemies() < 2 && !pendingSpawns_.empty())
        {
            const SpawnRequest request = pendingSpawns_.front();
            pendingSpawns_.erase(pendingSpawns_.begin());
            SpawnEnemy(request);
        }

        if (pendingSpawns_.empty() && CountLivingEnemies() == 0)
        {
            waveActive_ = false;
            ++currentWave_;
            player_.score += 1000;
        }
    }

    void SpawnEnemy(const SpawnRequest& request)
    {
        Enemy enemy;
        enemy.kind = request.kind;
        enemy.palette = request.palette;
        enemy.feetY = std::clamp(
            900.0F + request.yOffset,
            StageTop + 20.0F,
            StageBottom);
        enemy.bobPhase =
            static_cast<float>((enemies_.size() * 137) % 100) / 100.0F * Pi;
        enemy.facingLeft = true;
        enemy.state = EnemyState::Entering;
        enemy.stateTimer = 0.4F;

        if (request.kind == EnemyKind::Antonio)
        {
            enemy.maximumHealth = 260;
            enemy.health = 260;
            enemy.x = std::clamp(
                player_.x - 720.0F,
                cameraX_ + 190.0F,
                GetWorldWidth() - 180.0F);
        }
        else
        {
            enemy.maximumHealth =
                request.kind == EnemyKind::Signal ? 58 :
                request.kind == EnemyKind::Electra ? 76 :
                request.kind == EnemyKind::Shiva ? 68 : 48;
            enemy.health = enemy.maximumHealth;
            const bool enterFromLeft =
                (enemies_.size() + static_cast<std::size_t>(request.palette)) %
                    3 == 0 &&
                cameraX_ > 300.0F;
            enemy.x = enterFromLeft
                ? cameraX_ + 70.0F
                : std::min(GetWorldWidth() - 120.0F, cameraX_ + 1840.0F);
        }

        enemies_.push_back(enemy);
    }

    int CountLivingEnemies() const
    {
        return static_cast<int>(
            std::count_if(
                enemies_.begin(),
                enemies_.end(),
                [](const Enemy& enemy)
                {
                    return enemy.visible &&
                           enemy.state != EnemyState::KnockedOut;
                }));
    }

    void UpdateEnemies(float deltaSeconds)
    {
        for (auto& enemy : enemies_)
        {
            if (!enemy.visible)
            {
                continue;
            }

            enemy.bobPhase += deltaSeconds * 4.4F;
            enemy.attackCooldown =
                std::max(0.0F, enemy.attackCooldown - deltaSeconds);

            if (enemy.state == EnemyState::KnockedOut)
            {
                enemy.stateTimer -= deltaSeconds;
                enemy.x += enemy.knockbackVelocity * deltaSeconds;
                enemy.knockbackVelocity *= std::pow(0.035F, deltaSeconds);
                if (enemy.stateTimer <= 0.0F)
                {
                    enemy.disappearTimer += deltaSeconds;
                    if (enemy.disappearTimer > 0.55F)
                    {
                        enemy.visible = false;
                    }
                }
                continue;
            }

            if (enemy.state == EnemyState::Hurt)
            {
                enemy.stateTimer -= deltaSeconds;
                enemy.x += enemy.knockbackVelocity * deltaSeconds;
                enemy.knockbackVelocity *= std::pow(0.02F, deltaSeconds);
                if (enemy.stateTimer <= 0.0F)
                {
                    enemy.state = EnemyState::Chasing;
                    enemy.attackCooldown = 0.28F;
                }
                continue;
            }

            if (enemy.state == EnemyState::Entering)
            {
                enemy.stateTimer -= deltaSeconds;
                if (enemy.stateTimer <= 0.0F)
                {
                    enemy.state = EnemyState::Chasing;
                }
            }

            if (enemy.state == EnemyState::Attacking)
            {
                enemy.stateTimer -= deltaSeconds;
                if (!enemy.attackConnected && enemy.stateTimer <= 0.28F)
                {
                    enemy.attackConnected = true;
                    const float hitRange =
                        enemy.kind == EnemyKind::Antonio ? 180.0F :
                        enemy.kind == EnemyKind::Electra ? 285.0F :
                        enemy.kind == EnemyKind::Shiva ? 145.0F : 125.0F;
                    if (std::abs(enemy.x - player_.x) <= hitRange &&
                        std::abs(enemy.feetY - player_.feetY) <= 82.0F)
                    {
                        DamagePlayer(
                            enemy.kind == EnemyKind::Antonio ? 22 :
                            enemy.kind == EnemyKind::Electra ? 18 :
                            enemy.kind == EnemyKind::Shiva ? 17 :
                            enemy.kind == EnemyKind::Signal ? 15 : 12,
                            enemy.x);
                    }
                }
                if (enemy.stateTimer <= 0.0F)
                {
                    enemy.state = EnemyState::Chasing;
                    enemy.attackCooldown =
                        enemy.kind == EnemyKind::Antonio ? 0.72F :
                        enemy.kind == EnemyKind::Electra ? 1.05F : 0.56F;
                }
                continue;
            }

            const float dx = player_.x - enemy.x;
            const float dy = player_.feetY - enemy.feetY;
            enemy.facingLeft = dx < 0.0F;
            const float absoluteX = std::abs(dx);
            const float absoluteY = std::abs(dy);

            if (enemy.kind == EnemyKind::Antonio &&
                enemy.attackCooldown <= 0.0F &&
                !boomerang_.active &&
                absoluteX > 310.0F &&
                absoluteX < 820.0F &&
                absoluteY < 105.0F)
            {
                ThrowBoomerang(enemy);
                enemy.attackCooldown = 1.5F;
                continue;
            }

            const float attackRange =
                enemy.kind == EnemyKind::Antonio ? 155.0F :
                enemy.kind == EnemyKind::Electra ? 255.0F :
                enemy.kind == EnemyKind::Shiva ? 125.0F : 105.0F;
            if (absoluteX <= attackRange &&
                absoluteY <= 62.0F &&
                enemy.attackCooldown <= 0.0F)
            {
                enemy.state = EnemyState::Attacking;
                enemy.stateTimer =
                    enemy.kind == EnemyKind::Antonio ? 0.72F :
                    enemy.kind == EnemyKind::Electra ? 0.86F :
                    enemy.kind == EnemyKind::Shiva ? 0.52F : 0.58F;
                enemy.attackConnected = false;
                continue;
            }

            const float speed =
                enemy.kind == EnemyKind::Antonio ? 112.0F :
                enemy.kind == EnemyKind::Electra ? 116.0F :
                enemy.kind == EnemyKind::Shiva ? 195.0F :
                enemy.kind == EnemyKind::Signal ? 178.0F : 142.0F;
            if (absoluteY > 38.0F)
            {
                enemy.feetY +=
                    std::clamp(dy, -1.0F, 1.0F) * speed * 0.72F * deltaSeconds;
            }
            else if (absoluteX > attackRange * 0.82F)
            {
                enemy.x +=
                    std::clamp(dx, -1.0F, 1.0F) * speed * deltaSeconds;
            }

            enemy.feetY = std::clamp(enemy.feetY, StageTop, StageBottom);
            enemy.x = std::clamp(enemy.x, 90.0F, GetWorldWidth() - 90.0F);
        }
    }

    void ThrowBoomerang(Enemy& enemy)
    {
        boomerang_.active = true;
        boomerang_.hitPlayer = false;
        boomerang_.time = 0.0F;
        boomerang_.originX = enemy.x;
        boomerang_.originY = enemy.feetY - 255.0F;
        boomerang_.direction = enemy.facingLeft ? -1.0F : 1.0F;
    }

    void UpdateBoomerang(float deltaSeconds)
    {
        if (!boomerang_.active)
        {
            return;
        }

        boomerang_.time += deltaSeconds;
        const float progress = boomerang_.time / 1.42F;
        const float x = boomerang_.originX +
            boomerang_.direction * std::sin(progress * Pi) * 760.0F;
        const float y = boomerang_.originY +
            std::sin(progress * Pi * 2.0F) * 42.0F;

        if (!boomerang_.hitPlayer &&
            std::abs(x - player_.x) < 82.0F &&
            std::abs(y - (player_.feetY - 210.0F)) < 115.0F)
        {
            boomerang_.hitPlayer = true;
            DamagePlayer(18, x);
        }

        if (progress >= 1.0F)
        {
            boomerang_.active = false;
        }
    }

    void UpdatePickups(float deltaSeconds)
    {
        for (auto& pickup : pickups_)
        {
            if (pickup.collected || !pickup.revealed)
            {
                continue;
            }

            pickup.phase += deltaSeconds * 3.0F;
            if (DistanceSquared(
                    pickup.x,
                    pickup.feetY,
                    player_.x,
                    player_.feetY) < 78.0F * 78.0F)
            {
                pickup.collected = true;
                if (pickup.kind == PickupKind::Apple ||
                    pickup.kind == PickupKind::Beef)
                {
                    const int healing =
                        pickup.kind == PickupKind::Beef ? 100 : 50;
                    player_.health = std::min(100, player_.health + healing);
                    player_.score +=
                        pickup.kind == PickupKind::Beef ? 1000 : 350;
                }
                else if (pickup.kind == PickupKind::LeadPipe)
                {
                    player_.weapon = WeaponKind::LeadPipe;
                    player_.weaponUses = 99;
                    player_.score += 250;
                }
                else if (pickup.kind == PickupKind::Bottle)
                {
                    player_.weapon = WeaponKind::Bottle;
                    player_.weaponUses = 6;
                    player_.score += 200;
                }
                else if (pickup.kind == PickupKind::Pepper)
                {
                    player_.weapon = WeaponKind::Pepper;
                    player_.weaponUses = 1;
                    player_.score += 300;
                }

                for (int index = 0; index < 18; ++index)
                {
                    const float angle =
                        static_cast<float>(index) / 18.0F * Pi * 2.0F;
                    SpawnParticle(
                        pickup.x,
                        pickup.feetY - 70.0F,
                        std::cos(angle) * 170.0F,
                        std::sin(angle) * 170.0F,
                        0.55F,
                        7.0F,
                        SDL_Color{125, 255, 126, 255});
                }
            }
        }
    }

    void SpawnParticle(
        float x,
        float y,
        float velocityX,
        float velocityY,
        float life,
        float size,
        SDL_Color color)
    {
        particles_.push_back(
            Particle{
                x,
                y,
                velocityX,
                velocityY,
                life,
                life,
                size,
                color});
    }

    void UpdateParticles(float deltaSeconds)
    {
        for (auto& particle : particles_)
        {
            particle.life -= deltaSeconds;
            particle.x += particle.velocityX * deltaSeconds;
            particle.y += particle.velocityY * deltaSeconds;
            particle.velocityY += 580.0F * deltaSeconds;
            particle.velocityX *= std::pow(0.12F, deltaSeconds);
        }

        std::erase_if(
            particles_,
            [](const Particle& particle)
            {
                return particle.life <= 0.0F;
            });
    }

    void UpdateCamera(float deltaSeconds)
    {
        const float maximumCamera =
            std::max(0.0F, GetWorldWidth() - static_cast<float>(LogicalWidth));
        float target = cameraX_;
        const float screenX = player_.x - cameraX_;

        if (screenX > CameraAnchorRight)
        {
            target = player_.x - CameraAnchorRight;
        }
        else if (screenX < CameraAnchorLeft)
        {
            target = player_.x - CameraAnchorLeft;
        }

        if (waveActive_)
        {
            target = std::min(target, cameraLockX_);
        }

        target = std::clamp(target, 0.0F, maximumCamera);
        const float smoothing =
            1.0F - std::exp(-8.0F * deltaSeconds);
        cameraX_ += (target - cameraX_) * smoothing;
        cameraX_ = std::clamp(cameraX_, 0.0F, maximumCamera);
    }

    float GetWorldWidth() const
    {
        if (!background_.IsValid())
        {
            return 3400.0F;
        }

        return std::max(
            static_cast<float>(LogicalWidth),
            background_.width *
                (static_cast<float>(LogicalHeight) / background_.height));
    }

    void RenderBackground(float camera, float offsetY)
    {
        if (!background_.IsValid())
        {
            FillRect(
                renderer_,
                SDL_FRect{0.0F, 0.0F, 1920.0F, 1080.0F},
                SDL_Color{7, 16, 28, 255});
            return;
        }

        const float textureToWorld =
            static_cast<float>(LogicalHeight) / background_.height;
        const SDL_FRect source{
            std::clamp(camera / textureToWorld, 0.0F,
                std::max(0.0F, background_.width -
                    static_cast<float>(LogicalWidth) / textureToWorld)),
            0.0F,
            static_cast<float>(LogicalWidth) / textureToWorld,
            background_.height};
        const SDL_FRect destination{
            0.0F,
            offsetY,
            static_cast<float>(LogicalWidth),
            static_cast<float>(LogicalHeight)};
        SDL_RenderTexture(
            renderer_,
            background_.texture.get(),
            &source,
            &destination);
    }

    void RenderAtmosphere()
    {
        FillRect(
            renderer_,
            SDL_FRect{0.0F, 0.0F, 1920.0F, 160.0F},
            SDL_Color{2, 8, 16, 115});

        for (int index = 0; index < 76; ++index)
        {
            const float x = std::fmod(
                static_cast<float>(index * 173) +
                    animationClock_ * (92.0F + static_cast<float>(index % 5) * 17.0F),
                2010.0F) - 45.0F;
            const float y = std::fmod(
                static_cast<float>(index * 97) +
                    animationClock_ * (230.0F + static_cast<float>(index % 7) * 19.0F),
                1080.0F);
            SetDrawColor(
                renderer_,
                index % 5 == 0
                    ? SDL_Color{117, 206, 255, 78}
                    : SDL_Color{98, 153, 198, 48});
            SDL_RenderLine(renderer_, x, y, x - 8.0F, y + 25.0F);
        }

        const float pulse = 0.5F + 0.5F * std::sin(animationClock_ * 2.2F);
        FillRect(
            renderer_,
            SDL_FRect{0.0F, 540.0F, 1920.0F, 180.0F},
            SDL_Color{
                38,
                8,
                61,
                static_cast<Uint8>(12.0F + pulse * 12.0F)});
    }

    void RenderContainers(float camera, float offsetY)
    {
        for (std::size_t index = 0; index < containers_.size(); ++index)
        {
            const StreetContainer& container = containers_[index];
            const float x = container.x - camera;
            const float baseY = container.feetY + offsetY;
            if (x < -130.0F || x > 2050.0F)
            {
                continue;
            }

            if (container.broken)
            {
                FillRect(
                    renderer_,
                    SDL_FRect{x - 54.0F, baseY - 18.0F, 108.0F, 18.0F},
                    SDL_Color{28, 38, 48, 210});
                for (int shard = 0; shard < 5; ++shard)
                {
                    FillRect(
                        renderer_,
                        SDL_FRect{
                            x - 47.0F + static_cast<float>(shard) * 21.0F,
                            baseY - 27.0F - static_cast<float>(shard % 2) * 8.0F,
                            14.0F,
                            8.0F},
                        shard % 2 == 0
                            ? SDL_Color{72, 187, 211, 190}
                            : SDL_Color{187, 57, 116, 190});
                }
                continue;
            }

            const float pulse =
                0.5F + 0.5F * std::sin(animationClock_ * 2.5F + index);
            FillRect(
                renderer_,
                SDL_FRect{x - 58.0F, baseY - 268.0F, 116.0F, 268.0F},
                SDL_Color{18, 39, 53, 222});
            FillRect(
                renderer_,
                SDL_FRect{x - 50.0F, baseY - 228.0F, 100.0F, 174.0F},
                SDL_Color{29, 78, 96, 120});
            StrokeRect(
                renderer_,
                SDL_FRect{x - 58.0F, baseY - 268.0F, 116.0F, 268.0F},
                SDL_Color{
                    70,
                    214,
                    242,
                    static_cast<Uint8>(170.0F + pulse * 85.0F)});
            StrokeRect(
                renderer_,
                SDL_FRect{x - 50.0F, baseY - 228.0F, 100.0F, 174.0F},
                SDL_Color{203, 68, 151, 220});
            SetDrawColor(renderer_, SDL_Color{63, 184, 207, 145});
            SDL_RenderLine(renderer_, x, baseY - 228.0F, x, baseY - 54.0F);
            SDL_RenderLine(renderer_, x - 50.0F, baseY - 139.0F, x + 50.0F, baseY - 139.0F);
            FillRect(
                renderer_,
                SDL_FRect{x - 48.0F, baseY - 258.0F, 96.0F, 24.0F},
                SDL_Color{132, 26, 77, 230});
            DrawTextCentered(
                renderer_,
                "PHONE",
                x,
                baseY - 254.0F,
                1.0F,
                SDL_Color{242, 183, 224, 255});
        }
    }

    void RenderPickups(float camera, float offsetY)
    {
        for (const auto& pickup : pickups_)
        {
            if (pickup.collected || !pickup.revealed)
            {
                continue;
            }

            const float x = pickup.x - camera;
            if (x < -100.0F || x > 2020.0F)
            {
                continue;
            }

            const float bob = std::sin(pickup.phase) * 8.0F;
            const float y = pickup.feetY + offsetY - 58.0F + bob;
            const Uint8 glow = static_cast<Uint8>(
                46.0F + (0.5F + 0.5F * std::sin(pickup.phase)) * 44.0F);
            FillEllipse(
                renderer_,
                x,
                pickup.feetY + offsetY - 7.0F,
                54.0F,
                14.0F,
                SDL_Color{94, 255, 137, glow});

            if (pickup.kind == PickupKind::Apple)
            {
                FillEllipse(
                    renderer_,
                    x,
                    y,
                    25.0F,
                    24.0F,
                    SDL_Color{226, 39, 59, 255});
                FillEllipse(
                    renderer_,
                    x + 15.0F,
                    y - 21.0F,
                    11.0F,
                    6.0F,
                    SDL_Color{93, 216, 92, 255});
                SetDrawColor(renderer_, SDL_Color{116, 65, 35, 255});
                SDL_RenderLine(renderer_, x + 2.0F, y - 20.0F, x + 8.0F, y - 34.0F);
            }
            else if (pickup.kind == PickupKind::Beef)
            {
                FillEllipse(
                    renderer_,
                    x,
                    y,
                    39.0F,
                    25.0F,
                    SDL_Color{149, 71, 44, 255});
                FillEllipse(
                    renderer_,
                    x - 10.0F,
                    y - 5.0F,
                    25.0F,
                    13.0F,
                    SDL_Color{229, 133, 76, 255});
                FillEllipse(
                    renderer_,
                    x + 31.0F,
                    y + 4.0F,
                    13.0F,
                    9.0F,
                    SDL_Color{241, 231, 189, 255});
            }
            else if (pickup.kind == PickupKind::LeadPipe)
            {
                SetDrawColor(renderer_, SDL_Color{204, 220, 229, 255});
                for (int thickness = -4; thickness <= 4; ++thickness)
                {
                    SDL_RenderLine(
                        renderer_,
                        x - 38.0F,
                        y + 17.0F + static_cast<float>(thickness),
                        x + 38.0F,
                        y - 17.0F + static_cast<float>(thickness));
                }
                FillEllipse(
                    renderer_,
                    x + 39.0F,
                    y - 18.0F,
                    10.0F,
                    12.0F,
                    SDL_Color{90, 104, 114, 255});
            }
            else if (pickup.kind == PickupKind::Bottle)
            {
                FillRect(
                    renderer_,
                    SDL_FRect{x - 14.0F, y - 28.0F, 28.0F, 50.0F},
                    SDL_Color{46, 177, 143, 230});
                FillRect(
                    renderer_,
                    SDL_FRect{x - 7.0F, y - 43.0F, 14.0F, 18.0F},
                    SDL_Color{98, 225, 183, 240});
                StrokeRect(
                    renderer_,
                    SDL_FRect{x - 14.0F, y - 28.0F, 28.0F, 50.0F},
                    SDL_Color{179, 255, 230, 255});
            }
            else
            {
                FillEllipse(
                    renderer_,
                    x,
                    y,
                    21.0F,
                    28.0F,
                    SDL_Color{242, 204, 74, 255});
                FillRect(
                    renderer_,
                    SDL_FRect{x - 8.0F, y - 42.0F, 16.0F, 15.0F},
                    SDL_Color{200, 52, 67, 255});
                DrawTextCentered(
                    renderer_,
                    "P",
                    x,
                    y - 8.0F,
                    1.4F,
                    SDL_Color{59, 39, 20, 255});
            }
        }
    }

    void RenderActorShadows(float camera, float offsetY)
    {
        FillEllipse(
            renderer_,
            player_.x - camera,
            player_.feetY + offsetY - 4.0F,
            105.0F - std::min(42.0F, player_.jumpHeight * 0.11F),
            22.0F - std::min(9.0F, player_.jumpHeight * 0.024F),
            SDL_Color{
                0,
                4,
                10,
                static_cast<Uint8>(
                    110.0F - std::min(55.0F, player_.jumpHeight * 0.15F))});

        for (const auto& enemy : enemies_)
        {
            if (!enemy.visible)
            {
                continue;
            }
            const float alpha =
                enemy.state == EnemyState::KnockedOut
                    ? std::clamp(enemy.stateTimer + 0.2F, 0.0F, 1.0F)
                    : 1.0F;
            FillEllipse(
                renderer_,
                enemy.x - camera,
                enemy.feetY + offsetY - 4.0F,
                enemy.kind == EnemyKind::Antonio ? 122.0F : 88.0F,
                enemy.kind == EnemyKind::Antonio ? 25.0F : 19.0F,
                SDL_Color{0, 3, 8, static_cast<Uint8>(105.0F * alpha)});
        }
    }

    void RenderActors(float camera, float offsetY)
    {
        struct DrawItem
        {
            float feetY = 0.0F;
            bool player = false;
            std::size_t enemyIndex = 0;
        };

        std::vector<DrawItem> order;
        order.push_back(DrawItem{player_.feetY, true, 0});
        for (std::size_t index = 0; index < enemies_.size(); ++index)
        {
            if (enemies_[index].visible)
            {
                order.push_back(
                    DrawItem{enemies_[index].feetY, false, index});
            }
        }
        std::sort(
            order.begin(),
            order.end(),
            [](const DrawItem& left, const DrawItem& right)
            {
                return left.feetY < right.feetY;
            });

        for (const DrawItem& item : order)
        {
            if (item.player)
            {
                RenderPlayer(camera, offsetY);
            }
            else
            {
                RenderEnemy(enemies_[item.enemyIndex], camera, offsetY);
            }
        }
    }

    void RenderPlayer(float camera, float offsetY)
    {
        const TextureAsset* frame = nullptr;
        float height = PlayerHeight;
        float x = player_.x - camera;
        float y = player_.feetY + offsetY - player_.jumpHeight;

        if (player_.pose == PlayerPose::Punch && axelPunch_.IsValid())
        {
            frame = &axelPunch_;
            height = 455.0F;
            x += player_.facingLeft ? -34.0F : 34.0F;
        }
        else if (player_.pose == PlayerPose::Kick && axelKick_.IsValid())
        {
            frame = &axelKick_;
            height = 470.0F;
            x += player_.facingLeft ? -38.0F : 38.0F;
        }
        else if (player_.pose == PlayerPose::Walk)
        {
            frame = axelWalk_.Frame(player_.animationTime);
        }
        else
        {
            frame = axelIdle_.Frame(player_.animationTime);
        }

        if (frame == nullptr || !frame->IsValid())
        {
            FillRect(
                renderer_,
                SDL_FRect{x - 48.0F, y - 250.0F, 96.0F, 250.0F},
                SDL_Color{230, 230, 232, 255});
            return;
        }

        SDL_Color modulation{255, 255, 255, 255};
        if (player_.invulnerabilityTimer > 0.0F &&
            static_cast<int>(player_.invulnerabilityTimer * 16.0F) % 2 == 0)
        {
            modulation = SDL_Color{105, 229, 255, 148};
        }
        else if (player_.hitStunTimer > 0.0F)
        {
            modulation = SDL_Color{255, 113, 113, 255};
        }

        const float attackLean =
            player_.attackTimer > 0.0F
                ? std::sin(player_.attackTimer * 14.0F) * 2.5F
                : 0.0F;
        RenderTextureAtFeet(
            renderer_,
            *frame,
            x,
            y,
            height,
            player_.facingLeft,
            attackLean,
            modulation);

        if (player_.weapon != WeaponKind::None &&
            player_.pose != PlayerPose::Kick)
        {
            const float direction = player_.facingLeft ? -1.0F : 1.0F;
            const float handX = x + direction * 58.0F;
            const float handY = y - 245.0F;
            if (player_.weapon == WeaponKind::LeadPipe)
            {
                SetDrawColor(renderer_, SDL_Color{213, 227, 235, 255});
                for (int thickness = -4; thickness <= 4; ++thickness)
                {
                    SDL_RenderLine(
                        renderer_,
                        handX,
                        handY + static_cast<float>(thickness),
                        handX + direction * 122.0F,
                        handY - 52.0F + static_cast<float>(thickness));
                }
            }
            else if (player_.weapon == WeaponKind::Bottle)
            {
                FillRect(
                    renderer_,
                    SDL_FRect{
                        handX - 11.0F,
                        handY - 40.0F,
                        22.0F,
                        46.0F},
                    SDL_Color{54, 202, 157, 230});
                StrokeRect(
                    renderer_,
                    SDL_FRect{
                        handX - 11.0F,
                        handY - 40.0F,
                        22.0F,
                        46.0F},
                    SDL_Color{183, 255, 225, 255});
            }
            else
            {
                FillEllipse(
                    renderer_,
                    handX,
                    handY - 17.0F,
                    13.0F,
                    20.0F,
                    SDL_Color{243, 208, 80, 255});
            }
        }
    }

    void RenderEnemy(const Enemy& enemy, float camera, float offsetY)
    {
        const TextureAsset& texture =
            enemy.kind == EnemyKind::Antonio ? antonio_ :
            enemy.kind == EnemyKind::Electra ? electra_ :
            enemy.kind == EnemyKind::Shiva ? shiva_ : galsia_;
        const float baseHeight =
            enemy.kind == EnemyKind::Antonio ? 500.0F :
            enemy.kind == EnemyKind::Electra ? 410.0F :
            enemy.kind == EnemyKind::Shiva ? 420.0F : 405.0F;
        float height = baseHeight;
        float x = enemy.x - camera;
        float y = enemy.feetY + offsetY;
        double angle = 0.0;
        SDL_Color modulation{255, 255, 255, 255};

        if (enemy.kind == EnemyKind::Signal)
        {
            modulation =
                enemy.palette == 0
                    ? SDL_Color{104, 171, 255, 255}
                    : SDL_Color{198, 118, 255, 255};
        }
        else if (enemy.kind == EnemyKind::Galsia && enemy.palette == 1)
        {
            modulation = SDL_Color{255, 176, 126, 255};
        }
        else if (enemy.kind == EnemyKind::Electra && enemy.palette == 1)
        {
            modulation = SDL_Color{198, 165, 255, 255};
        }
        else if (enemy.kind == EnemyKind::Shiva && enemy.palette == 1)
        {
            modulation = SDL_Color{119, 202, 255, 255};
        }

        if (enemy.state == EnemyState::Hurt)
        {
            modulation = SDL_Color{255, 92, 104, 255};
            angle = enemy.knockbackVelocity > 0.0F ? 5.0 : -5.0;
        }
        else if (enemy.state == EnemyState::Attacking)
        {
            const float lunge =
                std::sin(std::clamp(
                    (0.58F - enemy.stateTimer) / 0.58F,
                    0.0F,
                    1.0F) * Pi);
            x += (enemy.facingLeft ? -1.0F : 1.0F) * lunge * 65.0F;
            angle = enemy.facingLeft ? -3.0 : 3.0;
        }
        else if (enemy.state == EnemyState::KnockedOut)
        {
            angle = enemy.knockbackVelocity >= 0.0F ? 78.0 : -78.0;
            height *= 0.92F;
            modulation.a = static_cast<Uint8>(
                255.0F * std::clamp(
                    enemy.stateTimer + 0.35F,
                    0.0F,
                    1.0F));
        }
        else
        {
            y += std::sin(enemy.bobPhase) * 7.0F;
            angle = std::sin(enemy.bobPhase * 0.5F) * 1.3;
        }

        if (!texture.IsValid())
        {
            FillRect(
                renderer_,
                SDL_FRect{x - 42.0F, y - 230.0F, 84.0F, 230.0F},
                enemy.kind == EnemyKind::Antonio
                    ? SDL_Color{68, 76, 90, 255}
                    : SDL_Color{193, 42, 60, 255});
            return;
        }

        RenderTextureAtFeet(
            renderer_,
            texture,
            x,
            y,
            height,
            !enemy.facingLeft,
            angle,
            modulation);
    }

    void RenderBoomerang(float camera, float offsetY)
    {
        if (!boomerang_.active)
        {
            return;
        }

        const float progress = boomerang_.time / 1.42F;
        const float x = boomerang_.originX +
            boomerang_.direction * std::sin(progress * Pi) * 760.0F - camera;
        const float y = boomerang_.originY +
            std::sin(progress * Pi * 2.0F) * 42.0F + offsetY;
        const float angle = boomerang_.time * 18.0F;

        for (int index = 0; index < 3; ++index)
        {
            const float trailX =
                x - boomerang_.direction * (18.0F + index * 19.0F);
            FillEllipse(
                renderer_,
                trailX,
                y,
                22.0F - index * 4.0F,
                8.0F - index * 1.5F,
                SDL_Color{
                    93,
                    221,
                    255,
                    static_cast<Uint8>(85 - index * 22)});
        }

        SetDrawColor(renderer_, SDL_Color{226, 241, 248, 255});
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const std::array<SDL_FPoint, 4> points = {
            SDL_FPoint{-34.0F, -20.0F},
            SDL_FPoint{0.0F, 0.0F},
            SDL_FPoint{-34.0F, 20.0F},
            SDL_FPoint{17.0F, 0.0F}};
        for (std::size_t index = 0; index + 1 < points.size(); ++index)
        {
            const SDL_FPoint start{
                x + points[index].x * cosine - points[index].y * sine,
                y + points[index].x * sine + points[index].y * cosine};
            const SDL_FPoint end{
                x + points[index + 1].x * cosine - points[index + 1].y * sine,
                y + points[index + 1].x * sine + points[index + 1].y * cosine};
            for (int thickness = -3; thickness <= 3; ++thickness)
            {
                SDL_RenderLine(
                    renderer_,
                    start.x,
                    start.y + static_cast<float>(thickness),
                    end.x,
                    end.y + static_cast<float>(thickness));
            }
        }
    }

    void RenderParticles(float camera, float offsetY)
    {
        for (const auto& particle : particles_)
        {
            const float alpha =
                std::clamp(
                    particle.life / particle.maximumLife,
                    0.0F,
                    1.0F);
            SDL_Color color = particle.color;
            color.a = static_cast<Uint8>(
                static_cast<float>(color.a) * alpha);
            const float size = particle.size * (0.6F + alpha * 0.7F);
            FillRect(
                renderer_,
                SDL_FRect{
                    particle.x - camera - size * 0.5F,
                    particle.y + offsetY - size * 0.5F,
                    size,
                    size},
                color);
        }
    }

    void RenderHud()
    {
        FillRect(
            renderer_,
            SDL_FRect{0.0F, 0.0F, 1920.0F, 144.0F},
            SDL_Color{2, 7, 15, 224});
        FillRect(
            renderer_,
            SDL_FRect{0.0F, 140.0F, 1920.0F, 4.0F},
            SDL_Color{56, 224, 255, 220});

        const TextureAsset* portrait = axelIdle_.Frame(0.0F);
        if (portrait != nullptr)
        {
            RenderTextureAtFeet(
                renderer_,
                *portrait,
                82.0F,
                135.0F,
                126.0F,
                false);
        }

        DrawText(
            renderer_,
            "AXEL",
            160.0F,
            22.0F,
            3.5F,
            SDL_Color{246, 241, 220, 255});
        DrawText(
            renderer_,
            "SCORE " + PaddedScore(player_.score),
            160.0F,
            62.0F,
            2.1F,
            SDL_Color{105, 222, 255, 255});
        DrawText(
            renderer_,
            "LIVES x" + std::to_string(player_.lives + 1),
            160.0F,
            95.0F,
            1.8F,
            SDL_Color{255, 109, 193, 255});

        const SDL_FRect healthBack{500.0F, 36.0F, 560.0F, 34.0F};
        FillRect(renderer_, healthBack, SDL_Color{18, 28, 40, 255});
        FillRect(
            renderer_,
            SDL_FRect{
                506.0F,
                42.0F,
                548.0F * static_cast<float>(player_.health) / 100.0F,
                22.0F},
            player_.health > 30
                ? SDL_Color{55, 224, 124, 255}
                : SDL_Color{255, 70, 70, 255});
        StrokeRect(
            renderer_,
            healthBack,
            SDL_Color{190, 230, 240, 255});
        DrawText(
            renderer_,
            "ENERGY",
            500.0F,
            82.0F,
            1.75F,
            SDL_Color{189, 207, 216, 255});

        DrawText(
            renderer_,
            "SPECIAL",
            1145.0F,
            28.0F,
            1.6F,
            SDL_Color{189, 207, 216, 255});
        for (int index = 0; index < std::max(1, player_.specials); ++index)
        {
            const bool available = index < player_.specials;
            FillRect(
                renderer_,
                SDL_FRect{
                    1150.0F + static_cast<float>(index) * 42.0F,
                    61.0F,
                    30.0F,
                    30.0F},
                available
                    ? SDL_Color{255, 74, 196, 255}
                    : SDL_Color{50, 55, 65, 255});
            FillRect(
                renderer_,
                SDL_FRect{
                    1159.0F + static_cast<float>(index) * 42.0F,
                    52.0F,
                    12.0F,
                    48.0F},
                available
                    ? SDL_Color{83, 226, 255, 235}
                    : SDL_Color{50, 55, 65, 255});
        }

        if (player_.weapon != WeaponKind::None)
        {
            const std::string weaponName =
                player_.weapon == WeaponKind::LeadPipe ? "LEAD PIPE" :
                player_.weapon == WeaponKind::Bottle ? "BOTTLE" :
                "PEPPER";
            DrawText(
                renderer_,
                "WEAPON " + weaponName,
                1145.0F,
                105.0F,
                1.25F,
                SDL_Color{255, 220, 104, 255});
        }

        DrawText(
            renderer_,
            "ROUND 1",
            1500.0F,
            22.0F,
            3.0F,
            SDL_Color{255, 231, 119, 255});
        DrawText(
            renderer_,
            "CITY STREET",
            1500.0F,
            68.0F,
            1.8F,
            SDL_Color{255, 106, 197, 255});
        DrawText(
            renderer_,
            "TIME " + std::to_string(
                static_cast<int>(std::ceil(roundTimeRemaining_))),
            1330.0F,
            105.0F,
            1.5F,
            roundTimeRemaining_ < 30.0F
                ? SDL_Color{255, 78, 89, 255}
                : SDL_Color{202, 223, 230, 255});

        const Enemy* boss = nullptr;
        for (const auto& enemy : enemies_)
        {
            if (enemy.visible &&
                enemy.kind == EnemyKind::Antonio &&
                enemy.state != EnemyState::KnockedOut)
            {
                boss = &enemy;
                break;
            }
        }
        if (boss != nullptr)
        {
            DrawTextCentered(
                renderer_,
                "ANTONIO",
                960.0F,
                94.0F,
                1.6F,
                SDL_Color{255, 193, 85, 255});
            FillRect(
                renderer_,
                SDL_FRect{680.0F, 120.0F, 560.0F, 15.0F},
                SDL_Color{46, 20, 28, 255});
            FillRect(
                renderer_,
                SDL_FRect{
                    684.0F,
                    124.0F,
                    552.0F *
                        static_cast<float>(boss->health) /
                        static_cast<float>(boss->maximumHealth),
                    7.0F},
                SDL_Color{255, 74, 82, 255});
        }

        DrawText(
            renderer_,
            "MOVE WASD/ARROWS   COMBO Z/J/SPACE   KICK X/K   JUMP V/I   SPECIAL C/L",
            24.0F,
            1043.0F,
            1.4F,
            SDL_Color{199, 220, 226, 210});
    }

    void RenderOverlays()
    {
        if (introTimer_ > 0.0F)
        {
            const float reveal =
                1.0F - std::clamp((introTimer_ - 2.8F) / 0.8F, 0.0F, 1.0F);
            const float fade =
                std::clamp(introTimer_ / 0.8F, 0.0F, 1.0F);
            const Uint8 alpha =
                static_cast<Uint8>(255.0F * reveal * fade);
            FillRect(
                renderer_,
                SDL_FRect{0.0F, 405.0F, 1920.0F, 250.0F},
                SDL_Color{2, 7, 16, static_cast<Uint8>(210.0F * reveal * fade)});
            FillRect(
                renderer_,
                SDL_FRect{0.0F, 404.0F, 1920.0F * reveal, 4.0F},
                SDL_Color{55, 225, 255, alpha});
            FillRect(
                renderer_,
                SDL_FRect{
                    1920.0F * (1.0F - reveal),
                    651.0F,
                    1920.0F * reveal,
                    4.0F},
                SDL_Color{255, 76, 200, alpha});
            DrawTextCentered(
                renderer_,
                "ROUND 1",
                960.0F,
                450.0F,
                6.0F,
                SDL_Color{255, 237, 155, alpha});
            DrawTextCentered(
                renderer_,
                "CITY STREET // 1993 REWIRED",
                960.0F,
                548.0F,
                2.5F,
                SDL_Color{88, 224, 255, alpha});
        }

        if (waveBannerTimer_ > 0.0F && currentWave_ < waves_.size())
        {
            const float fade =
                std::min(1.0F, waveBannerTimer_ * 2.0F);
            DrawTextCentered(
                renderer_,
                waves_[currentWave_].title,
                960.0F,
                184.0F,
                2.2F,
                SDL_Color{
                    255,
                    211,
                    103,
                    static_cast<Uint8>(255.0F * fade)});
        }

        if (waveActive_)
        {
            const float pulse =
                0.5F + 0.5F * std::sin(animationClock_ * 7.0F);
            DrawTextCentered(
                renderer_,
                "GO!",
                1740.0F,
                514.0F,
                3.7F,
                SDL_Color{
                    255,
                    225,
                    88,
                    static_cast<Uint8>(150.0F + pulse * 105.0F)});
        }

        if (mode_ == GameMode::Paused)
        {
            FillRect(
                renderer_,
                SDL_FRect{0.0F, 0.0F, 1920.0F, 1080.0F},
                SDL_Color{0, 4, 11, 185});
            DrawTextCentered(
                renderer_,
                "PAUSED",
                960.0F,
                470.0F,
                6.0F,
                SDL_Color{96, 226, 255, 255});
            DrawTextCentered(
                renderer_,
                "PRESS P TO CONTINUE",
                960.0F,
                570.0F,
                2.2F,
                SDL_Color{240, 240, 228, 255});
        }
        else if (mode_ == GameMode::GameOver)
        {
            FillRect(
                renderer_,
                SDL_FRect{0.0F, 0.0F, 1920.0F, 1080.0F},
                SDL_Color{17, 0, 8, 198});
            DrawTextCentered(
                renderer_,
                "GAME OVER",
                960.0F,
                435.0F,
                7.0F,
                SDL_Color{255, 61, 103, 255});
            DrawTextCentered(
                renderer_,
                "PRESS ENTER TO TRY AGAIN",
                960.0F,
                570.0F,
                2.4F,
                SDL_Color{255, 225, 176, 255});
        }
        else if (mode_ == GameMode::RoundClear)
        {
            FillRect(
                renderer_,
                SDL_FRect{0.0F, 0.0F, 1920.0F, 1080.0F},
                SDL_Color{0, 10, 20, 182});
            DrawTextCentered(
                renderer_,
                "ROUND 1 CLEAR",
                960.0F,
                395.0F,
                6.4F,
                SDL_Color{255, 230, 94, 255});
            DrawTextCentered(
                renderer_,
                "STREET SECURED",
                960.0F,
                505.0F,
                2.8F,
                SDL_Color{82, 228, 255, 255});
            DrawTextCentered(
                renderer_,
                "SCORE " + PaddedScore(player_.score),
                960.0F,
                590.0F,
                2.5F,
                SDL_Color{255, 109, 200, 255});
            DrawTextCentered(
                renderer_,
                "CLEAR 20000  TIME x100  SPECIAL x5000",
                960.0F,
                644.0F,
                1.5F,
                SDL_Color{187, 220, 230, 255});
            DrawTextCentered(
                renderer_,
                "PRESS ENTER TO REPLAY",
                960.0F,
                710.0F,
                2.0F,
                SDL_Color{235, 239, 232, 255});
        }

        if (screenFlash_ > 0.0F)
        {
            const float normalized =
                std::clamp(screenFlash_ / 0.48F, 0.0F, 1.0F);
            FillRect(
                renderer_,
                SDL_FRect{0.0F, 0.0F, 1920.0F, 1080.0F},
                SDL_Color{
                    167,
                    241,
                    255,
                    static_cast<Uint8>(210.0F * normalized)});
            DrawTextCentered(
                renderer_,
                "STREET SWEEPER",
                960.0F,
                310.0F,
                3.0F,
                SDL_Color{
                    8,
                    29,
                    55,
                    static_cast<Uint8>(255.0F * normalized)});
        }
    }

    SDL_Renderer* renderer_ = nullptr;
    TextureAsset background_;
    Animation axelIdle_;
    Animation axelWalk_;
    TextureAsset axelPunch_;
    TextureAsset axelKick_;
    TextureAsset galsia_;
    TextureAsset electra_;
    TextureAsset shiva_;
    TextureAsset antonio_;

    Player player_;
    std::vector<Enemy> enemies_;
    std::vector<Wave> waves_;
    std::vector<SpawnRequest> pendingSpawns_;
    std::vector<Pickup> pickups_;
    std::vector<StreetContainer> containers_;
    std::vector<Particle> particles_;
    Boomerang boomerang_;
    std::size_t currentWave_ = 0;
    GameMode mode_ = GameMode::Playing;
    bool waveActive_ = false;
    bool qaFreezeWaves_ = false;
    float cameraX_ = 0.0F;
    float cameraLockX_ = 0.0F;
    float animationClock_ = 0.0F;
    float introTimer_ = 0.0F;
    float roundTimeRemaining_ = 180.0F;
    float waveBannerTimer_ = 0.0F;
    float roundClearTimer_ = 0.0F;
    float screenFlash_ = 0.0F;
    float screenShake_ = 0.0F;
};
}

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    SDL_Window* rawWindow = nullptr;
    SDL_Renderer* rawRenderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(
            "Streets Enhanced // Round 1",
            WindowWidth,
            WindowHeight,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
            &rawWindow,
            &rawRenderer))
    {
        std::cerr << "SDL_CreateWindowAndRenderer failed: "
                  << SDL_GetError() << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    WindowPtr window{rawWindow};
    RendererPtr renderer{rawRenderer};
    SDL_SetRenderLogicalPresentation(
        renderer.get(),
        LogicalWidth,
        LogicalHeight,
        SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    {
        RoundOneDemo demo{renderer.get()};
        Uint64 previousTick = SDL_GetTicks();
        bool running = true;

        while (running)
        {
            SDL_Event event{};
            while (SDL_PollEvent(&event))
            {
                running = demo.HandleEvent(event) && running;
            }

            const Uint64 now = SDL_GetTicks();
            const float deltaSeconds = std::min(
                static_cast<float>(now - previousTick) / 1000.0F,
                0.05F);
            previousTick = now;

            demo.Update(deltaSeconds);
            demo.Render();
            SDL_RenderPresent(renderer.get());
        }
    }

    renderer.reset();
    window.reset();
    SDL_Quit();
    return EXIT_SUCCESS;
}
