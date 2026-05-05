#ifndef XHCI_REGS_H
#define XHCI_REGS_H
#include "xhci.h"

/* ═══════════════════════════════════════════════════════════════════════
   xhci_regs.h — all register offsets, derived from xhci_t MMIO bases.
   Usage: pass xhci_t *x, then use macros like R32(USBCMD(x)).
   ═══════════════════════════════════════════════════════════════════════ */

/* ── Capability registers (bar0) ────────────────────────────────────── */
#define CAPLENGTH(x)   ((x)->bar0 + 0x00)  /* bits[7:0] = cap length    */
#define HCIVERSION(x)  ((x)->bar0 + 0x02)  /* xHCI version              */
#define HCSPARAMS1(x)  ((x)->bar0 + 0x04)  /* #slots, #intrs, #ports    */
#define HCSPARAMS2(x)  ((x)->bar0 + 0x08)  /* IST, ERST, scratchpad     */
#define HCSPARAMS3(x)  ((x)->bar0 + 0x0C)  /* exit latencies            */
#define HCCPARAMS1(x)  ((x)->bar0 + 0x10)  /* AC64, BNC, CSZ, ...       */
#define DBOFF(x)       ((x)->bar0 + 0x14)  /* doorbell offset           */
#define RTSOFF(x)      ((x)->bar0 + 0x18)  /* runtime offset            */
#define HCCPARAMS2(x)  ((x)->bar0 + 0x1C)

/* ── Operational registers (op_base) ────────────────────────────────── */
#define USBCMD(x)      ((x)->op_base + 0x00)
#define USBSTS(x)      ((x)->op_base + 0x04)
#define PAGESIZE(x)    ((x)->op_base + 0x08)
#define DNCTRL(x)      ((x)->op_base + 0x14)
#define CRCR(x)        ((x)->op_base + 0x18)  /* 64-bit */
#define DCBAAP(x)      ((x)->op_base + 0x30)  /* 64-bit */
#define CONFIG(x)      ((x)->op_base + 0x38)

/* ── USBCMD bits ────────────────────────────────────────────────────── */
#define USBCMD_RUN     (1u<<0)
#define USBCMD_HCRST   (1u<<1)
#define USBCMD_INTE    (1u<<2)
#define USBCMD_HSEE    (1u<<3)
#define USBCMD_LHCRST  (1u<<7)
#define USBCMD_CSS     (1u<<8)
#define USBCMD_CRS     (1u<<9)
#define USBCMD_EWE     (1u<<10)
#define USBCMD_EU3S    (1u<<11)

/* ── USBSTS bits ────────────────────────────────────────────────────── */
#define USBSTS_HCH     (1u<<0)   /* Host Controller Halted   */
#define USBSTS_HSE     (1u<<2)   /* Host System Error        */
#define USBSTS_EINT    (1u<<3)   /* Event Interrupt          */
#define USBSTS_PCD     (1u<<4)   /* Port Change Detect       */
#define USBSTS_SSS     (1u<<8)   /* Save State Status        */
#define USBSTS_RSS     (1u<<9)   /* Restore State Status     */
#define USBSTS_SRE     (1u<<10)  /* Save/Restore Error       */
#define USBSTS_CNR     (1u<<11)  /* Controller Not Ready     */
#define USBSTS_HCE     (1u<<12)  /* Host Controller Error    */

/* ── Port registers — indexed from 0 ───────────────────────────────── */
#define PORTSC(x,n)    ((x)->op_base + 0x400 + (n)*0x10 + 0x00)
#define PORTPMSC(x,n)  ((x)->op_base + 0x400 + (n)*0x10 + 0x04)
#define PORTLI(x,n)    ((x)->op_base + 0x400 + (n)*0x10 + 0x08)
#define PORTHLPMC(x,n) ((x)->op_base + 0x400 + (n)*0x10 + 0x0C)

