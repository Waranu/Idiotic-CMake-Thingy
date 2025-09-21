#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_video.h"

uint32_t width = 1024;
uint32_t height = 800;

int main() {
    if ( !SDL_Init( SDL_INIT_VIDEO ) ) return -1;

    SDL_Window* window = SDL_CreateWindow(
        "A GPU Window", width, height, SDL_EVENT_WINDOW_SHOWN );

    SDL_GPUDevice* gpu = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr );

    if ( !SDL_ClaimWindowForGPUDevice( gpu, window ) ) {
        printf( "Failed to claim window for GPU\n" );
        return -1;
    }

    bool run = true;
    SDL_Event event = {};
    while ( run ) {
        while ( SDL_PollEvent( &event ) ) {
            switch ( event.type ) {
                case SDL_EVENT_QUIT:
                    run = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if ( event.key.key == SDLK_ESCAPE ) {
                        run = false;
                    }
                    break;
            }
        }
    }

    SDL_GPUCommandBuffer* commandBuffer =
        SDL_AcquireGPUCommandBuffer( gpu );

    SDL_GPUTexture* swapchain = nullptr;
    SDL_WaitAndAcquireGPUSwapchainTexture(
        commandBuffer, window, &swapchain, &width, &height );

    assert( SDL_SubmitGPUCommandBuffer( commandBuffer ) );

    SDL_Quit();
    return 0;
}
