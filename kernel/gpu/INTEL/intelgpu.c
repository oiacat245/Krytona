/**
 * intelgpu.c - Implementação do driver Intel GPU para SO próprio
 * Compatível com Intel GMA/HD Graphics Gen 4-9
 *
 * Referências:
 *  - Intel Open Source HD Graphics Programmer's Reference Manual (PRM)
 *  - Linux i915 driver (drivers/gpu/drm/i915)
 *  - OSDev Wiki: Intel HD Graphics
 */

#include "intelgpu.h"

/* ============================================================
 * Stubs de funções do kernel do seu SO
 * Substitua pelas suas implementações reais
 * ============================================================ */

/* Leitura/escrita de porta PCI config space */
static inline u32 pci_read32(u8 bus, u8 slot, u8 func, u8 offset) {
    /* Exemplo para x86: acesso via porta 0xCF8/0xCFC */
    u32 addr = (u32)(
        (1u    << 31) |  /* enable bit */
        ((u32)bus  << 16) |
        ((u32)slot << 11) |
        ((u32)func <<  8) |
        (offset & 0xFC)
    );
    /* outl(addr, 0xCF8); return inl(0xCFC); */
    (void)addr;
    return 0; /* substituir pela sua implementação */
}

static inline void pci_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 val) {
    u32 addr = (u32)(
        (1u    << 31) |
        ((u32)bus  << 16) |
        ((u32)slot << 11) |
        ((u32)func <<  8) |
        (offset & 0xFC)
    );
    (void)addr; (void)val;
    /* outl(addr, 0xCF8); outl(val, 0xCFC); */
}

/* Mapeamento de memória física -> virtual */
static inline void *phys_to_virt(u64 phys, u64 size) {
    (void)size;
    /* No seu kernel: retorne o endereço mapeado na page table */
    /* Exemplo simples (identity map): */
    return (void *)(uintptr_t)phys;
}

/* Delay em microsegundos */
static inline void udelay(u32 us) {
    (void)us;
    /* Implemente com seu timer ou busy-loop calibrado */
    volatile u32 i;
    for (i = 0; i < us * 1000; i++) __asm__ volatile ("nop");
}

/* memset simples (caso não tenha libc) */
static void *intel_memset(void *dst, int val, size_t n) {
    u8 *p = (u8 *)dst;
    while (n--) *p++ = (u8)val;
    return dst;
}

/* memcpy simples */
static void *intel_memcpy(void *dst, const void *src, size_t n) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* ============================================================
 * Offsets PCI config space
 * ============================================================ */
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C

#define PCI_CMD_MEMORY      (1 << 1)   /* Memory space enable */
#define PCI_CMD_MASTER      (1 << 2)   /* Bus master enable */

/* ============================================================
 * Modos pré-definidos
 * ============================================================ */

const intel_mode_t INTEL_MODE_640x480_32 = {
    .width        = 640,  .height       = 480,
    .bpp          = 32,   .pitch        = 640 * 4,
    .refresh_hz   = 60,
    .h_active     = 640,  .h_sync_start = 656,
    .h_sync_end   = 752,  .h_total      = 800,
    .v_active     = 480,  .v_sync_start = 490,
    .v_sync_end   = 492,  .v_total      = 525,
    .dot_clock_khz = 25175
};

const intel_mode_t INTEL_MODE_800x600_32 = {
    .width        = 800,  .height       = 600,
    .bpp          = 32,   .pitch        = 800 * 4,
    .refresh_hz   = 60,
    .h_active     = 800,  .h_sync_start = 840,
    .h_sync_end   = 968,  .h_total      = 1056,
    .v_active     = 600,  .v_sync_start = 601,
    .v_sync_end   = 605,  .v_total      = 628,
    .dot_clock_khz = 40000
};

const intel_mode_t INTEL_MODE_1024x768_32 = {
    .width        = 1024, .height       = 768,
    .bpp          = 32,   .pitch        = 1024 * 4,
    .refresh_hz   = 60,
    .h_active     = 1024, .h_sync_start = 1048,
    .h_sync_end   = 1184, .h_total      = 1344,
    .v_active     = 768,  .v_sync_start = 771,
    .v_sync_end   = 777,  .v_total      = 806,
    .dot_clock_khz = 65000
};

