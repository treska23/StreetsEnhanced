#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
constexpr int LogicalWidth = 1920;
constexpr int LogicalHeight = 1080;
constexpr int WindowWidth = 1920;
constexpr int WindowHeight = 1080;

constexpr std::size_t AxelIdleUniqueFrameCount = 3;
constexpr std::size_t AxelWalkFrameCount = 4;
constexpr Uint64 AxelIdleFrameDurationMs = 160;
constexpr Uint64 AxelWalkFrameDurationMs = 110;

constexpr float AxelDefaultFrameHeight = 440.0F;
constexpr float AxelMinimumFrameHeight = 220.0F;
constexpr float AxelMaximumFrameHeight = 720.0F;
constexpr float AxelScaleStep = 20.0F;

constexpr float AxelHorizontalSpeed = 430.0F;
constexpr float AxelVerticalSpeed = 280.0F;
constexpr float AxelMinimumFeetY = 760.0F;
constexpr float AxelMaximumFeetY = 1015.0F;
constexpr float CameraLeftMargin = 620.0F;
constexpr float CameraRightMargin = 1300.0F;

const std::filesystem::path AssetsRoot =
    std::filesystem::path{STREETS_SOURCE_DIR} /
    "LocalAssets" /
    "ReferenceSheets" /
    "Stages";

const std::filesystem::path Round1MapPath = AssetsRoot / "Round1.png";
const std::filesystem::path AxelFolderPath = AssetsRoot / "Axel Stone";
const std::filesystem::path AxelIdleFramesPath = AxelFolderPath / "Idle";
const std::filesystem::path AxelWalkFramesPath = AxelFolderPath / "Walk";

struct WindowDeleter
{
    void operator()(SDL_Window* value) const noexcept { SDL_DestroyWindow(value); }
};

struct RendererDeleter
{
    void operator()(SDL_Renderer* value) const noexcept { SDL_DestroyRenderer(value); }
};

struct TextureDeleter
{
    void operator()(SDL_Texture* value) const noexcept { SDL_DestroyTexture(value); }
};

using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, TextureDeleter>;

struct AnimationFrame
{
    std::filesystem::path path;
    TexturePtr texture;
    float width = 0.0F;
    float height = 0.0F;
};

struct Animation
{
    std::vector<AnimationFrame> frames;
    std::vector<std::size_t> playbackOrder;
    float referenceHeight = 0.0F;
};

enum class PlayerAnimationState
{
    Idle,
    Walk
};

TexturePtr LoadTexture(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    SDL_ScaleMode scaleMode)
{
    SDL_Texture* rawTexture = IMG_LoadTexture(renderer, path.string().c_str());
    if (rawTexture == nullptr)
    {
        std::cerr << "No se pudo cargar: " << path << '\n'
                  << "SDL_image: " << SDL_GetError() << '\n';
        return {};
    }

    if (!SDL_SetTextureScaleMode(rawTexture, scaleMode))
    {
        std::cerr << "No se pudo configurar el filtrado: "
                  << SDL_GetError() << '\n';
        SDL_DestroyTexture(rawTexture);
        return {};
    }

    SDL_SetTextureBlendMode(rawTexture, SDL_BLENDMODE_BLEND);
    return TexturePtr{rawTexture};
}

std::string ToLower(std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
    return text;
}

bool IsPng(const std::filesystem::path& path)
{
    return ToLower(path.extension().string()) == ".png";
}

int ExtractFrameNumber(const std::filesystem::path& path)
{
    const std::string stem = path.stem().string();
    int result = std::numeric_limits<int>::max();

    for (std::size_t index = 0; index < stem.size();)
    {
        if (!std::isdigit(static_cast<unsigned char>(stem[index])))
        {
            ++index;
            continue;
        }

        int value = 0;
        while (index < stem.size() &&
               std::isdigit(static_cast<unsigned char>(stem[index])))
        {
            value = (value * 10) + (stem[index] - '0');
            ++index;
        }
        result = value;
    }

    return result;
}

void SortFrameFiles(std::vector<std::filesystem::path>& files)
{
    std::sort(
        files.begin(),
        files.end(),
        [](const auto& left, const auto& right)
        {
            const int leftNumber = ExtractFrameNumber(left);
            const int rightNumber = ExtractFrameNumber(right);
            if (leftNumber != rightNumber)
            {
                return leftNumber < rightNumber;
            }
            return ToLower(left.filename().string()) <
                   ToLower(right.filename().string());
        });
}

