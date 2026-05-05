#include "radeon.h"
#include "../pci.h"
#include "../framebuffer.h"


typedef struct {
    uint16_t device_id;
    uint32_t flags;
} pci_entry_t;

static const pci_entry_t radeon_table[] = {
    { 0x5955, CHIP_FAMILY_RS480 | CHIP_HAS_CRTC2 | CHIP_IS_IGP | CHIP_IS_MOBILITY },
    { 0x5975, CHIP_FAMILY_RS480 | CHIP_HAS_CRTC2 | CHIP_IS_IGP | CHIP_IS_MOBILITY },
    { 0x4C59, CHIP_FAMILY_RV100 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4C5A, CHIP_FAMILY_RV100 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x5159, CHIP_FAMILY_RV100 | CHIP_HAS_CRTC2 },
    { 0x515A, CHIP_FAMILY_RV100 | CHIP_HAS_CRTC2 },
    { 0x5E48, CHIP_FAMILY_RV100 | CHIP_HAS_CRTC2 },
    { 0x4336, CHIP_FAMILY_RS100 | CHIP_HAS_CRTC2 | CHIP_IS_IGP | CHIP_IS_MOBILITY },
    { 0x4136, CHIP_FAMILY_RS100 | CHIP_HAS_CRTC2 | CHIP_IS_IGP },
    { 0x4337, CHIP_FAMILY_RS200 | CHIP_HAS_CRTC2 | CHIP_IS_IGP | CHIP_IS_MOBILITY },
    { 0x4137, CHIP_FAMILY_RS200 | CHIP_HAS_CRTC2 | CHIP_IS_IGP },
    { 0x4437, CHIP_FAMILY_RS200 | CHIP_HAS_CRTC2 | CHIP_IS_IGP | CHIP_IS_MOBILITY },
    { 0x4237, CHIP_FAMILY_RS200 | CHIP_HAS_CRTC2 | CHIP_IS_IGP },
    { 0x4242, CHIP_FAMILY_R200  | CHIP_HAS_CRTC2 },
    { 0x4243, CHIP_FAMILY_R200  | CHIP_HAS_CRTC2 },
    { 0x5148, CHIP_FAMILY_R200  | CHIP_HAS_CRTC2 },
    { 0x514C, CHIP_FAMILY_R200  | CHIP_HAS_CRTC2 },
    { 0x514D, CHIP_FAMILY_R200  | CHIP_HAS_CRTC2 },
    { 0x4C57, CHIP_FAMILY_RV200 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4C58, CHIP_FAMILY_RV200 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x5157, CHIP_FAMILY_RV200 | CHIP_HAS_CRTC2 },
    { 0x5158, CHIP_FAMILY_RV200 | CHIP_HAS_CRTC2 },
    { 0x4C64, CHIP_FAMILY_RV250 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4C65, CHIP_FAMILY_RV250 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4C66, CHIP_FAMILY_RV250 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4C67, CHIP_FAMILY_RV250 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4966, CHIP_FAMILY_RV250 | CHIP_HAS_CRTC2 },
    { 0x4967, CHIP_FAMILY_RV250 | CHIP_HAS_CRTC2 },
    { 0x5A62, CHIP_FAMILY_RC410 | CHIP_HAS_CRTC2 | CHIP_IS_IGP | CHIP_IS_MOBILITY },
    { 0x5835, CHIP_FAMILY_RS300 | CHIP_HAS_CRTC2 | CHIP_IS_IGP | CHIP_IS_MOBILITY },
    { 0x7835, CHIP_FAMILY_RS300 | CHIP_HAS_CRTC2 | CHIP_IS_IGP | CHIP_IS_MOBILITY },
    { 0x5834, CHIP_FAMILY_RS300 | CHIP_HAS_CRTC2 | CHIP_IS_IGP },
    { 0x7834, CHIP_FAMILY_RS300 | CHIP_HAS_CRTC2 | CHIP_IS_IGP },
    { 0x5C61, CHIP_FAMILY_RV280 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x5C63, CHIP_FAMILY_RV280 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x5960, CHIP_FAMILY_RV280 | CHIP_HAS_CRTC2 },
    { 0x5961, CHIP_FAMILY_RV280 | CHIP_HAS_CRTC2 },
    { 0x5962, CHIP_FAMILY_RV280 | CHIP_HAS_CRTC2 },
    { 0x5964, CHIP_FAMILY_RV280 | CHIP_HAS_CRTC2 },
    { 0x4144, CHIP_FAMILY_R300  | CHIP_HAS_CRTC2 },
    { 0x4145, CHIP_FAMILY_R300  | CHIP_HAS_CRTC2 },
    { 0x4146, CHIP_FAMILY_R300  | CHIP_HAS_CRTC2 },
    { 0x4147, CHIP_FAMILY_R300  | CHIP_HAS_CRTC2 },
    { 0x4E44, CHIP_FAMILY_R300  | CHIP_HAS_CRTC2 },
    { 0x4E45, CHIP_FAMILY_R300  | CHIP_HAS_CRTC2 },
    { 0x4E46, CHIP_FAMILY_R300  | CHIP_HAS_CRTC2 },
    { 0x4E47, CHIP_FAMILY_R300  | CHIP_HAS_CRTC2 },
    { 0x4E50, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4E51, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4E52, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4E53, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4E54, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4E56, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4150, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 },
    { 0x4151, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 },
    { 0x4152, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 },
    { 0x4153, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 },
    { 0x4154, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 },
    { 0x4156, CHIP_FAMILY_RV350 | CHIP_HAS_CRTC2 },
    { 0x4148, CHIP_FAMILY_R350  | CHIP_HAS_CRTC2 },
    { 0x4149, CHIP_FAMILY_R350  | CHIP_HAS_CRTC2 },
    { 0x414A, CHIP_FAMILY_R350  | CHIP_HAS_CRTC2 },
    { 0x414B, CHIP_FAMILY_R350  | CHIP_HAS_CRTC2 },
    { 0x4E48, CHIP_FAMILY_R350  | CHIP_HAS_CRTC2 },
    { 0x4E49, CHIP_FAMILY_R350  | CHIP_HAS_CRTC2 },
    { 0x4E4A, CHIP_FAMILY_R350  | CHIP_HAS_CRTC2 },
    { 0x4E4B, CHIP_FAMILY_R350  | CHIP_HAS_CRTC2 },
    { 0x3E50, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 },
    { 0x3E54, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 },
    { 0x3150, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x3154, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x5B60, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 },
    { 0x5B62, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 },
    { 0x5B63, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 },
    { 0x5B64, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 },
    { 0x5B65, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 },
    { 0x5460, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x5464, CHIP_FAMILY_RV380 | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4A48, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x4A49, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x4A4A, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x4A4B, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x4A4C, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x4A4D, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x4A4E, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 | CHIP_IS_MOBILITY },
    { 0x4A50, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x5548, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x5549, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x554A, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x554B, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x5551, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x5552, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x5554, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x5D57, CHIP_FAMILY_R420  | CHIP_HAS_CRTC2 },
    { 0x5144, CHIP_FAMILY_RADEON, 0 },
    { 0x5145, CHIP_FAMILY_RADEON, 0 },
    { 0x5146, CHIP_FAMILY_RADEON, 0 },
    { 0x5147, CHIP_FAMILY_RADEON, 0 },
    { 0, 0 }
};

