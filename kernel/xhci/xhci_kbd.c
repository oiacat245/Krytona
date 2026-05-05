#include "xhci_kbd.h"
#include "xhci.h"
#include "xhci_regs.h"
#include "xhci_ring.h"
#include "xhci_ctx.h"
#include "xhci_events.h"
#include "xhci_debug.h"
#include "../include/serial.h"
#include <stdint.h>

xhci_t g_xhci_kbd;

static uint8_t  kbd_pool[262144] __attribute__((aligned(65536)));
static uint32_t kbd_pool_top;

static void *kpalloc(uint32_t sz, uint32_t align) {
    uint8_t *p; uint32_t i;
    kbd_pool_top = (kbd_pool_top + align - 1u) & ~(align - 1u);
    p = kbd_pool + kbd_pool_top;
    for (i = 0; i < sz; i++) p[i] = 0;
    kbd_pool_top += sz;
    return (void *)p;
}

static void nwait(uint32_t n) { while (n--) __asm__ volatile("nop"); }

static uint8_t kbd_wait_ring(ring_t *r) {
    uint32_t t = 40000000u;
    while (t--) {
        process_events(&g_xhci);
        if (!ring_busy(r)) return (uint8_t)TRB_CC(r->evt.w[2]);
        nwait(5);
    }
    DBG("kbd wait_ring TIMEOUT");
    return 0xFF;
}

static uint8_t kbd_cmd(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t f) {
    ring_enq(g_xhci.cmds, w0, w1, w2, f);
    W32(DOORBELL((&g_xhci), 0), DB_HOST_CMD);
    return kbd_wait_ring(g_xhci.cmds);
}

static void kbd_ctrl_xfr(uint8_t bmt, uint8_t req, uint16_t val,
                          uint16_t idx, uint16_t len, uint32_t buf, uint8_t in) {
    xhci_t *x = &g_xhci_kbd;
    uint32_t lo  = (uint32_t)bmt | ((uint32_t)req<<8) | ((uint32_t)val<<16);
    uint32_t hi  = (uint32_t)idx | ((uint32_t)len<<16);
    uint32_t trt = len ? (in ? 3u : 2u) : 0u;
    ring_enq(x->ep0r, lo, hi, 8, (TR_SETUP<<10)|TRB_IDT|(trt<<16));
    if (len)
        ring_enq(x->ep0r, buf, 0, len, (TR_DATA<<10)|TRB_IOC|(in ? TRB_TR_DIR : 0));
    ring_enq(x->ep0r, 0, 0, 0, (TR_STATUS<<10)|TRB_IOC|(in ? 0 : TRB_TR_DIR));
    W32(DOORBELL((&g_xhci), x->slot_id), DB_EP0_OUT);
    kbd_wait_ring(x->ep0r);
    if (len) kbd_wait_ring(x->ep0r);
}

static int kbd_port_reset(uint8_t p) {
    uint32_t ps, t;
    xhci_t *x = &g_xhci;
    ps = R32(PORTSC(x, p));
    W32(PORTSC(x, p), (ps & ~PORTSC_WTCBITS) | PORTSC_PR);
    for (t = 3000000u; t--;) {
        ps = R32(PORTSC(x, p));
        if (!(ps & PORTSC_PR) && (ps & PORTSC_PED)) return 1;
        nwait(1);
    }
    return 0;
}

