#ifndef XHCI_CTX_H
#define XHCI_CTX_H
#include "xhci.h"

/* ═══════════════════════════════════════════════════════════════════════
   xhci_ctx.h — Input/Output context layout helpers (CSZ=0, 32-byte ctx)
   ═══════════════════════════════════════════════════════════════════════
   Memory layout of in_ctx[] (uint32_t array, 33 entries × 8 words):
     in_ctx[0..7]   = Input Control Context  (ICC)
     in_ctx[8..15]  = Slot Context           (DCI=0)
     in_ctx[16..23] = EP0 Context            (DCI=1)
     in_ctx[24..31] = EP1-OUT Context        (DCI=2)
     in_ctx[32..39] = EP1-IN  Context        (DCI=3)
     ...
   ═══════════════════════════════════════════════════════════════════════ */

/* Input Control Context fields */
#define ICC_DROP_FLAGS(x)   ((x)->in_ctx[0])  /* drop  context bitmask */
#define ICC_ADD_FLAGS(x)    ((x)->in_ctx[1])  /* add   context bitmask */

/* Slot context word accessors — w[0..7] relative to slot base */
#define SLOT_BASE(x)        ((x)->in_ctx + 8)
#define SLOT_W0(x)          ((x)->in_ctx[8+0])  /* speed, ctx_entries, route */
#define SLOT_W1(x)          ((x)->in_ctx[8+1])  /* port number [23:16]       */
#define SLOT_W2(x)          ((x)->in_ctx[8+2])  /* TT hub, port, think time  */
#define SLOT_W3(x)          ((x)->in_ctx[8+3])  /* device address, slot state*/

/* EP context word accessors — ep_ctx_base(dci) = in_ctx + 8 + dci*8 */
#define EP_BASE(x,dci)      ((x)->in_ctx + 8 + (dci)*8)
#define EP_W0(x,dci)        ((x)->in_ctx[8+(dci)*8+0])  /* interval, CErr, EP type */
#define EP_W1(x,dci)        ((x)->in_ctx[8+(dci)*8+1])  /* MPS, max burst, EP type  */
#define EP_W2(x,dci)        ((x)->in_ctx[8+(dci)*8+2])  /* dequeue ptr lo + DCS     */
#define EP_W3(x,dci)        ((x)->in_ctx[8+(dci)*8+3])  /* dequeue ptr hi           */
#define EP_W4(x,dci)        ((x)->in_ctx[8+(dci)*8+4])  /* avg TRB length           */

/* EP type values for EP_W1 bits[5:3] */
#define EP_TYPE_NOT_VALID  0
#define EP_TYPE_ISOCH_OUT  1
#define EP_TYPE_BULK_OUT   2
#define EP_TYPE_INTR_OUT   3
#define EP_TYPE_CTRL       4
#define EP_TYPE_ISOCH_IN   5
#define EP_TYPE_BULK_IN    6
#define EP_TYPE_INTR_IN    7

/* Build EP_W1 value: type, max_burst, mps */
#define EP_W1_VAL(type, max_burst, mps) \
    (((uint32_t)(type)<<3) | ((uint32_t)(max_burst)<<8) | ((uint32_t)(mps)<<16))

/* Build EP_W0 value: interval (xHCI encoded), cerr=3 */
#define EP_W0_VAL(interval) \
    (((uint32_t)(interval)<<16) | (3u<<1))

/* DCI (Doorbell Context Index) from endpoint address */
/* bEndpointAddress: bits[3:0]=ep number, bit[7]=direction (1=IN) */
#define EP_ADDR_TO_DCI(addr) \
    (((uint32_t)((addr)&0xFu)*2) + (((addr)>>7)&1u))

/* Zero entire input context */
static inline void ctx_zero(xhci_t *x) {
    uint32_t i;
    for (i = 0; i < 33*8; i++) x->in_ctx[i] = 0;
}

/* Copy one EP context from device (output) context into input context */
static inline void ctx_copy_ep(xhci_t *x, uint32_t dci) {
    uint32_t i;
    uint32_t *src = x->dev_ctx + dci*8;
    uint32_t *dst = x->in_ctx  + 8 + dci*8;
    for (i = 0; i < 8; i++) dst[i] = src[i];
}

/* Copy slot context from device context into input context */
static inline void ctx_copy_slot(xhci_t *x) {
    uint32_t i;
    uint32_t *src = x->dev_ctx;
    uint32_t *dst = x->in_ctx + 8;
    for (i = 0; i < 8; i++) dst[i] = src[i];
}

/*
 * xhci_interval() — convert bInterval (USB descriptor) to xHCI interval field.
 * For HS/SS interrupt: xHCI_interval = bInterval - 1  (already 2^(n-1) μframes)
 * For FS/LS interrupt: xHCI_interval = floor(log2(bInterval * 8)) clamped [3,10]
 */
static inline uint32_t xhci_interval(uint8_t speed, uint8_t bInterval) {
    if (speed == 3 || speed == 4) {
        /* HS or SS: bInterval is 1-16, xHCI = bInterval-1 */
        uint32_t v = (bInterval > 0) ? bInterval - 1 : 0;
        if (v > 15) v = 15;
        return v;
    } else {
        /* FS/LS: bInterval in ms, convert to 2^n 125μs frames */
        uint32_t ms8 = (uint32_t)bInterval * 8;
        uint32_t n = 0, v = 1;
        while (v < ms8 && n < 15) { v <<= 1; n++; }
        if (n < 3) n = 3;
        if (n > 10) n = 10;
        return n;
    }
}

#endif /* XHCI_CTX_H */
