/**
 * nvidiagpu.h - Driver para GPU NVIDIA para SO próprio
 * Compatível com NVIDIA GeForce série 6/7/8/9/200/300/400/500/600/700/900/10xx
 * Acesso direto via MMIO (BAR0) e framebuffer (BAR1)
 *
 * Referências:
 *  - envytools / nouveau (engenharia reversa oficial da comunidade)
 *  - nouveau driver (Linux kernel drivers/gpu/drm/nouveau)
 *  - NVIDIA open-gpu-kernel-modules (Turing+)
 */

#ifndef NVIDIAGPU_H
#define NVIDIAGPU_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * Tipos básicos
 * ============================================================ */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;

/* ============================================================
 * Vendor ID
 * ============================================================ */
#define NVIDIA_VENDOR_ID            0x10DE

/* ============================================================
 * Device IDs NVIDIA comuns
 * ============================================================ */

/* GeForce 6/7 (NV40/NV50 - Curie) */
#define NVIDIA_DEV_GF6600           0x0140  /* GeForce 6600 */
#define NVIDIA_DEV_GF6800           0x0041  /* GeForce 6800 */
#define NVIDIA_DEV_GF7600           0x0391  /* GeForce 7600 GT */
#define NVIDIA_DEV_GF7900           0x0290  /* GeForce 7900 GTX */

/* GeForce 8/9/200 (Tesla - G80/G90/GT200) */
#define NVIDIA_DEV_GF8800GTX        0x0191  /* GeForce 8800 GTX */
#define NVIDIA_DEV_GF8800GT         0x0612  /* GeForce 8800 GT */
#define NVIDIA_DEV_GF9800GT         0x0614  /* GeForce 9800 GT */
#define NVIDIA_DEV_GF9600GT         0x0622  /* GeForce 9600 GT */
#define NVIDIA_DEV_GT240            0x0CA3  /* GeForce GT 240 */

/* GeForce 400/500 (Fermi - GF100/GF110) */
#define NVIDIA_DEV_GTX480           0x06C0  /* GeForce GTX 480 */
#define NVIDIA_DEV_GTX580           0x1080  /* GeForce GTX 580 */
#define NVIDIA_DEV_GTX560TI         0x1200  /* GeForce GTX 560 Ti */

/* GeForce 600/700 (Kepler - GK104/GK110) */
#define NVIDIA_DEV_GTX680           0x1180  /* GeForce GTX 680 */
#define NVIDIA_DEV_GTX780           0x1004  /* GeForce GTX 780 */
#define NVIDIA_DEV_GTX780TI         0x100A  /* GeForce GTX 780 Ti */
#define NVIDIA_DEV_GT730            0x1287  /* GeForce GT 730 */
#define NVIDIA_DEV_GT710            0x128B  /* GeForce GT 710 */

/* GeForce 900 (Maxwell - GM200/GM204) */
#define NVIDIA_DEV_GTX980           0x13C0  /* GeForce GTX 980 */
#define NVIDIA_DEV_GTX970           0x13C2  /* GeForce GTX 970 */
#define NVIDIA_DEV_GTX960           0x1401  /* GeForce GTX 960 */
#define NVIDIA_DEV_GTX950           0x1402  /* GeForce GTX 950 */

/* GeForce 10xx (Pascal - GP102/GP104/GP106) */
#define NVIDIA_DEV_GTX1080TI        0x1B06  /* GeForce GTX 1080 Ti */
#define NVIDIA_DEV_GTX1080          0x1B80  /* GeForce GTX 1080 */
#define NVIDIA_DEV_GTX1070          0x1B81  /* GeForce GTX 1070 */
#define NVIDIA_DEV_GTX1060_6G       0x1C03  /* GeForce GTX 1060 6GB */
#define NVIDIA_DEV_GTX1050TI        0x1C82  /* GeForce GTX 1050 Ti */
#define NVIDIA_DEV_GTX1050          0x1C81  /* GeForce GTX 1050 */

/* GeForce 16xx / RTX 20xx (Turing - TU102/TU104/TU116) */
#define NVIDIA_DEV_RTX2080TI        0x1E04  /* GeForce RTX 2080 Ti */
#define NVIDIA_DEV_RTX2080          0x1E82  /* GeForce RTX 2080 */
#define NVIDIA_DEV_RTX2070          0x1F02  /* GeForce RTX 2070 */
#define NVIDIA_DEV_GTX1660TI        0x2182  /* GeForce GTX 1660 Ti */
#define NVIDIA_DEV_GTX1650          0x1F82  /* GeForce GTX 1650 */

