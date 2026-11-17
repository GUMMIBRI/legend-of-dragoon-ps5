#include "usb_storage.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_timer.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Link the PS5 title with -Wl,--wrap=main. The title runtime keeps its main. */
int __real_main(int argc, char **argv);
void pe_application_stage(const char *message);
int sceSystemServiceHideSplashScreen(void);

enum { SPLASH_DELAY_MS = 1500, SPLASH_RETRY_MS = 250, SPLASH_MAX_ATTEMPTS = 3 };

static Uint64 launch_ticks;

static Uint32 SDLCALL dismiss_launch_splash(void *userdata, SDL_TimerID timer, Uint32 interval)
{
    static unsigned attempts;
    (void)userdata;
    (void)timer;
    (void)interval;

    const int result = sceSystemServiceHideSplashScreen();
    ++attempts;
    fprintf(stderr, "Dragoon: PS5 splash dismissal at %" PRIu64
                    " ms, attempt %u, result 0x%08x\n",
            (uint64_t)(SDL_GetTicks() - launch_ticks), attempts, (unsigned)result);
    return result == 0 || attempts >= SPLASH_MAX_ATTEMPTS ? 0 : SPLASH_RETRY_MS;
}

static int disc_one_available(void)
{
    FILE *image = fopen(DRAGOON_DATA_ROOT "/disc/disc1.bin", "rb");
    if (!image)
        return 0;

    unsigned char descriptor[72];
    int valid = fseek(image, 16 * 2352 + 24, SEEK_SET) == 0 &&
                fread(descriptor, 1, sizeof descriptor, image) == sizeof descriptor &&
                descriptor[0] == 1 && descriptor[6] == 1 &&
                memcmp(descriptor + 1, "CD001", 5) == 0 &&
                memcmp(descriptor + 40, "SCUS94491", 9) == 0;
    fclose(image);
    return valid;
}

int __wrap_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    launch_ticks = SDL_GetTicks();
    if (dragoon_usb_enter() < 0) {
        pe_application_stage("Dragoon: could not open the USB game directory\n");
        return EXIT_FAILURE;
    }

    if (!disc_one_available()) {
        pe_application_stage("Dragoon: disc/disc1.bin is missing or is not US Disc 1 (SCUS-94491)\n");
        return EXIT_FAILURE;
    }

    /* Keep runtime errors alongside the CRT stages for the same boot. */
    if (!freopen(DRAGOON_LOG_ROOT "/runtime.log", "a", stderr)) {
        pe_application_stage("Dragoon: could not open USB runtime log\n");
        return EXIT_FAILURE;
    }
    setvbuf(stderr, NULL, _IONBF, 0);
    if (!freopen(DRAGOON_LOG_ROOT "/runtime.log", "a", stdout)) {
        pe_application_stage("Dragoon: could not redirect stdout to USB runtime log\n");
        return EXIT_FAILURE;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    fprintf(stderr, "\n=== Dragoon build: menu-movie-scope-dev-r2 ===\n");

    /* The guest rasterizer produces CPU pixels. Present them through the PS5
     * SDL GPU driver rather than SDL's generic window-surface driver. */
    if (!SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "gpu", SDL_HINT_OVERRIDE)) {
        pe_application_stage("Dragoon: could not select SDL GPU presentation\n");
        return EXIT_FAILURE;
    }
    const Uint64 elapsed = SDL_GetTicks() - launch_ticks;
    const Uint32 delay = elapsed < SPLASH_DELAY_MS ? (Uint32)(SPLASH_DELAY_MS - elapsed) : 1;
    if (!SDL_AddTimer(delay, dismiss_launch_splash, NULL)) {
        fprintf(stderr, "Dragoon: splash timer unavailable: %s\n", SDL_GetError());
        dismiss_launch_splash(NULL, 0, 0);
    }
    pe_application_stage("Dragoon: USB paths initialized; entering game runtime\n");

    /* A launcher argument must not redirect game data or saves. */
    char game_option[] = "--game";
    char game_path[] = DRAGOON_DATA_ROOT "/game.toml";
    char bios_option[] = "--bios";
    char bios_path[] = DRAGOON_DATA_ROOT "/bios/openbios.bin";
    char disc_option[] = "--disc";
    char disc_path[] = DRAGOON_DATA_ROOT "/disc/disc1.cue";
    char saves_option[] = "--memcard-dir";
    char saves_path[] = DRAGOON_DATA_ROOT "/saves";
    char renderer_option[] = "--renderer";
    char renderer[] = "vulkan";
    char executable[] = DRAGOON_APP_ROOT "/eboot.bin";
    char *usb_argv[] = {
        executable, game_option, game_path, bios_option, bios_path,
        disc_option, disc_path, saves_option, saves_path, renderer_option, renderer, NULL,
    };
    return __real_main((int)(sizeof usb_argv / sizeof usb_argv[0]) - 1, usb_argv);
}