const intel_mode_t INTEL_MODE_1280x720_32 = {
    .width        = 1280, .height       = 720,
    .bpp          = 32,   .pitch        = 1280 * 4,
    .refresh_hz   = 60,
    .h_active     = 1280, .h_sync_start = 1390,
    .h_sync_end   = 1430, .h_total      = 1650,
    .v_active     = 720,  .v_sync_start = 725,
    .v_sync_end   = 730,  .v_total      = 750,
    .dot_clock_khz = 74250
};

const intel_mode_t INTEL_MODE_1920x1080_32 = {
    .width        = 1920, .height       = 1080,
    .bpp          = 32,   .pitch        = 1920 * 4,
    .refresh_hz   = 60,
    .h_active     = 1920, .h_sync_start = 2008,
    .h_sync_end   = 2052, .h_total      = 2200,
    .v_active     = 1080, .v_sync_start = 1084,
    .v_sync_end   = 1089, .v_total      = 1125,
    .dot_clock_khz = 148500
};

/* ============================================================
 * Funções internas auxiliares
 * ============================================================ */

u32 intel_gpu_read32(const intel_gpu_t *gpu, u32 reg) {
    return *((volatile u32 *)(gpu->mmio_base + reg));
}

void intel_gpu_write32(intel_gpu_t *gpu, u32 reg, u32 val) {
    *((volatile u32 *)(gpu->mmio_base + reg)) = val;
    /* Memory barrier implícita em x86, mas bom praticar */
    __asm__ volatile ("" ::: "memory");
}

/** Aguarda até que bits de máscara no registrador atinjam o valor esperado */
static int intel_wait_reg(intel_gpu_t *gpu, u32 reg,
                          u32 mask, u32 expected, u32 timeout_us)
{
    u32 elapsed = 0;
    while (elapsed < timeout_us) {
        if ((intel_gpu_read32(gpu, reg) & mask) == expected)
            return 0;
        udelay(10);
        elapsed += 10;
    }
    return -1; /* timeout */
}

/** Lê o BAR PCI e retorna o endereço físico (64 ou 32 bits) */
static u64 pci_read_bar(u8 bus, u8 slot, u8 func, u8 bar_index) {
    u8 offset = (u8)(PCI_BAR0 + bar_index * 4);
    u32 lo = pci_read32(bus, slot, func, offset);

    /* BAR de memória de 64 bits? */
    if ((lo & 0x7) == 0x4 && bar_index < 5) {
        u32 hi = pci_read32(bus, slot, func, (u8)(offset + 4));
        return ((u64)hi << 32) | (lo & ~0xFu);
    }
    return (u64)(lo & ~0xFu);
}

/* ============================================================
 * Nomes de dispositivos
 * ============================================================ */

const char *intel_gpu_device_name(u16 device_id) {
    switch (device_id) {
        case INTEL_DEV_945GM:        return "Intel GMA 950 (945GM)";
        case INTEL_DEV_945G:         return "Intel GMA 950 (945G)";
        case INTEL_DEV_IRONLAKE:     return "Intel HD Graphics (Ironlake)";
        case INTEL_DEV_IRONLAKE_M:   return "Intel HD Graphics Mobile (Ironlake)";
        case INTEL_DEV_SANDYBRIDGE:  return "Intel HD Graphics 2000/3000 (Sandy Bridge)";
        case INTEL_DEV_SANDYBRIDGE_M:return "Intel HD Graphics 3000 Mobile (Sandy Bridge)";
        case INTEL_DEV_IVYBRIDGE:    return "Intel HD Graphics 2500/4000 (Ivy Bridge)";
        case INTEL_DEV_IVYBRIDGE_M:  return "Intel HD Graphics 4000 Mobile (Ivy Bridge)";
        case INTEL_DEV_HASWELL:      return "Intel HD Graphics 4600 (Haswell)";
        case INTEL_DEV_HASWELL_M:    return "Intel HD Graphics 4600 Mobile (Haswell)";
        case INTEL_DEV_BROADWELL:    return "Intel HD Graphics 5500 (Broadwell)";
        case INTEL_DEV_SKYLAKE_GT2:  return "Intel HD Graphics 520 (Skylake)";
        case INTEL_DEV_SKYLAKE_GT2_2:return "Intel HD Graphics 530 (Skylake)";
        default:                     return "Intel GPU (desconhecido)";
    }
}

/* ============================================================
 * Detecção PCI
 * ============================================================ */