/* GeForce RTX 30xx (Ampere - GA102/GA104) */
#define NVIDIA_DEV_RTX3090          0x2204  /* GeForce RTX 3090 */
#define NVIDIA_DEV_RTX3080          0x2206  /* GeForce RTX 3080 */
#define NVIDIA_DEV_RTX3070          0x2484  /* GeForce RTX 3070 */
#define NVIDIA_DEV_RTX3060TI        0x2486  /* GeForce RTX 3060 Ti */
#define NVIDIA_DEV_RTX3060          0x2503  /* GeForce RTX 3060 */

/* Quadro / profissional */
#define NVIDIA_DEV_QUADRO_RTX4000   0x1EB1
#define NVIDIA_DEV_QUADRO_P2000     0x1C31

/* ============================================================
 * Arquiteturas (geração interna)
 * ============================================================ */
typedef enum {
    NVIDIA_ARCH_UNKNOWN  = 0,
    NVIDIA_ARCH_NV40     = 4,   /* GeForce 6/7 */
    NVIDIA_ARCH_TESLA    = 5,   /* GeForce 8/9/200 */
    NVIDIA_ARCH_FERMI    = 6,   /* GeForce 400/500 */
    NVIDIA_ARCH_KEPLER   = 7,   /* GeForce 600/700 */
    NVIDIA_ARCH_MAXWELL  = 8,   /* GeForce 900 */
    NVIDIA_ARCH_PASCAL   = 9,   /* GeForce 10xx */
    NVIDIA_ARCH_TURING   = 10,  /* GeForce 16xx / RTX 20xx */
    NVIDIA_ARCH_AMPERE   = 11,  /* RTX 30xx */
} nvidia_arch_t;

/* ============================================================
 * Registradores MMIO NVIDIA (BAR0)
 * Baseados em envytools / nouveau
 * ============================================================ */

/* Boot / identificação */
#define NVIDIA_REG_BOOT0            0x000000  /* Chip ID e revisão */
#define NVIDIA_REG_BOOT1            0x000004
#define NVIDIA_REG_CHIPSET          0x000000  /* Alias de BOOT0 */

/* PMC - Master Control */
#define NVIDIA_REG_PMC_BOOT0        0x000000  /* Chip ID */
#define NVIDIA_REG_PMC_ENABLE       0x000200  /* Habilita engines */
#define NVIDIA_REG_PMC_INTR0        0x000100  /* Interrupções */
#define NVIDIA_REG_PMC_INTR_EN0     0x000140

/* PBUS - PCI/Bus */
#define NVIDIA_REG_PBUS_DEBUG0      0x001084

/* PFIFO - Submissão de comandos */
#define NVIDIA_REG_PFIFO_INTR0      0x002100
#define NVIDIA_REG_PFIFO_INTR_EN0   0x002140
#define NVIDIA_REG_PFIFO_RAMHT      0x002210  /* Hash table */
#define NVIDIA_REG_PFIFO_RAMFC      0x002214  /* FIFO context */
#define NVIDIA_REG_PFIFO_CACHES     0x002500
#define NVIDIA_REG_PFIFO_CACHE0_PUSH0   0x003000
#define NVIDIA_REG_PFIFO_CACHE1_PUSH0   0x003200
#define NVIDIA_REG_PFIFO_CACHE1_DMA_PUT 0x003240
#define NVIDIA_REG_PFIFO_CACHE1_DMA_GET 0x003244

/* PGRAPH - Engine gráfica */
#define NVIDIA_REG_PGRAPH_STATUS    0x400700
#define NVIDIA_REG_PGRAPH_INTR      0x400100
#define NVIDIA_REG_PGRAPH_INTR_EN   0x400140
#define NVIDIA_REG_PGRAPH_CTX_CTRL  0x400144

/* PCRTC / Display (legacy NV04-NV4x) */
#define NVIDIA_REG_PCRTC_START      0x600800  /* Endereço do framebuffer */
#define NVIDIA_REG_PCRTC_CONFIG     0x600804
#define NVIDIA_REG_PCRTC_CURSOR     0x600808  /* Endereço do cursor */

/* PRAMDAC - DAC / PLL */
#define NVIDIA_REG_PRAMDAC_VPLL_COEFF   0x680508  /* PLL coeficientes VPLL */
#define NVIDIA_REG_PRAMDAC_VPLL2_COEFF  0x680520
#define NVIDIA_REG_PRAMDAC_GENERAL_CTRL 0x680600
#define NVIDIA_REG_PRAMDAC_SEL_CLK      0x680524

/* PDISPLAY / EVO (GeForce 8+ display engine) */
#define NVIDIA_REG_PDISPLAY_BASE        0x610000
#define NVIDIA_REG_EVO_CTRL             0x610200
#define NVIDIA_REG_EVO_INTR             0x610020
#define NVIDIA_REG_EVO_INTR_EN          0x610024

