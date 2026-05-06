 /**

 * nvidiagpu.c - Implementação do driver NVIDIA GPU para SO próprio

 * Compatível com GeForce 6/7 (NV40) até RTX 30xx (Ampere)

 *

 * Referências:

 *  - envytools (https://envytools.readthedocs.io)

 *  - Linux nouveau driver (drivers/gpu/drm/nouveau)

 *  - NVIDIA open-gpu-kernel-modules (Turing+)

 */


#include "nvidiagpu.h"


/* ============================================================

 * Stubs do kernel — substitua pelas suas implementações

 * ============================================================ */


static inline u32 pci_read32(u8 bus, u8 slot, u8 func, u8 offset) {

    u32 addr = (1u << 31) |

               ((u32)bus  << 16) |

               ((u32)slot << 11) |

               ((u32)func <<  8) |

               (offset & 0xFC);

    (void)addr;

    /* outl(addr, 0xCF8); return inl(0xCFC); */

    return 0;

}


static inline void pci_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 val) {

    u32 addr = (1u << 31) |

               ((u32)bus  << 16) |

               ((u32)slot << 11) |

               ((u32)func <<  8) |

               (offset & 0xFC);

    (void)addr; (void)val;

    /* outl(addr, 0xCF8); outl(val, 0xCFC); */

}


static inline void *phys_to_virt(u64 phys, u64 size) {

    (void)size;

    return (void *)(uintptr_t)phys; /* identity map — ajuste pro seu kernel */

}


static inline void udelay(u32 us) {

    volatile u32 i;

    for (i = 0; i < us * 1000; i++) __asm__ volatile ("nop");

}


static void *nv_memset(void *dst, int val, size_t n) {

    u8 *p = (u8 *)dst;

    while (n--) *p++ = (u8)val;

    return dst;

}


static void *nv_memcpy(void *dst, const void *src, size_t n) {

    u8 *d = (u8 *)dst;

    const u8 *s = (const u8 *)src;

    while (n--) *d++ = *s++;

    return dst;

}


/* ============================================================

 * Offsets PCI config space

 * ============================================================ */

#define PCI_VENDOR_ID   0x00

#define PCI_DEVICE_ID   0x02

#define PCI_COMMAND     0x04

#define PCI_BAR0        0x10

#define PCI_BAR1        0x14

#define PCI_BAR2        0x18

#define PCI_BAR3        0x1C


#define PCI_CMD_MEMORY  (1 << 1)

#define PCI_CMD_MASTER  (1 << 2)


/* ============================================================

 * Modos pré-definidos

 * ============================================================ */


const nvidia_mode_t NVIDIA_MODE_640x480_32 = {

    .width=640,  .height=480,  .bpp=32, .pitch=640*4,  .refresh_hz=60,

    .h_display=640,  .h_sync_start=656,  .h_sync_end=752,  .h_total=800,

    .v_display=480,  .v_sync_start=490,  .v_sync_end=492,  .v_total=525,

    .dot_clock_khz=25175

};


const nvidia_mode_t NVIDIA_MODE_800x600_32 = {

    .width=800,  .height=600,  .bpp=32, .pitch=800*4,  .refresh_hz=60,

    .h_display=800,  .h_sync_start=840,  .h_sync_end=968,  .h_total=1056,

    .v_display=600,  .v_sync_start=601,  .v_sync_end=605,  .v_total=628,

    .dot_clock_khz=40000

};


const nvidia_mode_t NVIDIA_MODE_1024x768_32 = {

    .width=1024, .height=768,  .bpp=32, .pitch=1024*4, .refresh_hz=60,

    .h_display=1024, .h_sync_start=1048, .h_sync_end=1184, .h_total=1344,

    .v_display=768,  .v_sync_start=771,  .v_sync_end=777,  .v_total=806,

    .dot_clock_khz=65000

};


const nvidia_mode_t NVIDIA_MODE_1280x720_32 = {

    .width=1280, .height=720,  .bpp=32, .pitch=1280*4, .refresh_hz=60,

    .h_display=1280, .h_sync_start=1390, .h_sync_end=1430, .h_total=1650,

    .v_display=720,  .v_sync_start=725,  .v_sync_end=730,  .v_total=750,

    .dot_clock_khz=74250

};


const nvidia_mode_t NVIDIA_MODE_1920x1080_32 = {

    .width=1920, .height=1080, .bpp=32, .pitch=1920*4, .refresh_hz=60,

    .h_display=1920, .h_sync_start=2008, .h_sync_end=2052, .h_total=2200,

    .v_display=1080, .v_sync_start=1084, .v_sync_end=1089, .v_total=1125,

    .dot_clock_khz=148500

};


