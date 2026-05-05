#pragma once
#include <stdint.h>

typedef struct {
    uint32_t *addr;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;
    uint8_t   bpp;
} framebuffer_t;

extern framebuffer_t g_fb;

void fb_init(uint64_t addr, uint32_t width, uint32_t height, uint32_t pitch, uint8_t bpp);
void fb_fill(uint32_t color, uint32_t pitch);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color, uint32_t pitch);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color, uint32_t pitch);