int intel_gpu_detect(intel_gpu_t *gpu) {
    u16 known_devices[] = {
        INTEL_DEV_945GM, INTEL_DEV_945G,
        INTEL_DEV_IRONLAKE, INTEL_DEV_IRONLAKE_M,
        INTEL_DEV_SANDYBRIDGE, INTEL_DEV_SANDYBRIDGE_M,
        INTEL_DEV_IVYBRIDGE, INTEL_DEV_IVYBRIDGE_M,
        INTEL_DEV_HASWELL, INTEL_DEV_HASWELL_M,
        INTEL_DEV_BROADWELL,
        INTEL_DEV_SKYLAKE_GT2, INTEL_DEV_SKYLAKE_GT2_2,
        0
    };

    u8 bus, slot, func;
    for (bus = 0; bus < 8; bus++) {
        for (slot = 0; slot < 32; slot++) {
            for (func = 0; func < 8; func++) {
                u32 id = pci_read32(bus, slot, func, PCI_VENDOR_ID);
                u16 vendor = (u16)(id & 0xFFFF);
                u16 device = (u16)(id >> 16);

                if (vendor != INTEL_VENDOR_ID) continue;

                int i;
                for (i = 0; known_devices[i]; i++) {
                    if (device == known_devices[i]) {
                        gpu->vendor_id  = vendor;
                        gpu->device_id  = device;
                        gpu->pci_bus    = bus;
                        gpu->pci_slot   = slot;
                        gpu->pci_func   = func;
                        return 0;
                    }
                }
            }
        }
    }
    return -1;
}

/* ============================================================
 * Inicialização
 * ============================================================ */

int intel_gpu_init(intel_gpu_t *gpu, u8 bus, u8 slot, u8 func) {
    intel_memset(gpu, 0, sizeof(intel_gpu_t));

    gpu->pci_bus  = bus;
    gpu->pci_slot = slot;
    gpu->pci_func = func;

    /* Lê vendor/device */
    u32 id = pci_read32(bus, slot, func, PCI_VENDOR_ID);
    gpu->vendor_id = (u16)(id & 0xFFFF);
    gpu->device_id = (u16)(id >> 16);

    if (gpu->vendor_id != INTEL_VENDOR_ID)
        return -1;

    /* Habilita Memory Space e Bus Master no PCI */
    u32 cmd = pci_read32(bus, slot, func, PCI_COMMAND);
    cmd |= PCI_CMD_MEMORY | PCI_CMD_MASTER;
    pci_write32(bus, slot, func, PCI_COMMAND, cmd);

    /* BAR0 = MMIO registers */
    u64 mmio_phys = pci_read_bar(bus, slot, func, 0);
    gpu->mmio_size = 2 * 1024 * 1024; /* 2 MB típico */
    gpu->mmio_base = (volatile u8 *)phys_to_virt(mmio_phys, gpu->mmio_size);

    /* BAR2 = Framebuffer (aperture) */
    u64 fb_phys = pci_read_bar(bus, slot, func, 2);
    gpu->fb_phys = fb_phys;
    gpu->fb_size = 256 * 1024 * 1024; /* 256 MB aperture */
    gpu->fb_base = (u8 *)phys_to_virt(fb_phys, gpu->fb_size);

    /* GTT fica no final de BAR0 (offset 0x10000 em gerações antigas) */
    gpu->gtt_base    = (volatile u32 *)(gpu->mmio_base + 0x10000);
    gpu->gtt_entries = 512 * 1024; /* 512k entradas = 2GB de espaço gráfico */

    gpu->pipe        = 0; /* Pipe A por padrão */
    gpu->initialized = 1;

    return 0;
}

/* ============================================================
 * GTT
 * ============================================================ */

int intel_gpu_gtt_map(intel_gpu_t *gpu, u32 gtt_offset,
                      u64 phys_addr, u32 num_pages)
{
    if (!gpu->initialized || !gpu->gtt_base)
        return -1;

    u32 i;
    for (i = 0; i < num_pages; i++) {
        u64 addr = phys_addr + ((u64)i << 12); /* cada página = 4KB */
        u32 entry = (u32)(addr & ~0xFFFu) | INTEL_GTT_VALID | INTEL_GTT_CACHE_LLC;
        gpu->gtt_base[gtt_offset + i] = entry;
    }

    /* Flush de leitura para garantir que as entradas foram escritas */
    (void)intel_gpu_read32(gpu, 0); /* leitura dummy */
    return 0;
}

/* ============================================================
 * Configuração de modo de vídeo
 * ============================================================ */

