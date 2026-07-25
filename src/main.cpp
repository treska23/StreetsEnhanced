#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cctype>
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
constexpr float CameraStep = 64.0F;

constexpr std::size_t AxelIdleUniqueFrameCount = 3;
constexpr Uint64 AxelIdleFrameDurationMs = 160;
constexpr float AxelDefaultFrameHeight = 440.0F;
constexpr float AxelMinimumFrameHeight = 220.0F;
constexpr float AxelMaximumFrameHeight = 720.0F;
constexpr float AxelScaleStep = 20.0F;
constexpr float AxelFeetY = 1015.0F;

const std::filesystem::path AssetsRoot =
    std::filesystem::path{STREETS_SOURCE_DIR} /
    "LocalAssets" /
    "ReferenceSheets" /
    "Stages";

const std::filesystem::path Round1MapPath = AssetsRoot / "Round1.png";
const std::filesystem::path AxelFolderPath = AssetsRoot / "Axel Stone";
const std::filesystem::path AxelIdleFramesPath = AxelFolderPath / "Idle";

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

std::vector<std::filesystem::path> FindIdleFrameFiles()
{
    std::vector<std::filesystem::path> files;

    if (std::filesystem::is_directory(AxelIdleFramesPath))
    {
        for (const auto& entry : std::filesystem::directory_iterator(AxelIdleFramesPath))
        {
            if (entry.is_regular_file() && IsPng(entry.path()))
            {
                files.push_back(entry.path());
            }
        }
    }

    if (files.empty() && std::filesystem::is_directory(AxelFolderPath))
    {
        for (const auto& entry : std::filesystem::directory_iterator(AxelFolderPath))
        {
            if (!entry.is_regular_file() || !IsPng(entry.path()))
            {
                continue;
            }

            const std::string name = ToLower(entry.path().stem().string());
            if (name.find("idle") != std::string::npos &&
                name != "axel stone_idle")
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
            const int leftNumber = ExtractFrameNumber(left);
            const int rightNumber = ExtractFrameNumber(right);
            if (leftNumber != rightNumber)
            {
                return leftNumber < rightNumber;
            }
            return ToLower(left.filename().string()) <
                   ToLower(right.filename().string());
        });

    if (files.size() > AxelIdleUniqueFrameCount)
    {
        std::cout << "Se han encontrado " << files.size()
                  << " PNG, pero el Idle usara solo los tres primeros.\n";
        files.resize(AxelIdleUniqueFrameCount);
    }

    return files;
}

Animation LoadIdleAnimation(SDL_Renderer* renderer)
{
    Animation animation;
    const auto files = FindIdleFrameFiles();

    if (files.empty())
    {
        std::cerr
            << "No se encontraron PNG separados para el Idle.\n"
            << "Carpeta esperada: " << AxelIdleFramesPath << '\n';
        return animation;
    }

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
        animation.referenceHeight = std::max(animation.referenceHeight, height);
        animation.frames.push_back(
            AnimationFrame{path, std::move(texture), width, height});
    }

    if (animation.frames.size() >= 3)
    {
        animation.playbackOrder = {0, 1, 2, 1};
    }
    else if (animation.frames.size() == 2)
    {
        animation.playbackOrder = {0, 1};
    }
    else if (animation.frames.size() == 1)
    {
        animation.playbackOrder = {0};
    }

    std::cout << "Frames Idle usados: " << animation.frames.size() << '\n';
    for (std::size_t index = 0; index < animation.frames.size(); ++index)
    {
        const auto& frame = animation.frames[index];
        std::cout << "  [" << index << "] "
                  << frame.path.filename().string() << " ("
                  << frame.width << "x" << frame.height << ")\n";
    }
    std::cout << "Secuencia: 1 -> 2 -> 3 -> 2\n";

    return animation;
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
        source.w = textureHeight * viewportAspect;
        source.h = textureHeight;
        cameraX = std::clamp(
            cameraX,
            0.0F,
            std::max(0.0F, textureWidth - source.w));
        source.x = cameraX;
        source.y = 0.0F;
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