/* ============================================================

 * Leitura / escrita MMIO

 * ============================================================ */


u32 nvidia_gpu_read32(const nvidia_gpu_t *gpu, u32 reg) {

    return *((volatile u32 *)(gpu->mmio_base + reg));

}


void nvidia_gpu_write32(nvidia_gpu_t *gpu, u32 reg, u32 val) {

    *((volatile u32 *)(gpu->mmio_base + reg)) = val;

    __asm__ volatile ("" ::: "memory");

}


/** Aguarda bit de máscara em registrador (timeout em us) */

static int nv_wait_reg(nvidia_gpu_t *gpu, u32 reg,

                       u32 mask, u32 expected, u32 timeout_us)

{

    u32 elapsed = 0;

    while (elapsed < timeout_us) {

        if ((nvidia_gpu_read32(gpu, reg) & mask) == expected)

            return 0;

        udelay(10);

        elapsed += 10;

    }

    return -1;

}


/** Lê BAR PCI (suporte a 64 bits) */

static u64 pci_read_bar(u8 bus, u8 slot, u8 func, u8 bar_index) {

    u8 offset = (u8)(PCI_BAR0 + bar_index * 4);

    u32 lo = pci_read32(bus, slot, func, offset);

    if ((lo & 0x7) == 0x4 && bar_index < 5) {

        u32 hi = pci_read32(bus, slot, func, (u8)(offset + 4));

        return ((u64)hi << 32) | (lo & ~0xFu);

    }

    return (u64)(lo & ~0xFu);

}


/* ============================================================

 * Detecção de arquitetura

 * ============================================================ */


nvidia_arch_t nvidia_gpu_detect_arch(u16 device_id) {

    /* NVIDIA codifica a geração no nibble alto do device ID */

    u8 arch_id = (u8)((device_id >> 12) & 0xF);


    switch (arch_id) {

        case 0x0:

            /* Faixa 0x0xxx — distingue pelo nibble seguinte */

            if (device_id >= 0x0040 && device_id <= 0x00FF) return NVIDIA_ARCH_NV40;

            if (device_id >= 0x0100 && device_id <= 0x01FF) return NVIDIA_ARCH_NV40;

            if (device_id >= 0x0190 && device_id <= 0x019F) return NVIDIA_ARCH_TESLA;

            if (device_id >= 0x06C0 && device_id <= 0x06FF) return NVIDIA_ARCH_FERMI;

            if (device_id >= 0x0800 && device_id <= 0x08FF) return NVIDIA_ARCH_FERMI;

            if (device_id >= 0x0900 && device_id <= 0x09FF) return NVIDIA_ARCH_TESLA;

            return NVIDIA_ARCH_TESLA;

        case 0x1:

            if (device_id >= 0x1000 && device_id <= 0x107F) return NVIDIA_ARCH_FERMI;

            if (device_id >= 0x1180 && device_id <= 0x11BF) return NVIDIA_ARCH_KEPLER;

            if (device_id >= 0x1200 && device_id <= 0x127F) return NVIDIA_ARCH_FERMI;

            if (device_id >= 0x1280 && device_id <= 0x12FF) return NVIDIA_ARCH_KEPLER;

            if (device_id >= 0x13C0 && device_id <= 0x13FF) return NVIDIA_ARCH_MAXWELL;

            if (device_id >= 0x1400 && device_id <= 0x147F) return NVIDIA_ARCH_MAXWELL;

            if (device_id >= 0x1B00 && device_id <= 0x1BFF) return NVIDIA_ARCH_PASCAL;

            if (device_id >= 0x1C00 && device_id <= 0x1CFF) return NVIDIA_ARCH_PASCAL;

            if (device_id >= 0x1D00 && device_id <= 0x1DFF) return NVIDIA_ARCH_PASCAL;

            if (device_id >= 0x1E00 && device_id <= 0x1EFF) return NVIDIA_ARCH_TURING;

            if (device_id >= 0x1F00 && device_id <= 0x1FFF) return NVIDIA_ARCH_TURING;

            return NVIDIA_ARCH_KEPLER;

        case 0x2:

            if (device_id >= 0x2000 && device_id <= 0x20FF) return NVIDIA_ARCH_AMPERE;

            if (device_id >= 0x2100 && device_id <= 0x21FF) return NVIDIA_ARCH_TURING;

            if (device_id >= 0x2200 && device_id <= 0x22FF) return NVIDIA_ARCH_AMPERE;

            if (device_id >= 0x2400 && device_id <= 0x24FF) return NVIDIA_ARCH_AMPERE;

            if (device_id >= 0x2500 && device_id <= 0x25FF) return NVIDIA_ARCH_AMPERE;

            return NVIDIA_ARCH_AMPERE;

        default:

            return NVIDIA_ARCH_UNKNOWN;

    }

}


