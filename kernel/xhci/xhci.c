/*
 * xhci.c — xHCI USB host controller driver (Viuxne OS) used in (Ytar OS)
 *
 * Enumerates boot-protocol HID devices, auto-detecting mouse vs keyboard
 * by reading bInterfaceProtocol from the configuration descriptor.
 *
 * Architecture based on SeaBIOS usb-xhci.c
 * Copyright (C) 2013 Gerd Hoffmann, 2014 Kevin O'Connor (LGPLv3)
 */

#include "xhci.h"
#include "xhci_regs.h"
#include "xhci_ring.h"
#include "xhci_ctx.h"
#include "xhci_events.h"
#include "xhci_debug.h"
#include "../pci.h"
#include "../include/serial.h"
#include <stdint.h>

xhci_t g_xhci;

static uint8_t  pool[524288] __attribute__((aligned(65536)));
static uint32_t pool_top = 0;

static void *palloc(uint32_t sz, uint32_t align) {
    uint8_t *p;
    uint32_t i;
    pool_top = (pool_top + align - 1u) & ~(align - 1u);
    p = pool + pool_top;
    for (i = 0; i < sz; i++) p[i] = 0;
    pool_top += sz;
    return (void *)p;
}

static void nwait(uint32_t n) { while (n--) __asm__ volatile("nop"); }

static void doorbell(xhci_t *x, uint32_t slot, uint32_t target) {
    W32(DOORBELL(x, slot), target);
}

static uint8_t wait_ring(xhci_t *x, ring_t *r) {
    uint32_t t = 40000000u;
    while (t--) {
        process_events(x);
        if (!ring_busy(r)) return (uint8_t)TRB_CC(r->evt.w[2]);
        nwait(5);
    }
    DBG("wait_ring: TIMEOUT");
    return 0xFF;
}

static uint8_t cmd_submit(xhci_t *x,
                           uint32_t w0, uint32_t w1,
                           uint32_t w2, uint32_t f)
{
    ring_enq(x->cmds, w0, w1, w2, f);
    doorbell(x, 0, DB_HOST_CMD);
    return wait_ring(x, x->cmds);
}

static void ctrl_xfr(xhci_t *x,
                      uint8_t bmt, uint8_t req,
                      uint16_t val, uint16_t idx,
                      uint16_t len, uint32_t buf, uint8_t in)
{
    uint32_t lo  = (uint32_t)bmt
                 | ((uint32_t)req << 8)
                 | ((uint32_t)val << 16);
    uint32_t hi  = (uint32_t)idx | ((uint32_t)len << 16);
    uint32_t trt = len ? (in ? 3u : 2u) : 0u;
    ring_enq(x->ep0r, lo, hi, 8,
             (TR_SETUP << 10) | TRB_IDT | (trt << 16));
    if (len)
        ring_enq(x->ep0r, buf, 0, len,
                 (TR_DATA << 10) | TRB_IOC | (in ? TRB_TR_DIR : 0));
    ring_enq(x->ep0r, 0, 0, 0,
             (TR_STATUS << 10) | TRB_IOC | (in ? 0 : TRB_TR_DIR));
    doorbell(x, x->slot_id, DB_EP0_OUT);
    wait_ring(x, x->ep0r);
    if (len) wait_ring(x, x->ep0r);
}

static void usb_get_descriptor(xhci_t *x, uint16_t wValue,
                                uint32_t buf, uint16_t len) {
    ctrl_xfr(x, 0x80, 0x06, wValue, 0, len, buf, 1);
}
static void usb_set_configuration(xhci_t *x, uint8_t cfg) {
    ctrl_xfr(x, 0x00, 0x09, cfg, 0, 0, 0, 0);
}
static void __attribute__((unused)) usb_set_interface(xhci_t *x, uint8_t iface, uint8_t alt) {
    ctrl_xfr(x, 0x01, 0x0B, alt, iface, 0, 0, 0);
}
static void hid_set_protocol(xhci_t *x, uint8_t protocol) {
    ctrl_xfr(x, 0x21, 0x0B, protocol, 0, 0, 0, 0);
}
static void hid_set_idle(xhci_t *x) {
    ctrl_xfr(x, 0x21, 0x0A, 0x0000, 0, 0, 0, 0);
}