void RenderAxelIdle(
    SDL_Renderer* renderer,
    const Animation& animation,
    std::size_t frameIndex,
    float displayedReferenceHeight)
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
        (static_cast<float>(LogicalWidth) - displayedWidth) * 0.5F,
        AxelFeetY - displayedHeight,
        displayedWidth,
        displayedHeight};

    SDL_RenderTexture(renderer, frame.texture.get(), nullptr, &destination);
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
            "Streets Enhanced - Axel Idle",
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

    float cameraX = 0.0F;
    float axelFrameHeight = AxelDefaultFrameHeight;
    bool animationEnabled = true;
    std::size_t playbackStep = 0;
    std::size_t currentFrame = 0;
    Uint64 lastFrameChangeAt = SDL_GetTicks();

    TexturePtr round1Map =
        LoadTexture(renderer.get(), Round1MapPath, SDL_SCALEMODE_LINEAR);
    Animation axelIdle = LoadIdleAnimation(renderer.get());

    std::cout
        << "Controles:\n"
        << "  Flechas izquierda/derecha: mover camara\n"
        << "  + / -: cambiar tamano de Axel\n"
        << "  Espacio: pausar/reanudar\n"
        << "  1, 2, 3: mostrar un frame\n"
        << "  R: recargar recursos\n"
        << "  Esc: salir\n";

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

            if (event.type != SDL_EVENT_KEY_DOWN)
            {
                continue;
            }

            const SDL_Keycode key = event.key.key;
            if (key >= SDLK_1 && key <= SDLK_3)
            {
                const std::size_t requested =
                    static_cast<std::size_t>(key - SDLK_1);
                if (requested < axelIdle.frames.size())
                {
                    currentFrame = requested;
                    animationEnabled = false;
                }
                continue;
            }

            switch (key)
            {
            case SDLK_ESCAPE:
                running = false;
                break;
            case SDLK_LEFT:
                cameraX -= CameraStep;
                break;
            case SDLK_RIGHT:
                cameraX += CameraStep;
                break;
            case SDLK_HOME:
                cameraX = 0.0F;
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
                break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
                axelFrameHeight = std::max(
                    AxelMinimumFrameHeight,
                    axelFrameHeight - AxelScaleStep);
                break;
            case SDLK_R:
                round1Map =
                    LoadTexture(renderer.get(), Round1MapPath, SDL_SCALEMODE_LINEAR);
                axelIdle = LoadIdleAnimation(renderer.get());
                cameraX = 0.0F;
                playbackStep = 0;
                currentFrame = 0;
                lastFrameChangeAt = SDL_GetTicks();
                break;
            default:
                break;
            }
        }

        if (animationEnabled && !axelIdle.playbackOrder.empty())
        {
            const Uint64 now = SDL_GetTicks();
            const Uint64 elapsed = now - lastFrameChangeAt;
            if (elapsed >= AxelIdleFrameDurationMs)
            {
                const Uint64 advance = elapsed / AxelIdleFrameDurationMs;
                playbackStep =
                    (playbackStep + static_cast<std::size_t>(advance)) %
                    axelIdle.playbackOrder.size();
                currentFrame = axelIdle.playbackOrder[playbackStep];
                lastFrameChangeAt += advance * AxelIdleFrameDurationMs;
            }
        }

        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());

        if (round1Map)
        {
            RenderBackground(renderer.get(), round1Map.get(), cameraX);
        }

        RenderAxelIdle(
            renderer.get(),
            axelIdle,
            currentFrame,
            axelFrameHeight);

        SDL_RenderPresent(renderer.get());
    }

    axelIdle.frames.clear();
    round1Map.reset();
    renderer.reset();
    window.reset();
    SDL_Quit();
    return EXIT_SUCCESS;
}
