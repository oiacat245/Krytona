#ifndef XHCI_EVENTS_H
#define XHCI_EVENTS_H
#include "xhci.h"
#include "xhci_ring.h"
#include "xhci_regs.h"
#include "xhci_debug.h"

extern xhci_t g_xhci_kbd;

static inline void process_events(xhci_t *x)
{
ring_t *ev = x->evts;
for (;;) {
trb_t   *e    = &ev->trbs[ev->nidx];
uint32_t ctrl = e->w[3];
if ((ctrl & TRB_C) != (ev->cs ? 1u : 0u)) break;
uint32_t type = TRB_TYPE(ctrl);
uint32_t cc   = TRB_CC(e->w[2]);
        /* LINK TRB on event ring = wrap sentinel, stop processing */
if (type == TR_LINK) break;
if (type == ER_TRANSFER || type == ER_CMD_DONE) {
uint32_t rptr = e->w[0];
            /* Check mouse pool OR kbd pool */
if ((rptr & 0xFu) == 0 &&
                (cc == CC_SUCCESS || cc == CC_SHORT_PACKET) &&
                ((rptr >= x->pool_base && rptr < x->pool_end) ||
                 (rptr >= g_xhci_kbd.pool_base && rptr < g_xhci_kbd.pool_end))) {
trb_t  *rtrb = (trb_t *)rptr;
ring_t *ring = XHCI_RING(rtrb);
ring->evt = *e;
if (ring == x->intr_ring) {
x->report_ready = 1;
                } else if (ring == g_xhci_kbd.intr_ring) {
                    g_xhci_kbd.report_ready = 1;
                } else {
uint32_t idx = (uint32_t)(rtrb - ring->trbs);
if (idx < RING_USABLE)
ring->eidx = idx + 1;
                }
            }
        }
ev->nidx++;
if (ev->nidx == RING_ITEMS) { ev->nidx = 0; ev->cs ^= 1; }
W32(IR_ERDP(x), (uint32_t)&ev->trbs[ev->nidx] | IR_ERDP_EHB);
W32(IR_ERDP(x)+4, 0);
    }
}
#endif /* XHCI_EVENTS_H */