static int port_reset(xhci_t *x, uint8_t port) {
    uint32_t ps, t;
    ps = R32(PORTSC(x, port));
    W32(PORTSC(x, port), (ps & ~PORTSC_WTCBITS) | PORTSC_PR);
    for (t = 3000000u; t--; ) {
        ps = R32(PORTSC(x, port));
        if (!(ps & PORTSC_PR) && (ps & PORTSC_PED)) return 1;
        nwait(1);
    }
    DBG("port_reset: timeout");
    return 0;
}

static uint8_t probe_port(xhci_t *x, uint8_t port_idx) {
    ring_init(x->ep0r); /* reset ring state before each probe */
    uint8_t  cc;
    uint32_t ps;
    uint16_t ep0_mps;
    uint8_t *cfg_buf = (uint8_t *)palloc(64, 4);

    ps = R32(PORTSC(x, port_idx));
    if (PORTSC_PLS(ps) == PLS_POLLING) {
        if (!port_reset(x, port_idx)) return 0;
    }
    nwait(300000u);
    ps = R32(PORTSC(x, port_idx));
    x->dev_speed = (uint8_t)PORTSC_SPEED(ps);

    cc = cmd_submit(x, 0, 0, 0, CR_ENABLE_SLOT<<10);
    if (cc != CC_SUCCESS) return 0;
    x->slot_id = (uint8_t)TRB_SLOT(x->cmds->evt.w[3]);
    if (!x->slot_id) return 0;
    x->devs[x->slot_id * 2]     = (uint32_t)x->dev_ctx;
    x->devs[x->slot_id * 2 + 1] = 0;

    ep0_mps = (x->dev_speed == SPEED_SUPER) ? 512
            : (x->dev_speed == SPEED_HIGH)  ? 64 : 8;
    ctx_zero(x);
    ICC_ADD_FLAGS(x) = 0x03u;
    SLOT_W0(x) = ((uint32_t)x->dev_speed << 20) | (1u << 27);
    SLOT_W1(x) = (uint32_t)(port_idx + 1) << 16;
    EP_W0(x,1) = EP_W0_VAL(0);
    EP_W1(x,1) = EP_W1_VAL(EP_TYPE_CTRL, 0, ep0_mps);
    EP_W2(x,1) = (uint32_t)x->ep0r->trbs | 1u;
    EP_W4(x,1) = 8;
    cc = cmd_submit(x, (uint32_t)x->in_ctx, 0, 0,
                    (CR_ADDR_DEV<<10) | TRB_BSR | ((uint32_t)x->slot_id<<24));
    if (cc != CC_SUCCESS) return 0;
    nwait(500000u);

    usb_get_descriptor(x, 0x0100, (uint32_t)x->desc_buf, 18);

    {
        uint16_t mps = (x->dev_speed == SPEED_SUPER) ? 512u
                     : (uint16_t)x->desc_buf[7];
        ctx_zero(x);
        ICC_ADD_FLAGS(x) = 0x02u;
        ctx_copy_ep(x, 1);
        EP_W1(x,1) = (EP_W1(x,1) & ~(0xFFFFu<<16)) | ((uint32_t)mps<<16);
        cmd_submit(x, (uint32_t)x->in_ctx, 0, 0,
                   (CR_EVAL_CTX<<10) | ((uint32_t)x->slot_id<<24));
    }

    ctx_zero(x);
    ICC_ADD_FLAGS(x) = 0x03u;
    SLOT_W0(x) = ((uint32_t)x->dev_speed << 20) | (1u << 27);
    SLOT_W1(x) = (uint32_t)(port_idx + 1) << 16;
    ctx_copy_ep(x, 1);
    cc = cmd_submit(x, (uint32_t)x->in_ctx, 0, 0,
                    (CR_ADDR_DEV<<10) | ((uint32_t)x->slot_id<<24));
    if (cc != CC_SUCCESS) return 0;
    nwait(200000u);

    usb_set_configuration(x, 1);

    usb_get_descriptor(x, 0x0200, (uint32_t)cfg_buf, 64);

    {
        uint8_t *p = cfg_buf;
        uint8_t *end = cfg_buf + 64;
        while (p < end && p[0] >= 2) {
            if (p[1] == 0x04) {
                uint8_t proto = p[7];
                DBGD("HID protocol=", proto);
                return proto;
            }
            p += p[0];
        }
    }
    return 0;
}

