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
    float referenceWidth = 0.0F;
    float referenceHeight = 0.0F;
};

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
    int lastNumber = std::numeric_limits<int>::max();

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
        lastNumber = value;
    }

    return lastNumber;
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

    // Compatibilidad por si los PNG separados se guardaron directamente dentro
    // de la carpeta "Axel Stone" en lugar de dentro de "Idle".
    if (files.empty() && std::filesystem::is_directory(AxelFolderPath))
    {
        for (const auto& entry : std::filesystem::directory_iterator(AxelFolderPath))
        {
            if (!entry.is_regular_file() || !IsPng(entry.path()))
            {
                continue;
            }

            const std::string lowerName = ToLower(entry.path().stem().string());
            const bool isSeparateIdleFrame =
                lowerName.find("idle") != std::string::npos &&
                lowerName != "axel stone_idle";

            if (isSeparateIdleFrame)
            {
                files.push_back(entry.path());
            }
        }
    }

    std::sort(
        files.begin(),
        files.end(),
        [](const std::filesystem::path& left, const std::filesystem::path& right)
        {
            const int leftNumber = ExtractFrameNumber(left);
            const int rightNumber = ExtractFrameNumber(right);
            if (leftNumber != rightNumber)
            {
                return leftNumber < rightNumber;
            }
            return ToLower(left.filename().string()) < ToLower(right.filename().string());
        });

    return files;
}

Animation LoadIdleAnimation(SDL_Renderer* renderer)
{
    Animation animation;
    const auto frameFiles = FindIdleFrameFiles();

    if (frameFiles.empty())
    {
        std::cerr
            << "No se encontraron frames PNG separados para el estado Idle.\n"
            << "Carpeta esperada: " << AxelIdleFramesPath << '\n'
            << "Nombres recomendados: Idle_00.png, Idle_01.png, ...\n";
        return animation;
    }

    float firstWidth = 0.0F;
    float firstHeight = 0.0F;
    bool dimensionsDiffer = false;

    for (const auto& path : frameFiles)
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

        animation.referenceWidth = std::max(animation.referenceWidth, width);
        animation.referenceHeight = std::max(animation.referenceHeight, height);
        animation.frames.push_back(AnimationFrame{path, std::move(texture), width, height});
    }

    std::cout << "Frames Idle cargados: " << animation.frames.size() << '\n';
    for (std::size_t index = 0; index < animation.frames.size(); ++index)
    {
        const auto& frame = animation.frames[index];
        std::cout << "  [" << index << "] " << frame.path.filename().string()
                  << " (" << frame.width << "x" << frame.height << ")\n";
    }

    if (dimensionsDiffer)
    {
        std::cout
            << "AVISO: los frames no tienen todos el mismo tamano de lienzo. "
            << "Se alinearan por el centro inferior, pero lo ideal es que todos "
            << "tengan exactamente las mismas dimensiones y los pies en la misma linea.\n";
    }

    return animation;
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
    const Animation& animation,
    std::size_t frameIndex,
    float displayedReferenceHeight)
{
    if (animation.frames.empty() || animation.referenceHeight <= 0.0F)
    {
        return;
    }

    frameIndex %= animation.frames.size();
    const AnimationFrame& frame = animation.frames[frameIndex];

    // Todos los PNG usan el mismo punto de anclaje jugable: centro inferior.
    // El factor de escala es comun para todos los frames, por lo que el personaje
    // no cambia de tamano entre dibujos aunque un PNG tenga margenes distintos.
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
            "Streets Enhanced - Axel Idle Frames Test",
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
    std::size_t currentFrame = 0;
    Uint64 lastFrameChangeAt = SDL_GetTicks();

    TexturePtr round1Map =
        LoadTexture(renderer.get(), Round1MapPath, SDL_SCALEMODE_LINEAR);
    Animation axelIdle = LoadIdleAnimation(renderer.get());

    std::cout << "Prueba de Axel sobre Round 1 en 1920x1080:\n"
              << "  Flechas izquierda/derecha: mover camara\n"
              << "  + / -: aumentar o reducir el tamano de Axel\n"
              << "  Espacio: pausar/reanudar la animacion\n"
              << "  1 a 9: mostrar un fotograma concreto\n"
              << "  Inicio: volver al principio del escenario\n"
              << "  R: recargar fondo y frames Idle\n"
              << "  Esc: salir\n"
              << "Fondo: " << Round1MapPath << '\n'
              << "Carpeta Idle: " << AxelIdleFramesPath << '\n'
              << "Altura inicial de referencia de Axel: "
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
                const SDL_Keycode key = event.key.key;

                if (key >= SDLK_1 && key <= SDLK_9 && !axelIdle.frames.empty())
                {
                    const std::size_t requestedFrame =
                        static_cast<std::size_t>(key - SDLK_1);
                    if (requestedFrame < axelIdle.frames.size())
                    {
                        currentFrame = requestedFrame;
                        idleAnimationEnabled = false;
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
                    idleAnimationEnabled = !idleAnimationEnabled;
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
                    axelIdle = LoadIdleAnimation(renderer.get());
                    cameraX = 0.0F;
                    currentFrame = 0;
                    lastFrameChangeAt = SDL_GetTicks();
                    break;
                default:
                    break;
                }
            }
        }

        if (idleAnimationEnabled && !axelIdle.frames.empty())
        {
            const Uint64 now = SDL_GetTicks();
            const Uint64 elapsed = now - lastFrameChangeAt;
            if (elapsed >= AxelIdleFrameDurationMs)
            {
                const Uint64 framesToAdvance = elapsed / AxelIdleFrameDurationMs;
                currentFrame =
                    (currentFrame + static_cast<std::size_t>(framesToAdvance)) %
                    axelIdle.frames.size();
                lastFrameChangeAt += framesToAdvance * AxelIdleFrameDurationMs;
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