static void radeon_udelay(uint32_t us) {
    volatile uint32_t i;
    for (i = 0; i < us * 100; i++) {}
}

static void radeon_mdelay(uint32_t ms) {
    radeon_udelay(ms * 1000);
}

static void outb_port(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0,%1" :: "a"(val), "Nd"(port));
}

static uint8_t inb_port(uint16_t port) {
    uint8_t v;
    __asm__ volatile ("inb %1,%0" : "=a"(v) : "Nd"(port));
    return v;
}

static uint32_t pll_read(radeon_info_t *rinfo, uint32_t addr) {
    OUTREG8(CLOCK_CNTL_INDEX, addr & 0x3F);
    return INREG(CLOCK_CNTL_DATA);
}

static void pll_write(radeon_info_t *rinfo, uint32_t addr, uint32_t val) {
    OUTREG8(CLOCK_CNTL_INDEX, (addr & 0x3F) | 0x80);
    OUTREG(CLOCK_CNTL_DATA, val);
}

static void pll_writep(radeon_info_t *rinfo, uint32_t addr, uint32_t val, uint32_t mask) {
    uint32_t tmp = pll_read(rinfo, addr);
    tmp &= mask;
    tmp |= val;
    pll_write(rinfo, addr, tmp);
}

void radeon_engine_flush(radeon_info_t *rinfo) {
    uint32_t v = INREG(DSTCACHE_CTLSTAT);
    v |= RB2D_DC_FLUSH_ALL;
    OUTREG(DSTCACHE_CTLSTAT, v);
    for (int i = 0; i < 2000000; i++) {
        if (!(INREG(DSTCACHE_CTLSTAT) & RB2D_DC_BUSY)) return;
        radeon_udelay(1);
    }
}

