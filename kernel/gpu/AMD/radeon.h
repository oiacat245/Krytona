#pragma once

#include <stdint.h>
#include "../../include/stddef.h"
#include "../../pci.h"
#include "../framebuffer.h"

#define PCI_VENDOR_ATI              0x1002
#define RADEON_REGSIZE              0x4000

#define CHIP_FAMILY_MASK            0x0000ffff
#define CHIP_HAS_CRTC2              0x00010000
#define CHIP_IS_MOBILITY            0x00020000
#define CHIP_IS_IGP                 0x00040000

#define CHIP_ERRATA_PLL_DUMMYREADS  0x00000001
#define CHIP_ERRATA_PLL_DELAY       0x00000002
#define CHIP_ERRATA_R300_CG         0x00000004

typedef enum {
    CHIP_FAMILY_UNKNOW,
    CHIP_FAMILY_LEGACY,
    CHIP_FAMILY_RADEON,
    CHIP_FAMILY_RV100,
    CHIP_FAMILY_RS100,
    CHIP_FAMILY_RV200,
    CHIP_FAMILY_RS200,
    CHIP_FAMILY_R200,
    CHIP_FAMILY_RV250,
    CHIP_FAMILY_RS300,
    CHIP_FAMILY_RV280,
    CHIP_FAMILY_R300,
    CHIP_FAMILY_R350,
    CHIP_FAMILY_RV350,
    CHIP_FAMILY_RV380,
    CHIP_FAMILY_R420,
    CHIP_FAMILY_RC410,
    CHIP_FAMILY_RS400,
    CHIP_FAMILY_RS480,
    CHIP_FAMILY_LAST,
} radeon_family_t;

#define IS_RV100_VARIANT(r) \
    ((r)->family == CHIP_FAMILY_RV100 || (r)->family == CHIP_FAMILY_RV200 || \
     (r)->family == CHIP_FAMILY_RS100 || (r)->family == CHIP_FAMILY_RS200 || \
     (r)->family == CHIP_FAMILY_RV250 || (r)->family == CHIP_FAMILY_RV280 || \
     (r)->family == CHIP_FAMILY_RS300)

#define IS_R300_VARIANT(r) \
    ((r)->family == CHIP_FAMILY_R300  || (r)->family == CHIP_FAMILY_RV350 || \
     (r)->family == CHIP_FAMILY_R350  || (r)->family == CHIP_FAMILY_RV380 || \
     (r)->family == CHIP_FAMILY_R420  || (r)->family == CHIP_FAMILY_RC410 || \
     (r)->family == CHIP_FAMILY_RS480)

#define MC_FB_LOCATION          0x0148
#define MC_AGP_LOCATION         0x014C
#define DISPLAY_BASE_ADDR       0x0170
#define CRTC2_DISPLAY_BASE_ADDR 0x033C
#define OV0_BASE_ADDR           0x0148
#define NB_TOM                  0x015C
#define CNFG_MEMSIZE            0x00F8
#define CNFG_MEMSIZE_MASK       0x1FFFFFFF
#define MEM_SDRAM_MODE_REG      0x0158
#define MEM_CNTL                0x0140
#define R300_MEM_NUM_CHANNELS_MASK 0x03
#define RV100_MEM_HALF_MODE     0x00000004
#define MEM_NUM_CHANNELS_MASK   0x00000001
#define GRPH2_BUFFER_CNTL       0x02F0
#define CNFG_CNTL               0x00E0
#define CFG_ATI_REV_ID_MASK     0x0000000F
#define CFG_ATI_REV_A11         0x00000002
#define CLOCK_CNTL_INDEX        0x0008
#define CLOCK_CNTL_DATA         0x000C
#define CRTC_GEN_CNTL           0x0050
#define CRTC2_GEN_CNTL          0x03F8
#define CRTC_EXT_CNTL           0x0054
#define CRTC_MORE_CNTL          0x27C
#define DAC_CNTL                0x0058
#define CRTC_H_TOTAL_DISP       0x0200
#define CRTC_H_SYNC_STRT_WID    0x0204
#define CRTC_V_TOTAL_DISP       0x0208
#define CRTC_V_SYNC_STRT_WID    0x020C
#define CRTC_PITCH              0x0224
#define SURFACE_CNTL            0x0B00
#define RBBM_STATUS             0x0E40
#define DSTCACHE_CTLSTAT        0x0744
#define FP_GEN_CNTL             0x0284
#define LVDS_GEN_CNTL           0x02D0
#define PIXCLKS_CNTL            0x002C
#define OVR_CLR                 0x0230
#define OVR_WID_LEFT_RIGHT      0x0234
#define OVR_WID_TOP_BOTTOM      0x0238
#define OV0_SCALE_CNTL          0x0420
#define SUBPIC_CNTL             0x0540
#define VIPH_CONTROL            0x0C40
#define I2C_CNTL_1              0x0094
#define GEN_INT_CNTL            0x0040
#define CAP0_TRIG_CNTL          0x0950
#define CAP1_TRIG_CNTL          0x09C0
#define PPLL_DIV_3              0x000B
#define PPLL_REF_DIV            0x000A
#define PPLL_CNTL               0x0002
#define HTOTAL_CNTL             0x0009
#define VCLK_ECP_CNTL           0x0008
#define PPLL_DIV_3_REG          0x0403C
#define PPLL_REF_DIV_REG        0x04034
#define PPLL_CNTL_REG           0x04030
#define HTOTAL_CNTL_REG         0x04024
#define CRTC_CRNT_FRAME         0x0214
#define DST_PITCH_OFFSET        0x142
#define DP_GUI_MASTER_CNTL      0x146
#define SURFACE0_LOWER_BOUND    0x0B04
#define SURFACE0_UPPER_BOUND    0x0B08
#define SURFACE0_INFO           0x0B0C

