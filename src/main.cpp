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

const std::filesystem::path Round1MapPath =
    std::filesystem::path{STREETS_SOURCE_DIR} /
    "LocalAssets" /
    "ReferenceSheets" /
    "Stages" /
    "Round1.png";

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

TexturePtr LoadBackgroundTexture(SDL_Renderer* renderer, const std::filesystem::path& path)
{
    SDL_Texture* texture = IMG_LoadTexture(renderer, path.string().c_str());
    if (texture == nullptr)
    {
        std::cerr << "No se pudo cargar la imagen: " << path << '\n'
                  << "SDL_image: " << SDL_GetError() << '\n';
        return {};
    }

    // El nuevo fondo es una ilustracion HD, no pixel art. El filtrado lineal
    // evita que se vea dentado al adaptarlo a la ventana manteniendo proporciones.
    if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR))
    {
        std::cerr << "No se pudo activar el filtrado lineal: " << SDL_GetError() << '\n';
        SDL_DestroyTexture(texture);
        return {};
    }

    return TexturePtr{texture};
}

void DrawMissingAsset(SDL_Renderer* renderer)
{
    SDL_SetRenderDrawColor(renderer, 34, 18, 48, 255);
    SDL_RenderClear(renderer);
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
            "Streets Enhanced - Round 1 HD",
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

    // El lienzo interno del juego ya es Full HD. Si la ventana cambia de tamano,
    // SDL conserva el formato 16:9 mediante letterbox, sin deformar la imagen.
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
    TexturePtr round1Map = LoadBackgroundTexture(renderer.get(), Round1MapPath);

    std::cout << "Prueba Round 1 en 1920x1080:\n"
              << "  Flechas izquierda/derecha: mover camara\n"
              << "  Inicio: volver al principio\n"
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
                case SDLK_LEFT:
                    cameraX -= CameraStep;
                    break;
                case SDLK_RIGHT:
                    cameraX += CameraStep;
                    break;
                case SDLK_HOME:
                    cameraX = 0.0F;
                    break;
                case SDLK_R:
                    round1Map = LoadBackgroundTexture(renderer.get(), Round1MapPath);
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
            float textureWidth = 0.0F;
            float textureHeight = 0.0F;
            SDL_GetTextureSize(round1Map.get(), &textureWidth, &textureHeight);

            const float viewportAspect =
                static_cast<float>(LogicalWidth) / static_cast<float>(LogicalHeight);
            const float textureAspect = textureWidth / textureHeight;

            SDL_FRect source{};
            if (textureAspect >= viewportAspect)
            {
                // El escenario es mas largo que 16:9: se muestra toda su altura y
                // la camara recorre horizontalmente el resto del nivel.
                source.w = textureHeight * viewportAspect;
                source.h = textureHeight;
                const float maximumCameraX = std::max(0.0F, textureWidth - source.w);
                cameraX = std::clamp(cameraX, 0.0F, maximumCameraX);
                source.x = cameraX;
                source.y = 0.0F;
            }
            else
            {
                // Caso defensivo para imagenes menos panoramicas: recorte vertical
                // centrado, siempre sin deformar la relacion de aspecto.
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

            SDL_RenderTexture(renderer.get(), round1Map.get(), &source, &destination);
        }
        else
        {
            DrawMissingAsset(renderer.get());
        }

        SDL_RenderPresent(renderer.get());
    }

    round1Map.reset();
    renderer.reset();
    window.reset();
    SDL_Quit();
    return EXIT_SUCCESS;
}