static void radeon_fifo_wait(radeon_info_t *rinfo, int entries) {
    for (int i = 0; i < 2000000; i++) {
        if ((int)(INREG(RBBM_STATUS) & 0x7F) >= entries) return;
        radeon_udelay(1);
    }
}

void radeon_engine_idle(radeon_info_t *rinfo) {
    radeon_fifo_wait(rinfo, 64);
    for (int i = 0; i < 2000000; i++) {
        if (!(INREG(RBBM_STATUS) & GUI_ACTIVE)) {
            radeon_engine_flush(rinfo);
            return;
        }
        radeon_udelay(1);
    }
}

static void radeon_identify_vram(radeon_info_t *rinfo) {
    uint32_t tmp;
    if (rinfo->family == CHIP_FAMILY_RS100 || rinfo->family == CHIP_FAMILY_RS200 ||
        rinfo->family == CHIP_FAMILY_RS300 || rinfo->family == CHIP_FAMILY_RC410 ||
        rinfo->family == CHIP_FAMILY_RS400 || rinfo->family == CHIP_FAMILY_RS480) {
        uint32_t tom = INREG(NB_TOM);
        tmp = ((((tom >> 16) - (tom & 0xFFFF) + 1) << 6) * 1024);
        radeon_fifo_wait(rinfo, 6);
        OUTREG(MC_FB_LOCATION, tom);
        OUTREG(DISPLAY_BASE_ADDR, (tom & 0xFFFF) << 16);
        if (rinfo->has_CRTC2)
            OUTREG(CRTC2_DISPLAY_BASE_ADDR, (tom & 0xFFFF) << 16);
        OUTREG(OV0_BASE_ADDR, (tom & 0xFFFF) << 16);
        OUTREG(GRPH2_BUFFER_CNTL, INREG(GRPH2_BUFFER_CNTL) & ~0x7F0000);
    } else {
        tmp = INREG(CNFG_MEMSIZE);
    }
    rinfo->video_ram = tmp & CNFG_MEMSIZE_MASK;
    if (rinfo->video_ram == 0) {
        if (rinfo->chipset == 0x4C59 || rinfo->chipset == 0x4C5A)
            rinfo->video_ram = 8192 * 1024;
    }
    if (rinfo->is_IGP || rinfo->family >= CHIP_FAMILY_R300 ||
        (INREG(MEM_SDRAM_MODE_REG) & (1 << 30)))
        rinfo->vram_ddr = 1;
    else
        rinfo->vram_ddr = 0;
    tmp = INREG(MEM_CNTL);
    if (IS_R300_VARIANT(rinfo)) {
        switch (tmp & R300_MEM_NUM_CHANNELS_MASK) {
            case 0:  rinfo->vram_width = 64;  break;
            case 1:  rinfo->vram_width = 128; break;
            case 2:  rinfo->vram_width = 256; break;
            default: rinfo->vram_width = 128; break;
        }
    } else if (rinfo->family == CHIP_FAMILY_RV100 ||
               rinfo->family == CHIP_FAMILY_RS100 ||
               rinfo->family == CHIP_FAMILY_RS200) {
        rinfo->vram_width = (tmp & RV100_MEM_HALF_MODE) ? 32 : 64;
    } else {
        rinfo->vram_width = (tmp & MEM_NUM_CHANNELS_MASK) ? 128 : 64;
    }
}