/** Desabilita pipe e plano antes de reconfigurar */
static void intel_disable_display(intel_gpu_t *gpu) {
    u32 pipe_conf_reg = (gpu->pipe == 0) ? INTEL_REG_PIPEACONF : INTEL_REG_PIPEBCONF;
    u32 plane_reg     = (gpu->pipe == 0) ? INTEL_REG_DSPACNTR  : INTEL_REG_DSPBCNTR;

    /* Desabilita plano primeiro */
    u32 plane = intel_gpu_read32(gpu, plane_reg);
    plane &= ~INTEL_DISPLAY_PLANE_ENABLE;
    intel_gpu_write32(gpu, plane_reg, plane);

    /* Flush do plano (escrita no surface address aciona) */
    u32 surf_reg = (gpu->pipe == 0) ? INTEL_REG_DSPASURF : INTEL_REG_DSPBADDR;
    intel_gpu_write32(gpu, surf_reg, 0);

    udelay(100);

    /* Desabilita pipe */
    u32 conf = intel_gpu_read32(gpu, pipe_conf_reg);
    conf &= ~INTEL_PIPE_ENABLE;
    intel_gpu_write32(gpu, pipe_conf_reg, conf);

    /* Aguarda pipe apagar (até 20ms) */
    intel_wait_reg(gpu, pipe_conf_reg, INTEL_PIPE_STATE, 0, 20000);
}

/** Configura timings de display no pipe selecionado */
static void intel_set_pipe_timings(intel_gpu_t *gpu, const intel_mode_t *m) {
    u32 htotal_reg, vtotal_reg, hsync_reg, vsync_reg, pipesrc_reg;

    if (gpu->pipe == 0) {
        htotal_reg  = INTEL_REG_HTOTAL_A;
        vtotal_reg  = INTEL_REG_VTOTAL_A;
        hsync_reg   = INTEL_REG_HSYNC_A;
        vsync_reg   = INTEL_REG_VSYNC_A;
        pipesrc_reg = INTEL_REG_PIPEASRC;
    } else {
        htotal_reg  = INTEL_REG_HTOTAL_B;
        vtotal_reg  = INTEL_REG_VTOTAL_B;
        hsync_reg   = INTEL_REG_HSYNC_A; /* fallback */
        vsync_reg   = INTEL_REG_VSYNC_A;
        pipesrc_reg = INTEL_REG_PIPEASRC;
    }

    /* HTOTAL: [active-1 | total-1] */
    intel_gpu_write32(gpu, htotal_reg,
        ((m->h_active - 1) << 16) | (m->h_total - 1));

    /* VTOTAL: [active-1 | total-1] */
    intel_gpu_write32(gpu, vtotal_reg,
        ((m->v_active - 1) << 16) | (m->v_total - 1));

    /* HSYNC: [start-1 | end-1] */
    intel_gpu_write32(gpu, hsync_reg,
        ((m->h_sync_start - 1) << 16) | (m->h_sync_end - 1));

    /* VSYNC: [start-1 | end-1] */
    intel_gpu_write32(gpu, vsync_reg,
        ((m->v_sync_start - 1) << 16) | (m->v_sync_end - 1));

    /* PIPESRC: [width-1 | height-1] */
    intel_gpu_write32(gpu, pipesrc_reg,
        ((m->width - 1) << 16) | (m->height - 1));
}

int intel_gpu_set_mode(intel_gpu_t *gpu, const intel_mode_t *mode) {
    if (!gpu->initialized) return -1;

    /* Salva o modo */
    intel_memcpy(&gpu->mode, mode, sizeof(intel_mode_t));

    /* 1. Desabilita display */
    intel_disable_display(gpu);

    /* 2. Configura timings do pipe */
    intel_set_pipe_timings(gpu, mode);

    /* 3. Stride do framebuffer */
    u32 stride_reg = (gpu->pipe == 0) ? INTEL_REG_DSPASTRIDE : INTEL_REG_DSPBSTRIDE;
    intel_gpu_write32(gpu, stride_reg, mode->pitch);

    /* 4. Endereço do framebuffer (offset 0 = início da VRAM) */
    u32 addr_reg = (gpu->pipe == 0) ? INTEL_REG_DSPAADDR : INTEL_REG_DSPBADDR;
    intel_gpu_write32(gpu, addr_reg, 0);

    /* 5. Controle do plano: formato de pixel + enable */
    u32 plane_reg = (gpu->pipe == 0) ? INTEL_REG_DSPACNTR : INTEL_REG_DSPBCNTR;
    u32 plane_ctrl = INTEL_DISPLAY_PLANE_ENABLE;

    switch (mode->bpp) {
        case 16: plane_ctrl |= INTEL_DISPLAY_PLANE_16BPP; break;
        case 24: plane_ctrl |= INTEL_DISPLAY_PLANE_24BPP; break;
        default: plane_ctrl |= INTEL_DISPLAY_PLANE_32BPP; break;
    }
    intel_gpu_write32(gpu, plane_reg, plane_ctrl);

    /* 6. Surface address (aciona o plano) */
    u32 surf_reg = (gpu->pipe == 0) ? INTEL_REG_DSPASURF : INTEL_REG_DSPBADDR;
    intel_gpu_write32(gpu, surf_reg, (u32)gpu->fb_phys);

    /* 7. Habilita pipe */
    u32 pipe_conf_reg = (gpu->pipe == 0) ? INTEL_REG_PIPEACONF : INTEL_REG_PIPEBCONF;
    intel_gpu_write32(gpu, pipe_conf_reg, INTEL_PIPE_ENABLE);

    /* 8. Aguarda pipe ativar */
    if (intel_wait_reg(gpu, pipe_conf_reg, INTEL_PIPE_STATE, INTEL_PIPE_STATE, 20000) != 0)
        return -2; /* timeout ao ativar pipe */

    /* Limpa a tela com preto */
    intel_gpu_clear(gpu, 0x00000000);

    return 0;
}