std::vector<std::filesystem::path> FindAnimationFrameFiles(
    const std::filesystem::path& stateFolder,
    const std::string& stateName,
    std::size_t maximumFrames)
{
    std::vector<std::filesystem::path> files;

    if (std::filesystem::is_directory(stateFolder))
    {
        for (const auto& entry : std::filesystem::directory_iterator(stateFolder))
        {
            if (entry.is_regular_file() && IsPng(entry.path()))
            {
                files.push_back(entry.path());
            }
        }
    }

    // Compatibilidad por si los PNG se guardaron directamente en "Axel Stone".
    if (files.empty() && std::filesystem::is_directory(AxelFolderPath))
    {
        const std::string lowerStateName = ToLower(stateName);
        for (const auto& entry : std::filesystem::directory_iterator(AxelFolderPath))
        {
            if (!entry.is_regular_file() || !IsPng(entry.path()))
            {
                continue;
            }

            const std::string name = ToLower(entry.path().stem().string());
            if (name.find(lowerStateName) != std::string::npos)
            {
                files.push_back(entry.path());
            }
        }
    }

    SortFrameFiles(files);

    if (files.size() > maximumFrames)
    {
        std::cout << stateName << ": encontrados " << files.size()
                  << " PNG; se usaran los " << maximumFrames
                  << " primeros.\n";
        files.resize(maximumFrames);
    }

    return files;
}

Animation LoadAnimation(
    SDL_Renderer* renderer,
    const std::filesystem::path& stateFolder,
    const std::string& stateName,
    std::size_t maximumFrames,
    bool pingPong)
{
    Animation animation;
    const auto files =
        FindAnimationFrameFiles(stateFolder, stateName, maximumFrames);

    if (files.empty())
    {
        std::cerr << "No se encontraron PNG para " << stateName << ".\n"
                  << "Carpeta esperada: " << stateFolder << '\n';
        return animation;
    }

    float firstWidth = 0.0F;
    float firstHeight = 0.0F;
    bool dimensionsDiffer = false;

    for (const auto& path : files)
    {
        TexturePtr texture = LoadTexture(renderer, path, SDL_SCALEMODE_LINEAR);
        if (!texture)
        {
            continue;
        }

        float width = 0.0F;
        float height = 0.0F;
        SDL_GetTextureSize(texture.get(), &width, &height);

        if (animation.frames.empty())
        {
            firstWidth = width;
            firstHeight = height;
        }
        else if (width != firstWidth || height != firstHeight)
        {
            dimensionsDiffer = true;
        }

        animation.referenceHeight = std::max(animation.referenceHeight, height);
        animation.frames.push_back(
            AnimationFrame{path, std::move(texture), width, height});
    }

    for (std::size_t index = 0; index < animation.frames.size(); ++index)
    {
        animation.playbackOrder.push_back(index);
    }

    if (pingPong && animation.frames.size() > 2)
    {
        for (std::size_t index = animation.frames.size() - 2; index > 0; --index)
        {
            animation.playbackOrder.push_back(index);
        }
    }

    std::cout << stateName << ": " << animation.frames.size()
              << " frames cargados.\n";
    for (std::size_t index = 0; index < animation.frames.size(); ++index)
    {
        const auto& frame = animation.frames[index];
        std::cout << "  [" << index << "] "
                  << frame.path.filename().string() << " ("
                  << frame.width << "x" << frame.height << ")\n";
    }

    if (dimensionsDiffer)
    {
        std::cout
            << "AVISO: los PNG de " << stateName
            << " no tienen todos el mismo tamano de lienzo. "
            << "Se alinearan por el centro inferior.\n";
    }

    return animation;
}

float GetBackgroundWorldWidth(SDL_Texture* texture)
{
    if (texture == nullptr)
    {
        return static_cast<float>(LogicalWidth);
    }

    float textureWidth = 0.0F;
    float textureHeight = 0.0F;
    SDL_GetTextureSize(texture, &textureWidth, &textureHeight);

    if (textureHeight <= 0.0F)
    {
        return static_cast<float>(LogicalWidth);
    }

    const float scale = static_cast<float>(LogicalHeight) / textureHeight;
    return std::max(static_cast<float>(LogicalWidth), textureWidth * scale);
}

void RenderBackground(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    float& cameraX)
{
    float textureWidth = 0.0F;
    float textureHeight = 0.0F;
    SDL_GetTextureSize(texture, &textureWidth, &textureHeight);

    const float viewportAspect =
        static_cast<float>(LogicalWidth) / static_cast<float>(LogicalHeight);
    const float textureAspect = textureWidth / textureHeight;

    SDL_FRect source{};
    if (textureAspect >= viewportAspect)
    {
        const float textureToWorldScale =
            static_cast<float>(LogicalHeight) / textureHeight;
        const float worldWidth = textureWidth * textureToWorldScale;
        const float maximumCameraX =
            std::max(0.0F, worldWidth - static_cast<float>(LogicalWidth));

        cameraX = std::clamp(cameraX, 0.0F, maximumCameraX);
        source.x = cameraX / textureToWorldScale;
        source.y = 0.0F;
        source.w = static_cast<float>(LogicalWidth) / textureToWorldScale;
        source.h = textureHeight;
    }
    else
    {
        source.w = textureWidth;
        source.h = textureWidth / viewportAspect;
        source.x = 0.0F;
        source.y = std::max(0.0F, (textureHeight - source.h) * 0.5F);
        cameraX = 0.0F;
    }

    const SDL_FRect destination{
        0.0F,
        0.0F,
        static_cast<float>(LogicalWidth),
        static_cast<float>(LogicalHeight)};

    SDL_RenderTexture(renderer, texture, &source, &destination);
}