/* ── PORTSC bits ────────────────────────────────────────────────────── */
#define PORTSC_CCS     (1u<<0)   /* Current Connect Status   */
#define PORTSC_PED     (1u<<1)   /* Port Enabled/Disabled    */
#define PORTSC_OCA     (1u<<3)   /* Over-Current Active      */
#define PORTSC_PR      (1u<<4)   /* Port Reset               */
#define PORTSC_PLS(v)  (((v)>>5)&0xFu) /* Port Link State    */
#define PORTSC_PP      (1u<<9)   /* Port Power               */
#define PORTSC_SPEED(v)(((v)>>10)&0xFu)/* Port Speed         */
#define PORTSC_PIC(v)  (((v)>>14)&0x3u)/* Port Indicator     */
#define PORTSC_LWS     (1u<<16)  /* Port Link Write Strobe   */
#define PORTSC_CSC     (1u<<17)  /* Connect Status Change    */
#define PORTSC_PEC     (1u<<18)  /* Port Enable/Disable Chg  */
#define PORTSC_WRC     (1u<<19)  /* Warm Port Reset Change   */
#define PORTSC_OCC     (1u<<20)  /* Over-Current Change      */
#define PORTSC_PRC     (1u<<21)  /* Port Reset Change        */
#define PORTSC_PLC     (1u<<22)  /* Port Link State Change   */
#define PORTSC_CEC     (1u<<23)  /* Port Config Error Change */
#define PORTSC_CAS     (1u<<24)  /* Cold Attach Status       */
#define PORTSC_WCE     (1u<<25)  /* Wake on Connect          */
#define PORTSC_WDE     (1u<<26)  /* Wake on Disconnect       */
#define PORTSC_WOE     (1u<<27)  /* Wake on Over-Current     */
#define PORTSC_DR      (1u<<30)  /* Device Removable         */
#define PORTSC_WPR     (1u<<31)  /* Warm Port Reset          */
/* Write-1-to-clear bits mask (must preserve when writing) */
#define PORTSC_WTCBITS (PORTSC_CSC|PORTSC_PEC|PORTSC_WRC|PORTSC_OCC| \
                        PORTSC_PRC|PORTSC_PLC|PORTSC_CEC)

/* ── Doorbell register ──────────────────────────────────────────────── */
#define DOORBELL(x,slot) ((x)->db_base + (slot)*4)
/* DB target values */
#define DB_HOST_CMD    0          /* slot=0, target=0 → command ring    */
#define DB_EP0_OUT     1          /* slot=N, target=1 → EP0 out (ctrl)  */
#define DB_EP1_IN      3          /* slot=N, target=3 → EP1 IN (intr)   */

/* ── Interrupter registers (ir_base = runtime + 0x20 * n) ──────────── */
#define IR_IMAN(x)     ((x)->ir_base + 0x00)  /* Interrupt Mgmt       */
#define IR_IMOD(x)     ((x)->ir_base + 0x04)  /* Interrupt Moderation */
#define IR_ERSTSZ(x)   ((x)->ir_base + 0x08)  /* Event Ring Seg Size  */
#define IR_ERSTBA(x)   ((x)->ir_base + 0x10)  /* ERST Base Addr 64bit */
#define IR_ERDP(x)     ((x)->ir_base + 0x18)  /* Event Ring Deq Ptr   */

/* IR_IMAN bits */
#define IR_IMAN_IP     (1u<<0)   /* Interrupt Pending (W1C) */
#define IR_IMAN_IE     (1u<<1)   /* Interrupt Enable        */
/* IR_ERDP bits */
#define IR_ERDP_EHB    (1u<<3)   /* Event Handler Busy (W1C)*/

/* ── CONFIG register ────────────────────────────────────────────────── */
#define CONFIG_MAX_SLOTS_EN(n) ((n)&0xFF)

/* ── HCSPARAMS1 field extractors ────────────────────────────────────── */
#define HCSP1_MAXSLOTS(v) ((v)&0xFF)
#define HCSP1_MAXINTRS(v) (((v)>>8)&0x7FF)
#define HCSP1_MAXPORTS(v) (((v)>>24)&0xFF)

/* ── HCCPARAMS1 field extractors ────────────────────────────────────── */
#define HCCP1_AC64(v)  ((v)&1)        /* 64-bit addressing capable */
#define HCCP1_CSZ(v)   (((v)>>2)&1)   /* Context size: 0=32B 1=64B */

#endif /* XHCI_REGS_H */
