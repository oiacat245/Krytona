#ifndef XHCI_H
#define XHCI_H
#include <stdint.h>

/* ── TRB types ─────────────────────────────────────────────────────── */
#define TR_NORMAL       1
#define TR_SETUP        2
#define TR_DATA         3
#define TR_STATUS       4
#define TR_ISOCH        5
#define TR_LINK         6
#define TR_EVENT_DATA   7
#define TR_NOOP         8
#define CR_ENABLE_SLOT  9
#define CR_DISABLE_SLOT 10
#define CR_ADDR_DEV     11
#define CR_CFG_EP       12
#define CR_EVAL_CTX     13
#define CR_RESET_EP     14
#define CR_STOP_EP      15
#define CR_SET_TR_DEQUE 16
#define CR_RESET_DEV    17
#define CR_NOOP         23
#define ER_TRANSFER     32
#define ER_CMD_DONE     33
#define ER_PORT_STATUS  34

/* ── Completion codes ───────────────────────────────────────────────── */
#define CC_SUCCESS          1
#define CC_SHORT_PACKET     13

/* ── TRB control bits ───────────────────────────────────────────────── */
#define TRB_C        (1u<<0)
#define TRB_IOC      (1u<<5)
#define TRB_IDT      (1u<<6)
#define TRB_LK_TC    (1u<<1)
#define TRB_TR_DIR   (1u<<16)
#define TRB_BSR      (1u<<9)
#define TRB_TYPE(w)  (((w)>>10)&0x3Fu)
#define TRB_CC(w)    (((w)>>24)&0xFFu)
#define TRB_SLOT(w)  (((w)>>24)&0xFFu)

/* ── Port link states ───────────────────────────────────────────────── */
#define PLS_POLLING  7

/* ── Device speed ───────────────────────────────────────────────────── */
#define SPEED_FULL   1
#define SPEED_LOW    2
#define SPEED_HIGH   3
#define SPEED_SUPER  4

/* ── Ring ───────────────────────────────────────────────────────────── */
#define RING_ITEMS   16
#define RING_USABLE  (RING_ITEMS-1)
#define RING_BYTES   256

typedef struct { uint32_t w[4]; } trb_t;

typedef struct xhci_ring {
    trb_t    trbs[RING_ITEMS];
    trb_t    evt;
    uint32_t nidx;
    uint32_t cs;
    uint32_t eidx;
} ring_t;

#define XHCI_RING(p) ((ring_t*)((uint32_t)(p) & ~(uint32_t)(RING_BYTES-1)))

#define N_HID_BUFS   4
#define HID_BUF_SIZE 8

/* ── xHCI state ─────────────────────────────────────────────────────── */
typedef struct {
    uint32_t bar0;
    uint32_t op_base;
    uint32_t db_base;
    uint32_t ir_base;

    ring_t  *cmds;
    ring_t  *evts;
    ring_t  *ep0r;
    ring_t  *intr_ring;

    uint32_t *devs;
    uint32_t *dev_ctx;
    uint32_t *in_ctx;
    uint32_t *erst;

    uint8_t  *hid_buf;
    uint8_t  *desc_buf;

    volatile int report_ready;

    uint8_t  slot_id;
    uint8_t  port_num;
    uint8_t  kbd_port;   /* port where keyboard was found, 0 if none */
    uint8_t  dev_speed;
    uint8_t  n_ports;
    uint8_t  n_slots;
    int      intr_primed;
    uint32_t pool_base;
    uint32_t pool_end;
} xhci_t;

extern xhci_t g_xhci;

/* ── MMIO ───────────────────────────────────────────────────────────── */
static inline void     W32(uint32_t a, uint32_t v) { *(volatile uint32_t*)a = v; }
static inline uint32_t R32(uint32_t a)             { return *(volatile uint32_t*)a; }
static inline void     W64(uint32_t a, uint64_t v) {
    *(volatile uint32_t*)(a+0) = (uint32_t)v;
    *(volatile uint32_t*)(a+4) = (uint32_t)(v>>32);
}

/* ── API ────────────────────────────────────────────────────────────── */
int xhci_init(void);
int xhci_mbuff(uint8_t *btn, int8_t *dx, int8_t *dy);

#endif /* XHCI_H */