int xhci_init(void) {
    xhci_t  *x = &g_xhci;
    uint8_t  bus, dev, fn;
    uint8_t  p, cc;
    uint32_t ps;
    uint32_t hcsp1, hccp1;

    DBG("=== xhci_init ===");

    if (!pci_find(0x0C, 0x03, 0x30, &bus, &dev, &fn)) {
        DBG("ERR: xHCI PCI device not found");
        return 0;
    }
    x->bar0 = pci_read(bus, dev, fn, 0x10) & ~0xFu;
    pci_write(bus, dev, fn, 0x04, pci_read(bus, dev, fn, 0x04) | 0x06u);
    DBGX("bar0=", x->bar0);

    {
        uint8_t  cap_len = (uint8_t)(R32(CAPLENGTH(x)) & 0xFF);
        uint32_t db_off  = R32(DBOFF(x))  & ~3u;
        uint32_t rt_off  = R32(RTSOFF(x)) & ~31u;
        x->op_base = x->bar0 + cap_len;
        x->db_base = x->bar0 + db_off;
        x->ir_base = x->bar0 + rt_off + 0x20;
    }

    hcsp1 = R32(HCSPARAMS1(x));
    hccp1 = R32(HCCPARAMS1(x));
    x->n_slots = (uint8_t)HCSP1_MAXSLOTS(hcsp1);
    x->n_ports = (uint8_t)HCSP1_MAXPORTS(hcsp1);
    x->kbd_port = 0;
    DBGD("n_slots=", x->n_slots);
    DBGD("n_ports=", x->n_ports);
    DBGD("CSZ=",     HCCP1_CSZ(hccp1));

    if (R32(USBCMD(x)) & USBCMD_RUN) {
        W32(USBCMD(x), R32(USBCMD(x)) & ~USBCMD_RUN);
        { uint32_t t=1000000u; while (t-- && !(R32(USBSTS(x)) & USBSTS_HCH)) nwait(1); }
    }

    W32(USBCMD(x), USBCMD_HCRST);
    { uint32_t t=2000000u; while (t-- && (R32(USBCMD(x)) & USBCMD_HCRST)) nwait(1); }
    { uint32_t t=2000000u; while (t-- && (R32(USBSTS(x)) & USBSTS_CNR))  nwait(1); }
    nwait(200000u);
    DBG("reset done");

    x->cmds      = (ring_t *)palloc(sizeof(ring_t), RING_BYTES);
    x->evts      = (ring_t *)palloc(sizeof(ring_t), RING_BYTES);
    x->ep0r      = (ring_t *)palloc(sizeof(ring_t), RING_BYTES);
    x->intr_ring = (ring_t *)palloc(sizeof(ring_t), RING_BYTES);
    x->devs      = (uint32_t *)palloc(((uint32_t)x->n_slots+1)*8, 64);
    x->dev_ctx   = (uint32_t *)palloc(32*8*4, 1024);
    x->in_ctx    = (uint32_t *)palloc(33*8*4, 2048);
    x->erst      = (uint32_t *)palloc(16, 64);
    x->hid_buf   = (uint8_t *)palloc(HID_BUF_SIZE, 64);
    x->desc_buf  = (uint8_t *)palloc(18, 4);
    x->pool_base = (uint32_t)pool;
    x->pool_end  = (uint32_t)pool + sizeof(pool);
    ring_init(x->cmds);
    ring_init(x->evts);
    ring_init(x->ep0r);
    ring_init(x->intr_ring);

    W32(CONFIG(x),  CONFIG_MAX_SLOTS_EN(x->n_slots));
    W32(DNCTRL(x),  0);
    W64(DCBAAP(x),  (uint64_t)(uint32_t)x->devs);
    W64(CRCR(x),    (uint64_t)(uint32_t)x->cmds->trbs | 1u);

    x->erst[0] = (uint32_t)x->evts->trbs;
    x->erst[1] = 0;
    x->erst[2] = RING_ITEMS;
    x->erst[3] = 0;
    W32(IR_ERSTSZ(x), 1);
    W32(IR_ERDP(x),   (uint32_t)x->evts->trbs);
    W32(IR_ERDP(x)+4, 0);
    W32(IR_ERSTBA(x), (uint32_t)x->erst);
    W32(IR_ERSTBA(x)+4, 0);
    W32(IR_IMAN(x),   IR_IMAN_IE | IR_IMAN_IP);
    W32(IR_IMOD(x),   0);

    W32(USBCMD(x), USBCMD_RUN);
    { uint32_t t=1000000u; while (t-- && (R32(USBSTS(x)) & USBSTS_HCH)) nwait(1); }
    nwait(2000000u);
    DBGX("USBSTS=", R32(USBSTS(x)));

    x->port_num = 0;
    for (p = 0; p < x->n_ports; p++) {
        ps = R32(PORTSC(x, p));
        DBGD("port ", p+1); DBGX("  PORTSC=", ps);
        if (!(ps & PORTSC_CCS)) continue;

        uint8_t proto = probe_port(x, p);
        DBGD("port proto=", proto);

        if (proto == 2) {
            x->port_num = p + 1;
            DBG("mouse port found");
            break;
        } else if (proto == 1) {
            x->kbd_port = p + 1;
            DBGD("kbd_port=", x->kbd_port);
            cmd_submit(x, 0, 0, 0,
                       (CR_DISABLE_SLOT<<10) | ((uint32_t)x->slot_id<<24));
            x->slot_id = 0;
        }
    }

    if (!x->port_num) { DBG("ERR: no mouse port found"); return 0; }
    DBGD("mouse port=", x->port_num);

    hid_set_protocol(x, 0);
    hid_set_idle(x);

    {
        uint32_t interval = 3;
        ctx_zero(x);
        ICC_ADD_FLAGS(x) = (1u<<0)|(1u<<3);
        ctx_copy_slot(x);
        SLOT_W0(x) = (SLOT_W0(x) & ~(0x1Fu<<27)) | (3u<<27);
        EP_W0(x,3) = EP_W0_VAL(interval);
        EP_W1(x,3) = EP_W1_VAL(EP_TYPE_INTR_IN, 0, 8);
        EP_W2(x,3) = (uint32_t)x->intr_ring->trbs | 1u;
        EP_W4(x,3) = 8;
    }
    cc = cmd_submit(x, (uint32_t)x->in_ctx, 0, 0,
                    (CR_CFG_EP<<10) | ((uint32_t)x->slot_id<<24));
    DBGD("cfg_ep cc=", cc);
    if (cc != CC_SUCCESS) { DBG("ERR: CONFIGURE_ENDPOINT failed"); return 0; }

    ring_enq(x->intr_ring,
             (uint32_t)x->hid_buf, 0, HID_BUF_SIZE,
             (TR_NORMAL<<10) | TRB_IOC);
    doorbell(x, x->slot_id, DB_EP1_IN);
    x->intr_primed  = 1;
    x->report_ready = 0;

    DBG("=== xhci_init done ===");
    return 1;
}

int xhci_mbuff(uint8_t *btn, int8_t *dx, int8_t *dy) {
    xhci_t *x = &g_xhci;
    if (!x->intr_primed) return 0;
    process_events(x);
    if (!x->report_ready) return 0;
    x->report_ready = 0;
    *btn = x->hid_buf[0];
    *dx  = (int8_t)x->hid_buf[1];
    *dy  = (int8_t)x->hid_buf[2];
    ring_enq(x->intr_ring,
             (uint32_t)x->hid_buf, 0, HID_BUF_SIZE,
             (TR_NORMAL<<10) | TRB_IOC);
    doorbell(x, x->slot_id, DB_EP1_IN);
    return 1;
}