static void radeon_write_mode(radeon_info_t *rinfo, uint32_t width, uint32_t height, uint8_t bpp) {
    uint32_t format;
    switch (bpp) {
        case 8:  format = 2; break;
        case 15: format = 3; break;
        case 16: format = 4; break;
        case 24: format = 5; break;
        case 32: format = 6; break;
        default: format = 6; break;
    }
    uint32_t pitch = ((width * ((bpp + 1) / 8) + 0x3F) & ~0x3F) >> 6;
    rinfo->pitch = pitch;
    uint32_t hTotal = width + 160;
    uint32_t hSyncStart = width + 24;
    uint32_t hSyncEnd = hSyncStart + 32;
    uint32_t vTotal = height + 30;
    uint32_t vSyncStart = height + 3;
    uint32_t vSyncEnd = vSyncStart + 6;
    uint32_t hsync_wid = (hSyncEnd - hSyncStart) / 8;
    uint32_t vsync_wid = vSyncEnd - vSyncStart;
    if (hsync_wid == 0) hsync_wid = 1;
    if (hsync_wid > 0x3F) hsync_wid = 0x3F;
    if (vsync_wid == 0) vsync_wid = 1;
    if (vsync_wid > 0x1F) vsync_wid = 0x1F;
    radeon_engine_idle(rinfo);
    OUTREG(CRTC_GEN_CNTL, CRTC_EXT_DISP_EN | CRTC_EN | (format << 8));
    OUTREG(CRTC_EXT_CNTL, VGA_ATI_LINEAR | XCRT_CNT_EN | CRTC_CRT_ON);
    OUTREG(DAC_CNTL, DAC_MASK_ALL | DAC_VGA_ADR_EN | DAC_8BIT_EN);
    OUTREG(CRTC_H_TOTAL_DISP, (((hTotal / 8) - 1) & 0x3FF) | (((width / 8) - 1) << 16));
    OUTREG(CRTC_H_SYNC_STRT_WID, ((hSyncStart - 8) & 0x1FFF) | (hsync_wid << 16));
    OUTREG(CRTC_V_TOTAL_DISP, ((vTotal - 1) & 0xFFFF) | ((height - 1) << 16));
    OUTREG(CRTC_V_SYNC_STRT_WID, ((vSyncStart - 1) & 0xFFF) | (vsync_wid << 16));
    uint32_t crtc_pitch = (pitch << 3) / ((bpp + 1) / 8);
    OUTREG(CRTC_PITCH, crtc_pitch | (crtc_pitch << 16));
    for (int i = 0; i < 8; i++) {
        OUTREG(SURFACE0_LOWER_BOUND + i * 0x10, 0);
        OUTREG(SURFACE0_UPPER_BOUND + i * 0x10, 0x1F);
        OUTREG(SURFACE0_INFO + i * 0x10, 0);
    }
    OUTREG(DISPLAY_BASE_ADDR, rinfo->fb_local_base);
}

static int radeon_lookup(uint16_t device_id, uint32_t *flags_out) {
    for (int i = 0; radeon_table[i].device_id != 0; i++) {
        if (radeon_table[i].device_id == device_id) {
            *flags_out = radeon_table[i].flags;
            return 1;
        }
    }
    return 0;
}

static uint64_t pci_bar_base(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t bar_idx) {
    uint32_t bar = pci_read(bus, dev, fn, 0x10 + bar_idx * 4);
    return (uint64_t)(bar & 0xFFFFFFF0);
}

int radeon_init(radeon_info_t *rinfo) {
    uint8_t bus, d, fn;
    if (!pci_find(0x03, 0x00, 0x00, &bus, &d, &fn)) return 0;
    uint32_t id = pci_read(bus, d, fn, 0x00);
    uint16_t vendor = id & 0xFFFF;
    uint16_t device = (id >> 16) & 0xFFFF;
    if (vendor != 0x1002) return 0;
    uint32_t flags;
    if (!radeon_lookup(device, &flags)) return 0;
    rinfo->pci_bus = bus;
    rinfo->pci_dev = d;
    rinfo->pci_fn = fn;
    rinfo->chipset = device;
    rinfo->family = flags & 0x0000FFFF;
    rinfo->has_CRTC2 = (flags & 0x00010000) != 0;
    rinfo->is_mobility = (flags & 0x00020000) != 0;
    rinfo->is_IGP = (flags & 0x00040000) != 0;
    rinfo->fb_base_phys = pci_bar_base(bus, d, fn, 0);
    rinfo->mmio_base_phys = pci_bar_base(bus, d, fn, 2);
    rinfo->mmio_base = (volatile uint8_t *)(uintptr_t)rinfo->mmio_base_phys;
    rinfo->fb_base = (volatile uint8_t *)(uintptr_t)rinfo->fb_base_phys;
    rinfo->errata = 0;
    rinfo->fb_local_base = INREG(MC_FB_LOCATION) << 16;
    radeon_identify_vram(rinfo);
    uint32_t map_size = rinfo->video_ram;
    if (map_size > 0x10000000) map_size = 0x10000000;
    rinfo->mapped_vram = map_size;
    
    rinfo->is_active = 1;
    rinfo->fb_virt = (void*)rinfo->fb_base;
    return 1;
}

void radeon_set_mode(radeon_info_t *rinfo, uint32_t width, uint32_t height, uint8_t bpp) {
    rinfo->width  = width;
    rinfo->height = height;
    rinfo->bpp    = bpp;
    rinfo->depth  = bpp;
    radeon_write_mode(rinfo, width, height, bpp);
    uint32_t pitch_bytes = ((width * (bpp / 8)) + 63) & ~63u;
    fb_init(rinfo->fb_base_phys, width, height, pitch_bytes, bpp);
}