const char *nvidia_arch_name(nvidia_arch_t arch) {

    switch (arch) {

        case NVIDIA_ARCH_NV40:    return "NV40 (Curie / GeForce 6-7)";

        case NVIDIA_ARCH_TESLA:   return "Tesla (GeForce 8/9/200)";

        case NVIDIA_ARCH_FERMI:   return "Fermi (GeForce 400/500)";

        case NVIDIA_ARCH_KEPLER:  return "Kepler (GeForce 600/700)";

        case NVIDIA_ARCH_MAXWELL: return "Maxwell (GeForce 900)";

        case NVIDIA_ARCH_PASCAL:  return "Pascal (GeForce 10xx)";

        case NVIDIA_ARCH_TURING:  return "Turing (GeForce 16xx / RTX 20xx)";

        case NVIDIA_ARCH_AMPERE:  return "Ampere (RTX 30xx)";

        default:                  return "Desconhecido";

    }

}


const char *nvidia_gpu_device_name(u16 device_id) {

    switch (device_id) {

        case NVIDIA_DEV_GF6600:      return "GeForce 6600";

        case NVIDIA_DEV_GF6800:      return "GeForce 6800";

        case NVIDIA_DEV_GF7600:      return "GeForce 7600 GT";

        case NVIDIA_DEV_GF7900:      return "GeForce 7900 GTX";

        case NVIDIA_DEV_GF8800GTX:   return "GeForce 8800 GTX";

        case NVIDIA_DEV_GF8800GT:    return "GeForce 8800 GT";

        case NVIDIA_DEV_GF9800GT:    return "GeForce 9800 GT";

        case NVIDIA_DEV_GF9600GT:    return "GeForce 9600 GT";

        case NVIDIA_DEV_GT240:       return "GeForce GT 240";

        case NVIDIA_DEV_GTX480:      return "GeForce GTX 480";

        case NVIDIA_DEV_GTX580:      return "GeForce GTX 580";

        case NVIDIA_DEV_GTX560TI:    return "GeForce GTX 560 Ti";

        case NVIDIA_DEV_GTX680:      return "GeForce GTX 680";

        case NVIDIA_DEV_GTX780:      return "GeForce GTX 780";

        case NVIDIA_DEV_GTX780TI:    return "GeForce GTX 780 Ti";

        case NVIDIA_DEV_GT730:       return "GeForce GT 730";

        case NVIDIA_DEV_GT710:       return "GeForce GT 710";

        case NVIDIA_DEV_GTX980:      return "GeForce GTX 980";

        case NVIDIA_DEV_GTX970:      return "GeForce GTX 970";

        case NVIDIA_DEV_GTX960:      return "GeForce GTX 960";

        case NVIDIA_DEV_GTX950:      return "GeForce GTX 950";

        case NVIDIA_DEV_GTX1080TI:   return "GeForce GTX 1080 Ti";

        case NVIDIA_DEV_GTX1080:     return "GeForce GTX 1080";

        case NVIDIA_DEV_GTX1070:     return "GeForce GTX 1070";

        case NVIDIA_DEV_GTX1060_6G:  return "GeForce GTX 1060 6GB";

        case NVIDIA_DEV_GTX1050TI:   return "GeForce GTX 1050 Ti";

        case NVIDIA_DEV_GTX1050:     return "GeForce GTX 1050";

        case NVIDIA_DEV_RTX2080TI:   return "GeForce RTX 2080 Ti";

        case NVIDIA_DEV_RTX2080:     return "GeForce RTX 2080";

        case NVIDIA_DEV_RTX2070:     return "GeForce RTX 2070";

        case NVIDIA_DEV_GTX1660TI:   return "GeForce GTX 1660 Ti";

        case NVIDIA_DEV_GTX1650:     return "GeForce GTX 1650";

        case NVIDIA_DEV_RTX3090:     return "GeForce RTX 3090";

        case NVIDIA_DEV_RTX3080:     return "GeForce RTX 3080";

        case NVIDIA_DEV_RTX3070:     return "GeForce RTX 3070";

        case NVIDIA_DEV_RTX3060TI:   return "GeForce RTX 3060 Ti";

        case NVIDIA_DEV_RTX3060:     return "GeForce RTX 3060";

        default:                     return "NVIDIA GPU (desconhecido)";

    }

}