int xhci_kbd_init(void) {
    xhci_t  *x  = &g_xhci_kbd;
    xhci_t  *hc = &g_xhci;
    uint8_t  p, cc;
    uint32_t ps;

    kbd_pool_top = 0;

    x->bar0    = hc->bar0;
    x->op_base = hc->op_base;
    x->db_base = hc->db_base;
    x->ir_base = hc->ir_base;
    x->n_ports = hc->n_ports;
    x->n_slots = hc->n_slots;
    x->pool_base = (uint32_t)kbd_pool;
    x->pool_end  = (uint32_t)kbd_pool + sizeof(kbd_pool);

    x->ep0r      = (ring_t   *)kpalloc(sizeof(ring_t), RING_BYTES);
    x->intr_ring = (ring_t   *)kpalloc(sizeof(ring_t), RING_BYTES);
    x->dev_ctx   = (uint32_t *)kpalloc(32*8*4, 1024);
    x->in_ctx    = (uint32_t *)kpalloc(33*8*4, 2048);
    x->hid_buf   = (uint8_t  *)kpalloc(HID_BUF_SIZE, 64);
    x->desc_buf  = (uint8_t  *)kpalloc(18, 4);
    ring_init(x->ep0r);
    ring_init(x->intr_ring);
    x->report_ready = 0;
    x->intr_primed  = 0;

    /* use kbd_port pre-detected by xhci_init if available, else scan */
    x->port_num = 0;
    if (hc->kbd_port != 0) {
        x->port_num = hc->kbd_port;
        DBGD("kbd using pre-detected port=", x->port_num);
    } else {
        for (p = 0; p < x->n_ports; p++) {
            ps = R32(PORTSC(hc, p));
            if (!(ps & PORTSC_CCS)) continue;
            if (p+1 == hc->port_num) continue;
            x->port_num = p+1;
            break;
        }
    }
    if (!x->port_num) { DBG("kbd: no port"); return 0; }
    DBGD("kbd port=", x->port_num);

    ps = R32(PORTSC(hc, x->port_num-1));
    if (PORTSC_PLS(ps) == PLS_POLLING) {
        if (!kbd_port_reset(x->port_num-1)) { DBG("kbd port reset fail"); return 0; }
    }
    nwait(300000u);
    ps = R32(PORTSC(hc, x->port_num-1));
    x->dev_speed = (uint8_t)PORTSC_SPEED(ps);

    cc = kbd_cmd(0, 0, 0, CR_ENABLE_SLOT<<10);
    if (cc != CC_SUCCESS) { DBG("kbd enable slot fail"); return 0; }
    x->slot_id = (uint8_t)TRB_SLOT(hc->cmds->evt.w[3]);
    if (!x->slot_id) return 0;
    hc->devs[x->slot_id*2]   = (uint32_t)x->dev_ctx;
    hc->devs[x->slot_id*2+1] = 0;

    ctx_zero(x);
    ICC_ADD_FLAGS(x) = 0x03u;
    SLOT_W0(x) = ((uint32_t)x->dev_speed<<20)|(1u<<27);
    SLOT_W1(x) = (uint32_t)x->port_num<<16;
    EP_W1(x,1) = EP_W1_VAL(EP_TYPE_CTRL, 0, 8);
    EP_W2(x,1) = (uint32_t)x->ep0r->trbs | 1u;
    EP_W4(x,1) = 8;
    cc = kbd_cmd((uint32_t)x->in_ctx,0,0,(CR_ADDR_DEV<<10)|TRB_BSR|((uint32_t)x->slot_id<<24));
    if (cc != CC_SUCCESS) return 0;
    nwait(500000u);

    kbd_ctrl_xfr(0x80, 0x06, 0x0100, 0, 18, (uint32_t)x->desc_buf, 1);

    ctx_zero(x); ICC_ADD_FLAGS(x)=0x02u; ctx_copy_ep(x,1);
    EP_W1(x,1)=(EP_W1(x,1)&~(0xFFFFu<<16))|((uint32_t)x->desc_buf[7]<<16);
    kbd_cmd((uint32_t)x->in_ctx,0,0,(CR_EVAL_CTX<<10)|((uint32_t)x->slot_id<<24));

    ctx_zero(x); ICC_ADD_FLAGS(x)=0x03u;
    SLOT_W0(x)=((uint32_t)x->dev_speed<<20)|(1u<<27);
    SLOT_W1(x)=(uint32_t)x->port_num<<16;
    ctx_copy_ep(x,1);
    cc=kbd_cmd((uint32_t)x->in_ctx,0,0,(CR_ADDR_DEV<<10)|((uint32_t)x->slot_id<<24));
    if (cc != CC_SUCCESS) return 0;
    nwait(200000u);

    kbd_ctrl_xfr(0x00, 0x09, 1, 0, 0, 0, 0);
    kbd_ctrl_xfr(0x21, 0x0B, 0, 0, 0, 0, 0);
    kbd_ctrl_xfr(0x21, 0x0A, 0, 0, 0, 0, 0);

    ctx_zero(x);
    ICC_ADD_FLAGS(x)=(1u<<0)|(1u<<3);
    ctx_copy_slot(x);
    SLOT_W0(x)=(SLOT_W0(x)&~(0x1Fu<<27))|(3u<<27);
    EP_W0(x,3)=EP_W0_VAL(3);
    EP_W1(x,3)=EP_W1_VAL(EP_TYPE_INTR_IN,0,8);
    EP_W2(x,3)=(uint32_t)x->intr_ring->trbs|1u;
    EP_W4(x,3)=8;
    cc=kbd_cmd((uint32_t)x->in_ctx,0,0,(CR_CFG_EP<<10)|((uint32_t)x->slot_id<<24));
    if (cc != CC_SUCCESS) { DBG("kbd cfg ep fail"); return 0; }

    ring_enq(x->intr_ring,(uint32_t)x->hid_buf,0,HID_BUF_SIZE,(TR_NORMAL<<10)|TRB_IOC);
    W32(DOORBELL(hc, x->slot_id), DB_EP1_IN);
    x->intr_primed  = 1;
    x->report_ready = 0;

    DBG("kbd init ok");
    return 1;
}

int xhci_kbuff(uint8_t *report) {
    xhci_t *x = &g_xhci_kbd;
    uint8_t i;
    if (!x->intr_primed) return 0;
    process_events(&g_xhci);
    if (!x->report_ready) return 0;
    x->report_ready = 0;
    for (i = 0; i < 8; i++) report[i] = x->hid_buf[i];
    ring_enq(x->intr_ring,(uint32_t)x->hid_buf,0,HID_BUF_SIZE,(TR_NORMAL<<10)|TRB_IOC);
    W32(DOORBELL((&g_xhci), x->slot_id), DB_EP1_IN);
    return 1;
}
