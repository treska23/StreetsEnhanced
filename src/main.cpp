#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

namespace
{
constexpr int LogicalWidth = 1920;
constexpr int LogicalHeight = 1080;
constexpr int WindowWidth = 1920;
constexpr int WindowHeight = 1080;
constexpr float CameraStep = 64.0F;

constexpr int AxelIdleColumns = 3;
constexpr int AxelIdleRows = 2;
constexpr int AxelIdleFrameCount = 6;
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

const std::filesystem::path AxelIdlePath =
    AssetsRoot /
    "Axel Stone" /
    "Axel Stone_Idle.png";

struct WindowDeleter
{
    void operator()(SDL_Window* window) const noexcept
    {
        SDL_DestroyWindow(window);
    }
};

struct RendererDeleter
{
    void operator()(SDL_Renderer* renderer) const noexcept
    {
        SDL_DestroyRenderer(renderer);
    }
};

struct TextureDeleter
{
    void operator()(SDL_Texture* texture) const noexcept
    {
        SDL_DestroyTexture(texture);
    }
};

using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, TextureDeleter>;

TexturePtr LoadTexture(
    SDL_Renderer* renderer,
    const std::filesystem::path& path,
    SDL_ScaleMode scaleMode)
{
    SDL_Texture* texture = IMG_LoadTexture(renderer, path.string().c_str());
    if (texture == nullptr)
    {
        std::cerr << "No se pudo cargar la imagen: " << path << '\n'
                  << "SDL_image: " << SDL_GetError() << '\n';
        return {};
    }

    if (!SDL_SetTextureScaleMode(texture, scaleMode))
    {
        std::cerr << "No se pudo configurar el filtrado de la textura: "
                  << SDL_GetError() << '\n';
        SDL_DestroyTexture(texture);
        return {};
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return TexturePtr{texture};
}

void DrawMissingAsset(SDL_Renderer* renderer)
{
    SDL_SetRenderDrawColor(renderer, 34, 18, 48, 255);
    SDL_RenderClear(renderer);
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
        const float maximumCameraX = std::max(0.0F, textureWidth - source.w);
        cameraX = std::clamp(cameraX, 0.0F, maximumCameraX);
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
    SDL_Texture* texture,
    int frameIndex,
    float displayedFrameHeight)
{
    float textureWidth = 0.0F;
    float textureHeight = 0.0F;
    SDL_GetTextureSize(texture, &textureWidth, &textureHeight);

    const float frameWidth = textureWidth / static_cast<float>(AxelIdleColumns);
    const float frameHeight = textureHeight / static_cast<float>(AxelIdleRows);

    const int column = frameIndex % AxelIdleColumns;
    const int row = frameIndex / AxelIdleColumns;

    const SDL_FRect source{
        static_cast<float>(column) * frameWidth,
        static_cast<float>(row) * frameHeight,
        frameWidth,
        frameHeight};

    const float displayedFrameWidth =
        displayedFrameHeight * (frameWidth / frameHeight);

    const SDL_FRect destination{
        (static_cast<float>(LogicalWidth) - displayedFrameWidth) * 0.5F,
        AxelFeetY - displayedFrameHeight,
        displayedFrameWidth,
        displayedFrameHeight};

    SDL_RenderTexture(renderer, texture, &source, &destination);
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
            "Streets Enhanced - Axel Idle Test",
            WindowWidth,
            WindowHeight,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
            &rawWindow,
            &rawRenderer))
    {
        std::cerr << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError() << '\n';
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
        std::cerr << "SDL_SetRenderLogicalPresentation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_BLEND);

    float cameraX = 0.0F;
    float axelFrameHeight = AxelDefaultFrameHeight;
    bool idleAnimationEnabled = true;
    int frozenFrame = 0;

    TexturePtr round1Map =
        LoadTexture(renderer.get(), Round1MapPath, SDL_SCALEMODE_LINEAR);
    TexturePtr axelIdle =
        LoadTexture(renderer.get(), AxelIdlePath, SDL_SCALEMODE_LINEAR);

    const Uint64 animationStartedAt = SDL_GetTicks();

    std::cout << "Prueba de Axel sobre Round 1 en 1920x1080:\n"
              << "  Flechas izquierda/derecha: mover camara\n"
              << "  + / -: aumentar o reducir el tamano de Axel\n"
              << "  Espacio: pausar/reanudar la animacion\n"
              << "  1 a 6: mostrar un fotograma concreto\n"
              << "  Inicio: volver al principio del escenario\n"
              << "  R: recargar fondo y animacion\n"
              << "  Esc: salir\n"
              << "Fondo: " << Round1MapPath << '\n'
              << "Idle: " << AxelIdlePath << '\n'
              << "Altura inicial del lienzo de Axel: "
              << axelFrameHeight << " px\n";

    bool running = true;
    while (running)
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                switch (event.key.key)
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
                    idleAnimationEnabled = !idleAnimationEnabled;
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
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                case SDLK_5:
                case SDLK_6:
                    frozenFrame = static_cast<int>(event.key.key - SDLK_1);
                    idleAnimationEnabled = false;
                    break;
                case SDLK_R:
                    round1Map =
                        LoadTexture(renderer.get(), Round1MapPath, SDL_SCALEMODE_LINEAR);
                    axelIdle =
                        LoadTexture(renderer.get(), AxelIdlePath, SDL_SCALEMODE_LINEAR);
                    cameraX = 0.0F;
                    break;
                default:
                    break;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());

        if (round1Map)
        {
            RenderBackground(renderer.get(), round1Map.get(), cameraX);
        }
        else
        {
            DrawMissingAsset(renderer.get());
        }

        if (axelIdle)
        {
            int frameIndex = frozenFrame;
            if (idleAnimationEnabled)
            {
                const Uint64 elapsed = SDL_GetTicks() - animationStartedAt;
                frameIndex = static_cast<int>(
                    (elapsed / AxelIdleFrameDurationMs) % AxelIdleFrameCount);
            }

            RenderAxelIdle(
                renderer.get(),
                axelIdle.get(),
                frameIndex,
                axelFrameHeight);
        }

        SDL_RenderPresent(renderer.get());
    }

    axelIdle.reset();
    round1Map.reset();
    renderer.reset();
    window.reset();
    SDL_Quit();
    return EXIT_SUCCESS;
}