/* ============================================================

 * Detecção PCI

 * ============================================================ */


int nvidia_gpu_detect(nvidia_gpu_t *gpu) {

    u8 bus, slot, func;

    for (bus = 0; bus < 16; bus++) {

        for (slot = 0; slot < 32; slot++) {

            for (func = 0; func < 8; func++) {

                u32 id     = pci_read32(bus, slot, func, PCI_VENDOR_ID);

                u16 vendor = (u16)(id & 0xFFFF);

                u16 device = (u16)(id >> 16);


                if (vendor == 0xFFFF) continue; /* slot vazio */

                if (vendor != NVIDIA_VENDOR_ID) continue;


                /* Qualquer device NVIDIA com arquitetura conhecida */

                nvidia_arch_t arch = nvidia_gpu_detect_arch(device);

                if (arch == NVIDIA_ARCH_UNKNOWN) continue;


                gpu->vendor_id = vendor;

                gpu->device_id = device;

                gpu->pci_bus   = bus;

                gpu->pci_slot  = slot;

                gpu->pci_func  = func;

                return 0;

            }

        }

    }

    return -1;

}


/* ============================================================

 * Inicialização

 * ============================================================ */


int nvidia_gpu_init(nvidia_gpu_t *gpu, u8 bus, u8 slot, u8 func) {

    nv_memset(gpu, 0, sizeof(nvidia_gpu_t));


    gpu->pci_bus  = bus;

    gpu->pci_slot = slot;

    gpu->pci_func = func;


    u32 id = pci_read32(bus, slot, func, PCI_VENDOR_ID);

    gpu->vendor_id = (u16)(id & 0xFFFF);

    gpu->device_id = (u16)(id >> 16);


    if (gpu->vendor_id != NVIDIA_VENDOR_ID)

        return -1;


    /* Detecta arquitetura */

    gpu->arch = nvidia_gpu_detect_arch(gpu->device_id);


    /* Habilita Memory Space e Bus Master */

    u32 cmd = pci_read32(bus, slot, func, PCI_COMMAND);

    cmd |= PCI_CMD_MEMORY | PCI_CMD_MASTER;

    pci_write32(bus, slot, func, PCI_COMMAND, cmd);


    /* BAR0 = MMIO (16MB em GPUs modernas, 32MB em algumas) */

    u64 mmio_phys   = pci_read_bar(bus, slot, func, 0);

    gpu->mmio_size  = 32 * 1024 * 1024; /* 32 MB máximo seguro */

    gpu->mmio_base  = (volatile u8 *)phys_to_virt(mmio_phys, gpu->mmio_size);


    /* BAR1 = Framebuffer aperture (VRAM mapeada) */

    u64 fb_phys     = pci_read_bar(bus, slot, func, 1);

    gpu->fb_phys    = fb_phys;

    gpu->fb_size    = 256 * 1024 * 1024; /* 256 MB aperture */

    gpu->fb_base    = (u8 *)phys_to_virt(fb_phys, gpu->fb_size);


    /* RAMIN (BAR3 em NV40/Tesla, dentro do BAR0 em Fermi+) */

    if (gpu->arch <= NVIDIA_ARCH_TESLA) {

        u64 ramin_phys   = pci_read_bar(bus, slot, func, 3);

        gpu->ramin_size  = 16 * 1024 * 1024;

        gpu->ramin_base  = (volatile u8 *)phys_to_virt(ramin_phys, gpu->ramin_size);

    } else {

        /* Fermi+: RAMIN é acessado via BAR0 offset 0x700000 */

        gpu->ramin_base  = gpu->mmio_base + NVIDIA_REG_PRAMIN_BASE;

        gpu->ramin_size  = 1 * 1024 * 1024;

    }


    /* Define se usa EVO display engine (GeForce 8+) */

    gpu->use_nv50_display = (gpu->arch >= NVIDIA_ARCH_TESLA) ? 1 : 0;


    gpu->head        = 0;

    gpu->initialized = 1;


    /* Verifica chip ID via BOOT0 */

    u32 boot0 = nvidia_gpu_read32(gpu, NVIDIA_REG_PMC_BOOT0);

    (void)boot0; /* pode usar para verificação extra */


    /* Habilita engines básicos via PMC_ENABLE */

    u32 pmc_en = NVIDIA_PMC_ENABLE_PGRAPH  |

                 NVIDIA_PMC_ENABLE_PFIFO   |

                 NVIDIA_PMC_ENABLE_PTIMER  |

                 NVIDIA_PMC_ENABLE_PFB     |

                 NVIDIA_PMC_ENABLE_PCRTC   |

                 NVIDIA_PMC_ENABLE_PRAMDAC;

    nvidia_gpu_write32(gpu, NVIDIA_REG_PMC_ENABLE, pmc_en);

    udelay(100);


    return 0;

}


