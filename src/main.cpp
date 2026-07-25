#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <iostream>

namespace
{
constexpr int LogicalWidth = 320;
constexpr int LogicalHeight = 180;
constexpr int WindowWidth = 1920;
constexpr int WindowHeight = 1080;
}

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if (!SDL_CreateWindowAndRenderer(
            "Streets Enhanced",
            WindowWidth,
            WindowHeight,
            SDL_WINDOW_RESIZABLE,
            &window,
            &renderer))
    {
        std::cerr << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (!SDL_SetRenderLogicalPresentation(
            renderer,
            LogicalWidth,
            LogicalHeight,
            SDL_LOGICAL_PRESENTATION_INTEGER_SCALE))
    {
        std::cerr << "SDL_SetRenderLogicalPresentation failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

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
            else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
            {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Temporary debug framing for the 320x180 logical canvas.
        // The real Round 1 background and sprites will replace this once
        // the locally stored original assets are available to the project.
        SDL_FRect playArea{0.0F, 48.0F, 320.0F, 132.0F};
        SDL_SetRenderDrawColor(renderer, 18, 24, 48, 255);
        SDL_RenderFillRect(renderer, &playArea);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
