/* PSP homebrew entry point. This MVP renders the Material UI and playback chrome. */
#ifdef __PSP__
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <psptypes.h>
#include <string.h>

#include "ezt.h"

PSP_MODULE_INFO("EZTPlayer", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define PSP_STRIDE 512
static uint32_t software[EZ_SCREEN_W * EZ_SCREEN_H];
static uint32_t display_buf[PSP_STRIDE * EZ_SCREEN_H] __attribute__((aligned(64)));
static ez_app app;
static ez_fb fb;

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int cb = sceKernelCreateCallback("Exit Callback", exit_callback, 0);
    if (cb >= 0) sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int th = sceKernelCreateThread("callback_thread", callback_thread, 0x11, 0x1000,
                                   THREAD_ATTR_USER, 0);
    if (th >= 0) sceKernelStartThread(th, 0, 0);
}

static void present(void)
{
    ez_app_render(&app, &fb);
    for (int y = 0; y < EZ_SCREEN_H; y++) {
        const uint32_t *src = software + y * EZ_SCREEN_W;
        uint32_t *dst = display_buf + y * PSP_STRIDE;
        for (int x = 0; x < EZ_SCREEN_W; x++) {
            uint32_t c = src[x]; /* software is ARGB; PSP's 8888 framebuffer is ABGR */
            dst[x] = (c & 0xFF00FF00u) | ((c & 0x000000FFu) << 16) |
                     ((c & 0x00FF0000u) >> 16);
        }
    }
    sceDisplaySetFrameBuf(display_buf, PSP_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart();
}

int main(void)
{
    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    ez_fb_init(&fb, EZ_SCREEN_W, EZ_SCREEN_H, software);
    ez_app_init(&app);

    SceCtrlData pad;
    unsigned int old = 0;
    int old_ax = 0, old_ay = 0;
    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned int now = pad.Buttons;
        unsigned int pressed = now & ~old;
        old = now;
        if (pressed & PSP_CTRL_LEFT) ez_app_action(&app, EZ_ACT_LEFT);
        if (pressed & PSP_CTRL_RIGHT) ez_app_action(&app, EZ_ACT_RIGHT);
        if (pressed & PSP_CTRL_UP) ez_app_action(&app, EZ_ACT_UP);
        if (pressed & PSP_CTRL_DOWN) ez_app_action(&app, EZ_ACT_DOWN);
        if (pressed & PSP_CTRL_CROSS) ez_app_action(&app, EZ_ACT_CONFIRM);
        if (pressed & PSP_CTRL_CIRCLE) ez_app_action(&app, EZ_ACT_BACK);
        if (pressed & PSP_CTRL_LTRIGGER) ez_app_action(&app, EZ_ACT_TAB_PREV);
        if (pressed & PSP_CTRL_RTRIGGER) ez_app_action(&app, EZ_ACT_TAB_NEXT);
        if (pressed & PSP_CTRL_TRIANGLE) ez_app_action(&app, EZ_ACT_MENU);
        if (pressed & PSP_CTRL_SQUARE) ez_app_action(&app, EZ_ACT_THEME);

        int ax = pad.Lx < 64 ? -1 : pad.Lx > 192 ? 1 : 0;
        int ay = pad.Ly < 64 ? -1 : pad.Ly > 192 ? 1 : 0;
        if (ax < 0 && old_ax >= 0) ez_app_action(&app, EZ_ACT_LEFT);
        if (ax > 0 && old_ax <= 0) ez_app_action(&app, EZ_ACT_RIGHT);
        if (ay < 0 && old_ay >= 0) ez_app_action(&app, EZ_ACT_UP);
        if (ay > 0 && old_ay <= 0) ez_app_action(&app, EZ_ACT_DOWN);
        old_ax = ax; old_ay = ay;

        ez_app_update(&app, 1.0f / 60.0f);
        present();
        sceKernelDelayThread(16000);
    }
    return 0;
}
#endif