/* ============================================================

 * Configuração de modo — NV40 / Tesla legacy (PRAMDAC)

 * ============================================================ */


/**

 * Calcula coeficientes do PLL de vídeo para o clock desejado.

 * Formula: dot_clock = (N * reference_clock) / (M * P)

 * Reference clock NVIDIA: ~27000 kHz

 */

static void nv_calc_pll(u32 clock_khz, u32 *m_out, u32 *n_out, u32 *p_out) {

    const u32 ref_clock = 27000; /* kHz */

    u32 best_m = 1, best_n = 1, best_p = 1;

    u32 best_diff = 0xFFFFFFFF;

    u32 m, n, p;


    for (p = 1; p <= 8; p++) {

        for (m = 1; m <= 13; m++) {

            n = (clock_khz * m * p) / ref_clock;

            if (n < 1 || n > 255) continue;

            u32 actual = (ref_clock * n) / (m * p);

            u32 diff = (actual > clock_khz) ?

                       (actual - clock_khz) : (clock_khz - actual);

            if (diff < best_diff) {

                best_diff = diff;

                best_m = m; best_n = n; best_p = p;

            }

        }

    }

    *m_out = best_m;

    *n_out = best_n;

    *p_out = best_p;

}


static int nv_set_mode_legacy(nvidia_gpu_t *gpu, const nvidia_mode_t *m) {

    /* 1. Configura PLL de vídeo (VPLL) */

    u32 pll_m, pll_n, pll_p;

    nv_calc_pll(m->dot_clock_khz, &pll_m, &pll_n, &pll_p);


    /* Coef: [P(7:4) | M(12:8) | N(23:16)] */

    u32 vpll = ((pll_p - 1) << 16) | (pll_n << 8) | pll_m;

    u32 vpll_reg = (gpu->head == 0) ?

                   NVIDIA_REG_PRAMDAC_VPLL_COEFF :

                   NVIDIA_REG_PRAMDAC_VPLL2_COEFF;

    nvidia_gpu_write32(gpu, vpll_reg, vpll);

    udelay(200);


    /* 2. Configura timings via CRTC (legado VGA + NVIDIA extension)

     * Nota: NVIDIA herda os registradores CRTC do VGA para NV04-NV4x.

     * Em um SO próprio sem VGA, use os registradores PCRTC direto. */


    /* PCRTC_START: endereço físico do framebuffer */

    nvidia_gpu_write32(gpu, NVIDIA_REG_PCRTC_START, (u32)gpu->fb_phys);


    /* 3. Stride */

    /* Em NV40 o stride fica em um registrador CRTC estendido.

     * Como não temos acesso ao CRTC legado aqui, configuramos o

     * mínimo necessário via registrador de superfície. */

    u32 stride_reg = NVIDIA_REG_PCRTC_CONFIG;

    nvidia_gpu_write32(gpu, stride_reg, m->pitch);


    udelay(100);

    return 0;

}


/* ============================================================

 * Configuração de modo — NV50+ (EVO display engine)

 * ============================================================ */