void RenderAxel(
    SDL_Renderer* renderer,
    const Animation& animation,
    std::size_t frameIndex,
    float displayedReferenceHeight,
    float screenCenterX,
    float feetY,
    bool facingLeft)
{
    if (animation.frames.empty() || animation.referenceHeight <= 0.0F)
    {
        return;
    }

    frameIndex %= animation.frames.size();
    const auto& frame = animation.frames[frameIndex];

    const float scale = displayedReferenceHeight / animation.referenceHeight;
    const float displayedWidth = frame.width * scale;
    const float displayedHeight = frame.height * scale;

    const SDL_FRect destination{
        screenCenterX - (displayedWidth * 0.5F),
        feetY - displayedHeight,
        displayedWidth,
        displayedHeight};

    SDL_RenderTextureRotated(
        renderer,
        frame.texture.get(),
        nullptr,
        &destination,
        0.0,
        nullptr,
        facingLeft ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
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
            "Streets Enhanced - Axel Walk Test",
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

    if (!SDL_SetRenderLogicalPresentation(
            renderer.get(),
            LogicalWidth,
            LogicalHeight,
            SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        std::cerr << "SDL_SetRenderLogicalPresentation failed: "
                  << SDL_GetError() << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);

    TexturePtr round1Map =
        LoadTexture(renderer.get(), Round1MapPath, SDL_SCALEMODE_LINEAR);
    Animation axelIdle = LoadAnimation(
        renderer.get(),
        AxelIdleFramesPath,
        "Idle",
        AxelIdleUniqueFrameCount,
        true);
    Animation axelWalk = LoadAnimation(
        renderer.get(),
        AxelWalkFramesPath,
        "Walk",
        AxelWalkFrameCount,
        false);

    float cameraX = 0.0F;
    float axelFrameHeight = AxelDefaultFrameHeight;
    float playerWorldX = 960.0F;
    float playerFeetY = AxelMaximumFeetY;
    bool facingLeft = false;
    bool animationEnabled = true;

    PlayerAnimationState animationState = PlayerAnimationState::Idle;
    std::size_t playbackStep = 0;
    std::size_t currentFrame = 0;
    Uint64 lastFrameChangeAt = SDL_GetTicks();
    Uint64 previousTick = SDL_GetTicks();

    std::cout
        << "Controles:\n"
        << "  Flechas o WASD: andar\n"
        << "  + / -: cambiar tamano de Axel\n"
        << "  Espacio: pausar/reanudar la animacion\n"
        << "  Inicio: volver a la posicion inicial\n"
        << "  R: recargar fondo, Idle y Walk\n"
        << "  Esc: salir\n"
        << "Carpeta Walk: " << AxelWalkFramesPath << '\n'
        << "Se usan como maximo los cuatro primeros PNG de Walk.\n";

    bool running = true;
    while (running)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
                continue;
            }

            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
            {
                continue;
            }

            switch (event.key.key)
            {
            case SDLK_ESCAPE:
                running = false;
                break;
            case SDLK_HOME:
                cameraX = 0.0F;
                playerWorldX = 960.0F;
                playerFeetY = AxelMaximumFeetY;
                facingLeft = false;
                break;
            case SDLK_SPACE:
                animationEnabled = !animationEnabled;
                playbackStep = 0;
                currentFrame = 0;
                lastFrameChangeAt = SDL_GetTicks();
                break;
            case SDLK_EQUALS:
            case SDLK_KP_PLUS:
                axelFrameHeight = std::min(
                    AxelMaximumFrameHeight,
                    axelFrameHeight + AxelScaleStep);
                std::cout << "Altura de Axel: " << axelFrameHeight << " px\n";
                break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
                axelFrameHeight = std::max(
                    AxelMinimumFrameHeight,
                    axelFrameHeight - AxelScaleStep);
                std::cout << "Altura de Axel: " << axelFrameHeight << " px\n";
                break;
            case SDLK_R:
                round1Map =
                    LoadTexture(renderer.get(), Round1MapPath, SDL_SCALEMODE_LINEAR);
                axelIdle = LoadAnimation(
                    renderer.get(),
                    AxelIdleFramesPath,
                    "Idle",
                    AxelIdleUniqueFrameCount,
                    true);
                axelWalk = LoadAnimation(
                    renderer.get(),
                    AxelWalkFramesPath,
                    "Walk",
                    AxelWalkFrameCount,
                    false);
                playbackStep = 0;
                currentFrame = 0;
                lastFrameChangeAt = SDL_GetTicks();
                break;
            default:
                break;
            }
        }

        const Uint64 now = SDL_GetTicks();
        const float deltaSeconds = std::min(
            static_cast<float>(now - previousTick) / 1000.0F,
            0.05F);
        previousTick = now;

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        float horizontalDirection = 0.0F;
        float verticalDirection = 0.0F;

        if (keyboard[SDL_SCANCODE_LEFT] || keyboard[SDL_SCANCODE_A])
        {
            horizontalDirection -= 1.0F;
        }
        if (keyboard[SDL_SCANCODE_RIGHT] || keyboard[SDL_SCANCODE_D])
        {
            horizontalDirection += 1.0F;
        }
        if (keyboard[SDL_SCANCODE_UP] || keyboard[SDL_SCANCODE_W])
        {
            verticalDirection -= 1.0F;
        }
        if (keyboard[SDL_SCANCODE_DOWN] || keyboard[SDL_SCANCODE_S])
        {
            verticalDirection += 1.0F;
        }

        const bool isMoving =
            horizontalDirection != 0.0F || verticalDirection != 0.0F;

        if (isMoving)
        {
            const float length = std::sqrt(
                (horizontalDirection * horizontalDirection) +
                (verticalDirection * verticalDirection));
            horizontalDirection /= length;
            verticalDirection /= length;

            playerWorldX +=
                horizontalDirection * AxelHorizontalSpeed * deltaSeconds;
            playerFeetY +=
                verticalDirection * AxelVerticalSpeed * deltaSeconds;

            if (horizontalDirection < 0.0F)
            {
                facingLeft = true;
            }
            else if (horizontalDirection > 0.0F)
            {
                facingLeft = false;
            }
        }

        const float worldWidth = GetBackgroundWorldWidth(round1Map.get());
        playerWorldX = std::clamp(playerWorldX, 120.0F, worldWidth - 120.0F);
        playerFeetY = std::clamp(
            playerFeetY,
            AxelMinimumFeetY,
            AxelMaximumFeetY);

        float playerScreenX = playerWorldX - cameraX;
        if (playerScreenX > CameraRightMargin)
        {
            cameraX = playerWorldX - CameraRightMargin;
        }
        else if (playerScreenX < CameraLeftMargin)
        {
            cameraX = playerWorldX - CameraLeftMargin;
        }

        const float maximumCameraX =
            std::max(0.0F, worldWidth - static_cast<float>(LogicalWidth));
        cameraX = std::clamp(cameraX, 0.0F, maximumCameraX);
        playerScreenX = playerWorldX - cameraX;

        const PlayerAnimationState requestedState =
            isMoving && !axelWalk.frames.empty()
                ? PlayerAnimationState::Walk
                : PlayerAnimationState::Idle;

        if (requestedState != animationState)
        {
            animationState = requestedState;
            playbackStep = 0;
            currentFrame = 0;
            lastFrameChangeAt = now;
        }

        const Animation& activeAnimation =
            animationState == PlayerAnimationState::Walk
                ? axelWalk
                : axelIdle;
        const Uint64 activeFrameDuration =
            animationState == PlayerAnimationState::Walk
                ? AxelWalkFrameDurationMs
                : AxelIdleFrameDurationMs;

        if (animationEnabled && !activeAnimation.playbackOrder.empty())
        {
            const Uint64 elapsed = now - lastFrameChangeAt;
            if (elapsed >= activeFrameDuration)
            {
                const Uint64 advance = elapsed / activeFrameDuration;
                playbackStep =
                    (playbackStep + static_cast<std::size_t>(advance)) %
                    activeAnimation.playbackOrder.size();
                currentFrame = activeAnimation.playbackOrder[playbackStep];
                lastFrameChangeAt += advance * activeFrameDuration;
            }
        }

        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());

        if (round1Map)
        {
            RenderBackground(renderer.get(), round1Map.get(), cameraX);
        }

        RenderAxel(
            renderer.get(),
            activeAnimation,
            currentFrame,
            axelFrameHeight,
            playerScreenX,
            playerFeetY,
            facingLeft);

        SDL_RenderPresent(renderer.get());
    }

    axelWalk.frames.clear();
    axelIdle.frames.clear();
    round1Map.reset();
    renderer.reset();
    window.reset();
    SDL_Quit();
    return EXIT_SUCCESS;
}
