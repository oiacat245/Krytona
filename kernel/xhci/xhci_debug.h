#ifndef XHCI_DEBUG_H
#define XHCI_DEBUG_H
#include "xhci.h"
#include "../../include/serial.h"

/* ═══════════════════════════════════════════════════════════════════════
   xhci_debug.h — diagnostic helpers
   Compile with -DXHCI_DEBUG=0 to strip all output.
   ═══════════════════════════════════════════════════════════════════════ */

/*
 * Fallback definitions so IntelliSense and compilers that don't see
 * serial.h still parse this file without errors.
 */
#ifndef DBG
#  define DBG(s)      ((void)0)
#endif
#ifndef DBGX
#  define DBGX(s,v)   ((void)(v))
#endif
#ifndef DBGD
#  define DBGD(s,v)   ((void)(v))
#endif

#ifndef XHCI_DEBUG
#  define XHCI_DEBUG 1
#endif

#if XHCI_DEBUG

/* Dump all four words of a TRB */
static inline void dbg_trb(const char *tag, trb_t *t) {
    serial_puts(tag); serial_putc('\n');
    DBGX("  w0=",   t->w[0]);
    DBGX("  w1=",   t->w[1]);
    DBGX("  w2=",   t->w[2]);
    DBGX("  w3=",   t->w[3]);
    DBGD("  type=", TRB_TYPE(t->w[3]));
    DBGD("  cc=",   TRB_CC(t->w[2]));
}

/* Dump ring bookkeeping state */
static inline void dbg_ring(const char *tag, ring_t *r) {
    serial_puts(tag); serial_putc('\n');
    DBGX("  base=", (uint32_t)r->trbs);
    DBGD("  nidx=", r->nidx);
    DBGD("  eidx=", r->eidx);
    DBGD("  cs=",   r->cs);
}

/* Dump a summary of the controller state */
static inline void dbg_xhci(xhci_t *x) {
    DBG("=== xhci state ===");
    DBGX("op_base=",  x->op_base);
    DBGX("db_base=",  x->db_base);
    DBGX("ir_base=",  x->ir_base);
    DBGD("slot_id=",  (uint32_t)x->slot_id);
    DBGD("port_num=", (uint32_t)x->port_num);
    DBGD("speed=",    (uint32_t)x->dev_speed);
    dbg_ring("  cmds", x->cmds);
    dbg_ring("  evts", x->evts);
    dbg_ring("  ep0r", x->ep0r);
    dbg_ring("  intr", x->intr_ring);
}

/* Dump first n bytes of a buffer as decimal */
static inline void dbg_buf(const char *tag, uint8_t *buf, uint32_t n) {
    uint32_t i;
    serial_puts(tag); serial_putc('\n');
    for (i = 0; i < n; i++) DBGD("  b=", (uint32_t)buf[i]);
}

#else /* XHCI_DEBUG == 0 — zero overhead */

static inline void dbg_trb(const char *tag, trb_t *t)
    { (void)tag; (void)t; }
static inline void dbg_ring(const char *tag, ring_t *r)
    { (void)tag; (void)r; }
static inline void dbg_xhci(xhci_t *x)
    { (void)x; }
static inline void dbg_buf(const char *tag, uint8_t *b, uint32_t n)
    { (void)tag; (void)b; (void)n; }

#endif /* XHCI_DEBUG */

#endif /* XHCI_DEBUG_H */
