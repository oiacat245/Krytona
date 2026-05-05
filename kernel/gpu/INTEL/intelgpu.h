#ifndef INTELGPU_H
#define INTELGPU_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;


#define INTEL_VENDOR_ID         0x8086

/* Gen 4 - GMA 950 / 945GM */
#define INTEL_DEV_945GM         0x27A6
#define INTEL_DEV_945G          0x2772

/* Gen 5 - HD Graphics (Ironlake) */
#define INTEL_DEV_IRONLAKE      0x0042
#define INTEL_DEV_IRONLAKE_M    0x0046

/* Gen 6 - HD Graphics 2000/3000 (Sandy Bridge) */
#define INTEL_DEV_SANDYBRIDGE   0x0102
#define INTEL_DEV_SANDYBRIDGE_M 0x0106

/* Gen 7 - HD Graphics 2500/4000 (Ivy Bridge) */
#define INTEL_DEV_IVYBRIDGE     0x0152
#define INTEL_DEV_IVYBRIDGE_M   0x0156

/* Gen 7.5 - HD Graphics 4600 (Haswell) */
#define INTEL_DEV_HASWELL       0x0412
#define INTEL_DEV_HASWELL_M     0x0416

/* Gen 8 - HD Graphics 5500 (Broadwell) */
#define INTEL_DEV_BROADWELL     0x1616

/* Gen 9 - HD Graphics 520/530 (Skylake) */
#define INTEL_DEV_SKYLAKE_GT2   0x1916
#define INTEL_DEV_SKYLAKE_GT2_2 0x1912


#define INTEL_REG_GFX_MODE          0x0002520
#define INTEL_REG_INSTPM            0x0020C0
#define INTEL_REG_GDRST             0x0000941C  /* Reset da GPU */

/* Display / Pipe A */
#define INTEL_REG_HTOTAL_A          0x060000
#define INTEL_REG_HBLANK_A          0x060004
#define INTEL_REG_HSYNC_A           0x060008
#define INTEL_REG_VTOTAL_A          0x06000C
#define INTEL_REG_VBLANK_A          0x060010
#define INTEL_REG_VSYNC_A           0x060014
#define INTEL_REG_PIPEASRC          0x06001C   /* Resolução do pipe A */
#define INTEL_REG_PIPEACONF         0x070008   /* Controle do pipe A */
#define INTEL_REG_DSPAADDR          0x071184   /* Endereço do framebuffer A (linear) */
#define INTEL_REG_DSPASTRIDE        0x071188   /* Stride (bytes por linha) */
#define INTEL_REG_DSPASURF          0x07119C   /* Surface address */
#define INTEL_REG_DSPACNTR          0x070180   /* Controle do plano A */

/* Display / Pipe B */
#define INTEL_REG_HTOTAL_B          0x061000
#define INTEL_REG_VTOTAL_B          0x06100C
#define INTEL_REG_PIPEBCONF         0x071008
#define INTEL_REG_DSPBADDR          0x071984
#define INTEL_REG_DSPBSTRIDE        0x071988
#define INTEL_REG_DSPBCNTR          0x071280

/* PLL / Clock */
#define INTEL_REG_DPLL_A            0x060010   /* PLL do pipe A */
#define INTEL_REG_DPLL_B            0x061010
#define INTEL_REG_FPA0              0x060020
#define INTEL_REG_FPA1              0x060024
#define INTEL_REG_FPB0              0x061020
#define INTEL_REG_FPB1              0x061024

/* GTT / Memory */
#define INTEL_REG_GTTADR            0x0010    /* Base da GTT (em BAR0) */
#define INTEL_GTT_VALID             (1 << 0)  /* Entrada GTT válida */
#define INTEL_GTT_CACHE_LLC         (1 << 1)  /* Cache LLC */

/* Backlight */
#define INTEL_REG_BLC_PWM_CTL       0x061250
#define INTEL_REG_BLC_PWM_CTL2      0x061254

/* PIPEACONF / PIPEBCONF */
#define INTEL_PIPE_ENABLE           (1u << 31)
#define INTEL_PIPE_STATE            (1u << 30)

#define INTEL_DISPLAY_PLANE_ENABLE  (1u << 31)
#define INTEL_DISPLAY_PLANE_TILED   (1u << 10)
#define INTEL_DISPLAY_PLANE_32BPP   (0x6 << 26)  /* BGRX 8888 */
#define INTEL_DISPLAY_PLANE_16BPP   (0x5 << 26)  /* RGB 565 */
#define INTEL_DISPLAY_PLANE_24BPP   (0x7 << 26)  /* BGR 888 */


typedef struct {
    u32 width;
    u32 height;
    u32 bpp;         
    u32 pitch;       
    u32 refresh_hz;  

    
    u32 h_active;
    u32 h_sync_start;
    u32 h_sync_end;
    u32 h_total;


    u32 v_active;
    u32 v_sync_start;
    u32 v_sync_end;
    u32 v_total;

    u32 dot_clock_khz; 
} intel_mode_t;

typedef struct {
    /* PCI */
    u16 vendor_id;
    u16 device_id;
    u8  pci_bus;
    u8  pci_slot;
    u8  pci_func;

    /* MMIO (BAR0) */
    volatile u8 *mmio_base;
    u64          mmio_size;

    /* Framebuffer (BAR2 ou GTT) */
    u8  *fb_base;       
    u64  fb_phys;       
    u64  fb_size;       
    /* GTT (Graphics Translation Table) */
    volatile u32 *gtt_base;
    u32           gtt_entries;

    
    intel_mode_t  mode;

   
    u8  initialized;
    u8  pipe;           /* 0 = pipe A, 1 = pipe B */
} intel_gpu_t;


typedef struct {
    u64 phys_addr;
    u32 flags;
} intel_gtt_entry_t;


int intel_gpu_init(intel_gpu_t *gpu, u8 bus, u8 slot, u8 func);


int intel_gpu_detect(intel_gpu_t *gpu);


int intel_gpu_set_mode(intel_gpu_t *gpu, const intel_mode_t *mode);


void *intel_gpu_get_framebuffer(intel_gpu_t *gpu);


void intel_gpu_clear(intel_gpu_t *gpu, u32 color);


void intel_gpu_put_pixel(intel_gpu_t *gpu, u32 x, u32 y, u32 color);


void intel_gpu_fill_rect(intel_gpu_t *gpu, u32 x, u32 y, u32 w, u32 h, u32 color);


void intel_gpu_blit(intel_gpu_t *gpu, const void *src,
                    u32 dst_x, u32 dst_y, u32 width, u32 height);
    
int intel_gpu_gtt_map(intel_gpu_t *gpu, u32 gtt_offset,
                      u64 phys_addr, u32 num_pages);


u32 intel_gpu_read32(const intel_gpu_t *gpu, u32 reg);


void intel_gpu_write32(intel_gpu_t *gpu, u32 reg, u32 val);


void intel_gpu_shutdown(intel_gpu_t *gpu);


const char *intel_gpu_device_name(u16 device_id);

extern const intel_mode_t INTEL_MODE_640x480_32;
extern const intel_mode_t INTEL_MODE_800x600_32;
extern const intel_mode_t INTEL_MODE_1024x768_32;
extern const intel_mode_t INTEL_MODE_1280x720_32;
extern const intel_mode_t INTEL_MODE_1920x1080_32;

#endif /* INTELGPU_H */