/* Scanout / Head 0 (NV50+) */
#define NVIDIA_REG_HEAD0_OWNER          0x610110
#define NVIDIA_REG_HEAD0_SYNC_CTRL      0x610180
#define NVIDIA_REG_HEAD0_HTOTAL         0x610184
#define NVIDIA_REG_HEAD0_VTOTAL         0x610188
#define NVIDIA_REG_HEAD0_HSYNC          0x61018C
#define NVIDIA_REG_HEAD0_VSYNC          0x610190
#define NVIDIA_REG_HEAD0_SIZE           0x6101DC  /* Resolução: [w-1 | h-1] */
#define NVIDIA_REG_HEAD0_FB_ADDR        0x6101A4  /* Endereço do buffer */
#define NVIDIA_REG_HEAD0_FB_PITCH       0x6101A8  /* Stride em bytes */
#define NVIDIA_REG_HEAD0_CTRL           0x610300

/* Scanout / Head 1 */
#define NVIDIA_REG_HEAD1_OWNER          0x610150
#define NVIDIA_REG_HEAD1_HTOTAL         0x6101C4
#define NVIDIA_REG_HEAD1_VTOTAL         0x6101C8

/* PFB - Frame Buffer Controller */
#define NVIDIA_REG_PFB_CONFIG0          0x100200
#define NVIDIA_REG_PFB_FIFO_DATA        0x100204
#define NVIDIA_REG_PFB_TILE(n)          (0x100240 + (n)*16)
#define NVIDIA_REG_PFB_TLIMIT(n)        (0x100244 + (n)*16)
#define NVIDIA_REG_PFB_TSIZE(n)         (0x100248 + (n)*16)

/* PRAM / RAMIN */
#define NVIDIA_REG_PRAMIN_BASE          0x700000  /* Offset para RAMIN */

/* Timer */
#define NVIDIA_REG_PTIMER_NUMERATOR     0x009200
#define NVIDIA_REG_PTIMER_DENOMINATOR   0x009210
#define NVIDIA_REG_PTIMER_TIME_0        0x009400
#define NVIDIA_REG_PTIMER_TIME_1        0x009410

/* I2C / DDC para EDID */
#define NVIDIA_REG_PRMVIO_CR_INDEX      0x601374
#define NVIDIA_REG_I2C_BASE             0x00E000

/* ============================================================
 * Bits importantes
 * ============================================================ */

/* PMC_ENABLE */
#define NVIDIA_PMC_ENABLE_PGRAPH        (1 << 12)
#define NVIDIA_PMC_ENABLE_PFIFO         (1 << 8)
#define NVIDIA_PMC_ENABLE_PTIMER        (1 << 16)
#define NVIDIA_PMC_ENABLE_PFB           (1 << 20)
#define NVIDIA_PMC_ENABLE_PCRTC         (1 << 24)
#define NVIDIA_PMC_ENABLE_PRAMDAC       (1 << 28)

/* PCRTC_CONFIG */
#define NVIDIA_PCRTC_CONFIG_START_ADDR  (1 << 0)

/* HEAD SYNC CTRL */
#define NVIDIA_HEAD_HSYNC_POS           (1 << 0)   /* H-sync positivo */
#define NVIDIA_HEAD_VSYNC_POS           (1 << 4)   /* V-sync positivo */

/* PGRAPH STATUS */
#define NVIDIA_PGRAPH_STATUS_IDLE       0x00000000

/* ============================================================
 * Estruturas de dados
 * ============================================================ */

/** Modo de vídeo */
typedef struct {
    u32 width;
    u32 height;
    u32 bpp;
    u32 pitch;       /* bytes por linha */
    u32 refresh_hz;

    /* Timings horizontais */
    u32 h_display;
    u32 h_sync_start;
    u32 h_sync_end;
    u32 h_total;

    /* Timings verticais */
    u32 v_display;
    u32 v_sync_start;
    u32 v_sync_end;
    u32 v_total;

    u32 dot_clock_khz;
} nvidia_mode_t;

/** Estado interno do driver */
typedef struct {
    /* PCI */
    u16 vendor_id;
    u16 device_id;
    u8  pci_bus;
    u8  pci_slot;
    u8  pci_func;

    /* Arquitetura detectada */
    nvidia_arch_t arch;

    /* MMIO (BAR0) */
    volatile u8 *mmio_base;
    u64          mmio_size;

    /* Framebuffer (BAR1) */
    u8  *fb_base;
    u64  fb_phys;
    u64  fb_size;

    /* RAMIN / PRAMIN (BAR3 em GPUs mais antigas) */
    volatile u8 *ramin_base;
    u64          ramin_size;

    /* Modo atual */
    nvidia_mode_t mode;

    /* Head ativo (0 ou 1) */
    u8  head;

    /* Flags */
    u8  initialized;
    u8  use_nv50_display; /* 1 = GeForce 8+ EVO display engine */
} nvidia_gpu_t;

