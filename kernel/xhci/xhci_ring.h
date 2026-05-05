#ifndef XHCI_RING_H
#define XHCI_RING_H
#include "xhci.h"

/* ═══════════════════════════════════════════════════════════════════════
   xhci_ring.h — TRB ring management (inline, header-only)

   Ring layout:
     Slots 0 .. RING_USABLE-1  usable for TRBs
     Slot  RING_USABLE          permanent LINK TRB (wraps back to slot 0)

   Cycle bit protocol:
     - cs starts at 1
     - Every TRB written gets the current cs bit
     - When nidx reaches RING_USABLE (the LINK slot), we:
         1. Update the LINK's cycle bit to match current cs
         2. Flip cs
         3. Wrap nidx to 0
     - Hardware follows the LINK TRB and toggles its cycle state
       because TC=1 is set in the LINK TRB.
   ═══════════════════════════════════════════════════════════════════════ */

/* Initialise a ring. Must be called before any use. */
static inline void ring_init(ring_t *r) {
    uint32_t i;
    for (i = 0; i < RING_ITEMS; i++) {
        r->trbs[i].w[0] = 0;
        r->trbs[i].w[1] = 0;
        r->trbs[i].w[2] = 0;
        r->trbs[i].w[3] = 0;
    }
    r->nidx        = 0;
    r->eidx        = 0;
    r->cs          = 1;
    r->evt.w[0]    = 0;
    r->evt.w[1]    = 0;
    r->evt.w[2]    = 0;
    r->evt.w[3]    = 0;

    /* Permanent LINK TRB: points back to trbs[0], TC=1, cs=1 */
    r->trbs[RING_USABLE].w[0] = (uint32_t)r->trbs;
    r->trbs[RING_USABLE].w[1] = 0;
    r->trbs[RING_USABLE].w[2] = 0;
    r->trbs[RING_USABLE].w[3] = (TR_LINK<<10) | TRB_LK_TC | TRB_C;
}

/*
 * ring_enq() — enqueue one TRB.
 * f = TRB-type and flags WITHOUT the cycle bit; cycle is added here.
 * Never enqueues into the LINK slot.
 */
static inline void ring_enq(ring_t *r,
                             uint32_t w0, uint32_t w1,
                             uint32_t w2, uint32_t f)
{
    trb_t *t;

    /* Wrap: update LINK cycle bit, flip cs, reset index */
    if (r->nidx == RING_USABLE) {
        r->trbs[RING_USABLE].w[3] =
            (TR_LINK<<10) | TRB_LK_TC | (r->cs ? TRB_C : 0);
        r->cs  ^= 1;
        r->nidx = 0;
    }

    t = &r->trbs[r->nidx];
    t->w[0] = w0;
    t->w[1] = w1;
    t->w[2] = w2;
    t->w[3] = f | (r->cs ? TRB_C : 0);
    r->nidx++;
}

/*
 * ring_busy() — true if the last-submitted TRB (cmd or ctrl)
 * has not yet been acknowledged by the hardware.
 */
static inline int ring_busy(ring_t *r) {
    return r->eidx != r->nidx;
}

/*
 * ring_slots_free() — number of usable slots available before
 * the ring would need to wrap. Useful for burst submissions.
 */
static inline uint32_t ring_slots_free(ring_t *r) {
    if (r->nidx <= RING_USABLE)
        return RING_USABLE - r->nidx;
    return 0;
}

#endif /* XHCI_RING_H */