static int nv_set_mode_evo(nvidia_gpu_t *gpu, const nvidia_mode_t *m) {

    u32 head_off = (gpu->head == 0) ? 0 : 0x40; /* offset entre heads */


    /* 1. Configura timings do head */

    /* HTOTAL: [h_total-1 | h_display-1] */

    nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_HTOTAL + head_off,

        ((m->h_total - 1) << 16) | (m->h_display - 1));


    /* VTOTAL: [v_total-1 | v_display-1] */

    nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_VTOTAL + head_off,

        ((m->v_total - 1) << 16) | (m->v_display - 1));


    /* HSYNC: [h_sync_end | h_sync_start] */

    nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_HSYNC + head_off,

        ((m->h_sync_end - 1) << 16) | (m->h_sync_start - 1));


    /* VSYNC: [v_sync_end | v_sync_start] */

    nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_VSYNC + head_off,

        ((m->v_sync_end - 1) << 16) | (m->v_sync_start - 1));


    /* SIZE: [width-1 | height-1] */

    nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_SIZE + head_off,

        ((m->width - 1) << 16) | (m->height - 1));


    /* 2. Configura o framebuffer do head */

    nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_FB_ADDR + head_off,

        (u32)(gpu->fb_phys & 0xFFFFFFFF));


    nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_FB_PITCH + head_off,

        m->pitch);


    /* 3. Habilita o head */

    nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_CTRL + head_off, 1);


    /* 4. Aguarda display engine ficar ativo (até 50ms) */

    nv_wait_reg(gpu, NVIDIA_REG_EVO_CTRL, 0x80000000, 0, 50000);


    udelay(100);

    return 0;

}


int nvidia_gpu_set_mode(nvidia_gpu_t *gpu, const nvidia_mode_t *mode) {

    if (!gpu->initialized) return -1;


    nv_memcpy(&gpu->mode, mode, sizeof(nvidia_mode_t));


    if (gpu->use_nv50_display)

        return nv_set_mode_evo(gpu, mode);

    else

        return nv_set_mode_legacy(gpu, mode);

}


/* ============================================================

 * Framebuffer direto

 * ============================================================ */


void *nvidia_gpu_get_framebuffer(nvidia_gpu_t *gpu) {

    return (void *)gpu->fb_base;

}


void nvidia_gpu_clear(nvidia_gpu_t *gpu, u32 color) {

    u32 total = gpu->mode.height * (gpu->mode.pitch / 4);

    u32 *fb = (u32 *)gpu->fb_base;

    u32 i;


    /* NVIDIA usa ARGB nativamente (já é RRGGBB direto) */

    for (i = 0; i < total; i++)

        fb[i] = color;

}


void nvidia_gpu_put_pixel(nvidia_gpu_t *gpu, u32 x, u32 y, u32 color) {

    if (x >= gpu->mode.width || y >= gpu->mode.height) return;

    u32 offset = y * (gpu->mode.pitch / 4) + x;

    ((u32 *)gpu->fb_base)[offset] = color;

}


void nvidia_gpu_fill_rect(nvidia_gpu_t *gpu, u32 x, u32 y,

                          u32 w, u32 h, u32 color)

{

    if (x >= gpu->mode.width)  return;

    if (y >= gpu->mode.height) return;

    if (x + w > gpu->mode.width)  w = gpu->mode.width  - x;

    if (y + h > gpu->mode.height) h = gpu->mode.height - y;


    u32 stride = gpu->mode.pitch / 4;

    u32 row, col;

    for (row = 0; row < h; row++) {

        u32 *line = (u32 *)gpu->fb_base + (y + row) * stride + x;

        for (col = 0; col < w; col++)

            line[col] = color;

    }

}


void nvidia_gpu_blit(nvidia_gpu_t *gpu, const void *src,

                     u32 dst_x, u32 dst_y, u32 width, u32 height)

{

    if (!src) return;

    if (dst_x >= gpu->mode.width || dst_y >= gpu->mode.height) return;

    if (dst_x + width  > gpu->mode.width)  width  = gpu->mode.width  - dst_x;

    if (dst_y + height > gpu->mode.height) height = gpu->mode.height - dst_y;


    u32 stride_dst = gpu->mode.pitch / 4;

    const u32 *s = (const u32 *)src;

    u32 row;

    for (row = 0; row < height; row++) {

        u32 *d = (u32 *)gpu->fb_base + (dst_y + row) * stride_dst + dst_x;

        nv_memcpy(d, s + row * width, width * 4);

    }

}


/* ============================================================

 * Shutdown

 * ============================================================ */


void nvidia_gpu_shutdown(nvidia_gpu_t *gpu) {

    if (!gpu->initialized) return;


    if (gpu->use_nv50_display) {

        /* Desabilita head EVO */

        u32 head_off = (gpu->head == 0) ? 0 : 0x40;

        nvidia_gpu_write32(gpu, NVIDIA_REG_HEAD0_CTRL + head_off, 0);

    } else {

        /* Desabilita PCRTC legacy */

        nvidia_gpu_write32(gpu, NVIDIA_REG_PCRTC_CONFIG, 0);

    }


    /* Desabilita engines */

    nvidia_gpu_write32(gpu, NVIDIA_REG_PMC_ENABLE, 0);


    gpu->initialized = 0;

}
