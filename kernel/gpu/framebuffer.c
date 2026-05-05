#include "framebuffer.h"
#include <stdint.h>

framebuffer_t g_fb;

void fb_init(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp) {
    g_fb.addr   = (uint32_t *)(uintptr_t)addr;
    g_fb.width  = width;
    g_fb.height = height;
    g_fb.pitch  = pitch;
    g_fb.bpp    = bpp;
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color, uint32_t pitch) {
    if (x >= g_fb.width || y >= g_fb.height) return;
    uint32_t *pixel_ptr = (uint32_t *)((uint8_t *)g_fb.addr + (y * pitch) + (x * (g_fb.bpp / 8)));
    *pixel_ptr = color;
}

void fb_fill(uint32_t color, uint32_t pitch) {
    for (uint32_t y = 0; y < g_fb.height; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)g_fb.addr + y * pitch);
        for (uint32_t x = 0; x < g_fb.width; x++) {
            row[x] = color;
        }
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, uint32_t pitch) {
    for (uint32_t row = y; row < y + h; row++) {
        for (uint32_t col = x; col < x + w; col++) {
            fb_put_pixel(col, row, color, pitch);
        }
    }
}
