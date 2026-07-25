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
constexpr int LogicalWidth = 384;
constexpr int LogicalHeight = 216;
constexpr int WindowWidth = 1920;
constexpr int WindowHeight = 1080;

constexpr int OriginalScreenWidth = 256;
constexpr int OriginalScreenHeight = 192;
constexpr int OriginalHudHeight = 16;
constexpr int OriginalStageHeight = 176;

constexpr int OriginalViewportLeft = (LogicalWidth - OriginalScreenWidth) / 2;
constexpr int OriginalViewportTop = (LogicalHeight - OriginalScreenHeight) / 2;

const std::filesystem::path Round1MapPath =
    std::filesystem::path{STREETS_SOURCE_DIR} /
    "LocalAssets" /
    "ReferenceSheets" /
    "Stages" /
    "Round1.png";

enum class ViewMode
{
    Original4x3,
    Widescreen16x9
};

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

TexturePtr LoadNearestTexture(SDL_Renderer* renderer, const std::filesystem::path& path)
{
    SDL_Texture* texture = IMG_LoadTexture(renderer, path.string().c_str());
    if (texture == nullptr)
    {
        std::cerr << "No se pudo cargar la imagen: " << path << '\n'
                  << "SDL_image: " << SDL_GetError() << '\n';
        return {};
    }

    if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST))
    {
        std::cerr << "No se pudo activar nearest-neighbour: " << SDL_GetError() << '\n';
        SDL_DestroyTexture(texture);
        return {};
    }

    return TexturePtr{texture};
}

void DrawMissingAsset(SDL_Renderer* renderer, const SDL_FRect& area)
{
    SDL_SetRenderDrawColor(renderer, 34, 18, 48, 255);
    SDL_RenderFillRect(renderer, &area);

    constexpr float TileSize = 8.0F;
    for (float y = area.y; y < area.y + area.h; y += TileSize)
    {
        for (float x = area.x; x < area.x + area.w; x += TileSize)
        {
            const auto tileX = static_cast<int>((x - area.x) / TileSize);
            const auto tileY = static_cast<int>((y - area.y) / TileSize);
            if (((tileX + tileY) & 1) == 0)
            {
                SDL_FRect tile{x, y, TileSize, TileSize};
                SDL_SetRenderDrawColor(renderer, 54, 28, 72, 255);
                SDL_RenderFillRect(renderer, &tile);
            }
        }
    }
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
            "Streets Enhanced",
            WindowWidth,
            WindowHeight,
            SDL_WINDOW_RESIZABLE,
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
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE))
    {
        std::cerr << "SDL_SetRenderLogicalPresentation failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_NONE);

    ViewMode viewMode = ViewMode::Original4x3;
    int cameraX = 0;
    TexturePtr round1Map = LoadNearestTexture(renderer.get(), Round1MapPath);

    std::cout << "Controles de la prueba:\n"
              << "  F1: encuadre original 256x192\n"
              << "  F2: encuadre panoramico 384x216\n"
              << "  Flechas izquierda/derecha: mover camara\n"
              << "  R: recargar Round1.png\n"
              << "  Esc: salir\n"
              << "Ruta esperada del mapa: " << Round1MapPath << '\n';

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
                case SDLK_F1:
                    viewMode = ViewMode::Original4x3;
                    break;
                case SDLK_F2:
                    viewMode = ViewMode::Widescreen16x9;
                    break;
                case SDLK_LEFT:
                    cameraX = std::max(0, cameraX - 8);
                    break;
                case SDLK_RIGHT:
                    cameraX += 8;
                    break;
                case SDLK_R:
                    round1Map = LoadNearestTexture(renderer.get(), Round1MapPath);
                    cameraX = 0;
                    break;
                default:
                    break;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());

        const bool originalMode = viewMode == ViewMode::Original4x3;
        const float viewportLeft = originalMode ? static_cast<float>(OriginalViewportLeft) : 0.0F;
        const float viewportWidth = originalMode ? static_cast<float>(OriginalScreenWidth) : static_cast<float>(LogicalWidth);
        const float viewportTop = static_cast<float>(OriginalViewportTop);

        SDL_FRect hudArea{
            viewportLeft,
            viewportTop,
            viewportWidth,
            static_cast<float>(OriginalHudHeight)};
        SDL_SetRenderDrawColor(renderer.get(), 8, 8, 16, 255);
        SDL_RenderFillRect(renderer.get(), &hudArea);

        SDL_FRect stageArea{
            viewportLeft,
            viewportTop + static_cast<float>(OriginalHudHeight),
            viewportWidth,
            static_cast<float>(OriginalStageHeight)};

        if (round1Map)
        {
            float textureWidth = 0.0F;
            float textureHeight = 0.0F;
            SDL_GetTextureSize(round1Map.get(), &textureWidth, &textureHeight);

            const int sourceWidth = originalMode ? OriginalScreenWidth : LogicalWidth;
            const int maximumCameraX = std::max(0, static_cast<int>(textureWidth) - sourceWidth);
            cameraX = std::clamp(cameraX, 0, maximumCameraX);

            SDL_FRect source{
                static_cast<float>(cameraX),
                0.0F,
                std::min(static_cast<float>(sourceWidth), textureWidth),
                std::min(static_cast<float>(OriginalStageHeight), textureHeight)};

            SDL_RenderTexture(renderer.get(), round1Map.get(), &source, &stageArea);
        }
        else
        {
            DrawMissingAsset(renderer.get(), stageArea);
        }

        SDL_RenderPresent(renderer.get());
    }

    round1Map.reset();
    renderer.reset();
    window.reset();
    SDL_Quit();
    return EXIT_SUCCESS;
}