/* ============================================================
 * Framebuffer direto
 * ============================================================ */

void *intel_gpu_get_framebuffer(intel_gpu_t *gpu) {
    return (void *)gpu->fb_base;
}

void intel_gpu_clear(intel_gpu_t *gpu, u32 color) {
    u32 total = gpu->mode.height * (gpu->mode.pitch / 4);
    u32 *fb = (u32 *)gpu->fb_base;
    u32 i;

    /* Converte RRGGBB -> BGRX (formato Intel) */
    u32 bgrx = ((color & 0xFF) << 16) |
               ((color & 0xFF00)) |
               ((color >> 16) & 0xFF);

    for (i = 0; i < total; i++)
        fb[i] = bgrx;
}

void intel_gpu_put_pixel(intel_gpu_t *gpu, u32 x, u32 y, u32 color) {
    if (x >= gpu->mode.width || y >= gpu->mode.height) return;

    /* Converte RRGGBB -> BGRX */
    u32 bgrx = ((color & 0xFF) << 16) |
               ((color & 0xFF00)) |
               ((color >> 16) & 0xFF);

    u32 offset = y * (gpu->mode.pitch / 4) + x;
    ((u32 *)gpu->fb_base)[offset] = bgrx;
}

void intel_gpu_fill_rect(intel_gpu_t *gpu, u32 x, u32 y,
                         u32 w, u32 h, u32 color)
{
    /* Clipping */
    if (x >= gpu->mode.width)  return;
    if (y >= gpu->mode.height) return;
    if (x + w > gpu->mode.width)  w = gpu->mode.width  - x;
    if (y + h > gpu->mode.height) h = gpu->mode.height - y;

    /* Converte RRGGBB -> BGRX */
    u32 bgrx = ((color & 0xFF) << 16) |
               ((color & 0xFF00)) |
               ((color >> 16) & 0xFF);

    u32 stride = gpu->mode.pitch / 4;
    u32 row, col;
    for (row = 0; row < h; row++) {
        u32 *line = (u32 *)gpu->fb_base + (y + row) * stride + x;
        for (col = 0; col < w; col++)
            line[col] = bgrx;
    }
}

void intel_gpu_blit(intel_gpu_t *gpu, const void *src,
                    u32 dst_x, u32 dst_y, u32 width, u32 height)
{
    if (!src) return;
    if (dst_x >= gpu->mode.width || dst_y >= gpu->mode.height) return;

    u32 stride_dst = gpu->mode.pitch / 4;
    u32 stride_src = width; /* assume source packed */
    const u32 *s = (const u32 *)src;
    u32 row;

    /* Clipping horizontal */
    if (dst_x + width  > gpu->mode.width)  width  = gpu->mode.width  - dst_x;
    if (dst_y + height > gpu->mode.height) height = gpu->mode.height - dst_y;

    for (row = 0; row < height; row++) {
        u32 *d = (u32 *)gpu->fb_base + (dst_y + row) * stride_dst + dst_x;
        intel_memcpy(d, s + row * stride_src, width * 4);
    }
}

/* ============================================================
 * Shutdown
 * ============================================================ */

void intel_gpu_shutdown(intel_gpu_t *gpu) {
    if (!gpu->initialized) return;
    intel_disable_display(gpu);
    gpu->initialized = 0;
}