/* ============================================================
 * API pública
 * ============================================================ */

/**
 * Inicializa o driver NVIDIA.
 * Detecta o dispositivo PCI, mapeia BAR0 (MMIO) e BAR1 (framebuffer).
 *
 * @param gpu    Ponteiro para estrutura de estado
 * @param bus    Barramento PCI
 * @param slot   Slot PCI
 * @param func   Função PCI
 * @return       0 em sucesso, negativo em erro
 */
int nvidia_gpu_init(nvidia_gpu_t *gpu, u8 bus, u8 slot, u8 func);

/**
 * Detecta automaticamente a GPU NVIDIA no barramento PCI.
 *
 * @param gpu    Ponteiro para estrutura a ser preenchida
 * @return       0 se encontrou, -1 se não encontrou
 */
int nvidia_gpu_detect(nvidia_gpu_t *gpu);

/**
 * Define o modo de vídeo.
 * Suporta GeForce 6/7 (PRAMDAC legacy) e GeForce 8+ (EVO).
 *
 * @param gpu    Estado do driver
 * @param mode   Modo desejado
 * @return       0 em sucesso, negativo em erro
 */
int nvidia_gpu_set_mode(nvidia_gpu_t *gpu, const nvidia_mode_t *mode);

/**
 * Retorna ponteiro para o framebuffer mapeado.
 *
 * @param gpu    Estado do driver
 * @return       Ponteiro para início do framebuffer
 */
void *nvidia_gpu_get_framebuffer(nvidia_gpu_t *gpu);

/**
 * Preenche o framebuffer com cor sólida.
 *
 * @param gpu    Estado do driver
 * @param color  Cor no formato 0x00RRGGBB
 */
void nvidia_gpu_clear(nvidia_gpu_t *gpu, u32 color);

/**
 * Desenha um pixel no framebuffer (32bpp).
 *
 * @param gpu    Estado do driver
 * @param x, y  Coordenadas
 * @param color  Cor 0x00RRGGBB
 */
void nvidia_gpu_put_pixel(nvidia_gpu_t *gpu, u32 x, u32 y, u32 color);

/**
 * Desenha um retângulo sólido.
 *
 * @param gpu          Estado do driver
 * @param x, y         Canto superior esquerdo
 * @param w, h         Largura e altura
 * @param color        Cor 0x00RRGGBB
 */
void nvidia_gpu_fill_rect(nvidia_gpu_t *gpu, u32 x, u32 y,
                          u32 w, u32 h, u32 color);

/**
 * Copia buffer de pixels para o framebuffer (blit).
 *
 * @param gpu     Estado do driver
 * @param src     Buffer de origem (mesmo formato do framebuffer)
 * @param dst_x   X destino
 * @param dst_y   Y destino
 * @param width   Largura
 * @param height  Altura
 */
void nvidia_gpu_blit(nvidia_gpu_t *gpu, const void *src,
                     u32 dst_x, u32 dst_y, u32 width, u32 height);

/**
 * Lê registrador MMIO de 32 bits (BAR0).
 */
u32  nvidia_gpu_read32(const nvidia_gpu_t *gpu, u32 reg);

/**
 * Escreve registrador MMIO de 32 bits (BAR0).
 */
void nvidia_gpu_write32(nvidia_gpu_t *gpu, u32 reg, u32 val);

/**
 * Detecta a arquitetura da GPU pelo Device ID.
 *
 * @param device_id  Device ID PCI
 * @return           nvidia_arch_t correspondente
 */
nvidia_arch_t nvidia_gpu_detect_arch(u16 device_id);

/**
 * Retorna string legível com o nome do dispositivo.
 *
 * @param device_id  Device ID PCI
 * @return           Nome do dispositivo
 */
const char *nvidia_gpu_device_name(u16 device_id);

/**
 * Retorna string legível com o nome da arquitetura.
 *
 * @param arch  Arquitetura
 * @return      Nome da arquitetura
 */
const char *nvidia_arch_name(nvidia_arch_t arch);

/**
 * Desliga o driver (desabilita display).
 *
 * @param gpu  Estado do driver
 */
void nvidia_gpu_shutdown(nvidia_gpu_t *gpu);

/* ============================================================
 * Modos pré-definidos comuns
 * ============================================================ */
extern const nvidia_mode_t NVIDIA_MODE_640x480_32;
extern const nvidia_mode_t NVIDIA_MODE_800x600_32;
extern const nvidia_mode_t NVIDIA_MODE_1024x768_32;
extern const nvidia_mode_t NVIDIA_MODE_1280x720_32;
extern const nvidia_mode_t NVIDIA_MODE_1920x1080_32;

#endif /* NVIDIAGPU_H */