#define GUI_ACTIVE              0x80000000
#define RB2D_DC_FLUSH_ALL       0x000F
#define RB2D_DC_BUSY            0x80000000
#define PLL_WR_EN               0x00000080
#define CRTC_DISP_REQ_EN_B      0x00000002
#define CRTC2_DISP_REQ_EN_B     0x00000002
#define CRTC_DISPLAY_DIS        0x00000004
#define CRTC_HSYNC_DIS          0x00000008
#define CRTC_VSYNC_DIS          0x00000010
#define CRTC_EXT_DISP_EN        0x01000000
#define CRTC_EN                 0x02000000
#define CRTC_CRT_ON             0x00000100
#define CRTC_H_CUTOFF_ACTIVE_EN 0x00040000
#define VGA_ATI_LINEAR          0x00000008
#define XCRT_CNT_EN             0x00000020
#define DAC_MASK_ALL            0xFF000000
#define DAC_VGA_ADR_EN          0x00000200
#define DAC_8BIT_EN             0x00000100
#define FP_FPON                 0x00010000
#define FP_TMDS_EN              0x02000000
#define LVDS_DISPLAY_DIS        0x00000008
#define LVDS_BLON               0x00000020
#define LVDS_ON                 0x00000001
#define LVDS_EN                 0x00000002
#define LVDS_DIGON              0x00000010
#define LVDS_BL_MOD_EN          0x00002000
#define LVDS_STATE_MASK         0x0000FF38
#define PIXCLK_LVDS_ALWAYS_ONb  0x00000004
#define NONSURF_AP0_SWP_16BPP   0x00000100
#define NONSURF_AP1_SWP_16BPP   0x00000200
#define NONSURF_AP0_SWP_32BPP   0x00000300
#define NONSURF_AP1_SWP_32BPP   0x00000C00
#define CRTC_DBL_SCAN_EN        0x00000002
#define CRTC_INTERLACE_EN       0x00000001

#define MAX_MAPPED_VRAM         (2048*2048*4)
#define MIN_MAPPED_VRAM         (1024*768*1)

typedef struct {
    uint16_t device_id;
    uint32_t flags;
} radeon_pci_entry_t;

typedef struct {
    volatile uint8_t *mmio_base;
    volatile uint8_t *fb_base;
    uint64_t          fb_base_phys;
    uint64_t          mmio_base_phys;
    uint32_t          fb_local_base;
    uint32_t          video_ram;
    uint32_t          mapped_vram;
    uint32_t          vram_width;
    int               vram_ddr;
    radeon_family_t   family;
    uint16_t          chipset;
    int               has_CRTC2;
    int               is_mobility;
    int               is_IGP;
    uint32_t          errata;
    uint32_t          bpp;
    uint32_t          depth;
    uint32_t          pitch;
    uint32_t          pitch_bytes;
    uint8_t           pci_bus;
    uint8_t           pci_dev;
    uint8_t           pci_fn;
    
    int               is_active;
    uint32_t          width;
    uint32_t          height;
    void*             fb_virt;
} radeon_info_t;

extern radeon_info_t g_radeon;

static inline uint32_t radeon_readl(radeon_info_t *r, uint32_t reg) {
    return *(volatile uint32_t *)(r->mmio_base + reg);
}
static inline void radeon_writel(radeon_info_t *r, uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(r->mmio_base + reg) = val;
}

#define INREG(reg)          radeon_readl(rinfo, reg)
#define OUTREG(reg, val)    radeon_writel(rinfo, reg, val)
#define INREG8(reg)         (*(volatile uint8_t *)(rinfo->mmio_base + (reg)))
#define OUTREG8(reg, val)   (*(volatile uint8_t *)(rinfo->mmio_base + (reg)) = (val))

int  radeon_init(radeon_info_t *rinfo);
void radeon_set_mode(radeon_info_t *rinfo, uint32_t width, uint32_t height, uint8_t bpp);
void radeon_engine_flush(radeon_info_t *rinfo);
void radeon_engine_idle(radeon_info_t *rinfo);
