/**
 * kernel.c - Kernel principal do KryFX OS
 * x86 32-bit — chamado pelo bootloader/boot.asm
 */

#include <stdint.h>
#include <stddef.h>

/* ── GPU ─────────────────────────────────────────────────────
 * Estrutura real: kernel/gpu/INTEL/, kernel/gpu/NVIDIA/, kernel/gpu/AMD/
 * ─────────────────────────────────────────────────────────── */
#include "gpu/INTEL/intelgpu.h"
#include "gpu/NVIDIA/nvidiagpu.h"

/* ── USB ──────────────────────────────────────────────────── */
#include "ohci/ohci.h"
#include "ehci/ehci.h"
#include "xhci/xhci.h"

/* ── PS/2 ─────────────────────────────────────────────────── */
#include "PS2/ps2.h"

/* ── KryFX ───────────────────────────────────────────────── */
#include "kryfx/kryfx.h"

/* ============================================================
 * Tipos
 * ============================================================ */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* ============================================================
 * I/O ports
 * ============================================================ */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void io_wait(void) { outb(0x80, 0); }

/* ============================================================
 * Serial COM1 — debug
 * ============================================================ */
#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1+1, 0x00);
    outb(COM1+3, 0x80);
    outb(COM1+0, 0x03);
    outb(COM1+1, 0x00);
    outb(COM1+3, 0x03);
    outb(COM1+2, 0xC7);
    outb(COM1+4, 0x0B);
}
static void serial_putc(char c) {
    while (!(inb(COM1+5) & 0x20));
    outb(COM1, (u8)c);
}
static void kprint(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}
static void kprint_hex(u32 v) {
    const char *h = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    int i;
    for (i = 9; i >= 2; i--) { buf[i] = h[v & 0xF]; v >>= 4; }
    kprint(buf);
}

/* ============================================================
 * GDT — flat 32-bit
 * ============================================================ */
typedef struct __attribute__((packed)) {
    u16 limit_lo, base_lo;
    u8  base_mid, access, flags_limit_hi, base_hi;
} gdt_entry_t;

typedef struct __attribute__((packed)) { u16 limit; u32 base; } gdt_ptr_t;

static gdt_entry_t gdt[5];
static gdt_ptr_t   gdt_ptr;

static void gdt_set(int i, u32 base, u32 limit, u8 access, u8 flags) {
    gdt[i].base_lo        = (u16)(base & 0xFFFF);
    gdt[i].base_mid       = (u8)((base >> 16) & 0xFF);
    gdt[i].base_hi        = (u8)((base >> 24) & 0xFF);
    gdt[i].limit_lo       = (u16)(limit & 0xFFFF);
    gdt[i].flags_limit_hi = (u8)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    gdt[i].access         = access;
}

static void gdt_install(void) {
    gdt_set(0, 0, 0x00000, 0x00, 0x00);
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF); /* Kernel Code */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF); /* Kernel Data */
    gdt_set(3, 0, 0xFFFFF, 0xFA, 0xCF); /* User Code   */
    gdt_set(4, 0, 0xFFFFF, 0xF2, 0xCF); /* User Data   */
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (u32)&gdt;
    __asm__ volatile (
        "lgdt (%0)\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:\n\t"
        :: "r"(&gdt_ptr) : "eax"
    );
    kprint("[GDT] OK\n");
}

/* ============================================================
 * IDT
 * ============================================================ */
typedef struct __attribute__((packed)) {
    u16 offset_lo, selector;
    u8  zero, type_attr;
    u16 offset_hi;
} idt_entry_t;

typedef struct __attribute__((packed)) { u16 limit; u32 base; } idt_ptr_t;

#define IDT_ENTRIES 256
static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

static void idt_set_gate(u8 num, u32 handler, u16 sel, u8 flags) {
    idt[num].offset_lo = (u16)(handler & 0xFFFF);
    idt[num].offset_hi = (u16)((handler >> 16) & 0xFFFF);
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].type_attr = flags;
}

static volatile u32 g_ticks = 0;

static void __attribute__((interrupt)) isr_default(void *f)  { (void)f; outb(0x20,0x20); outb(0xA0,0x20); }
static void __attribute__((interrupt)) isr_timer(void *f)    { (void)f; g_ticks++; outb(0x20,0x20); }
static void __attribute__((interrupt)) isr_keyboard(void *f) {
    (void)f;
    u8 sc = inb(0x60);
    (void)sc;
    outb(0x20, 0x20);
}

static void idt_install(void) {
    u32 i;
    for (i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate((u8)i, (u32)isr_default, 0x08, 0x8E);
    idt_set_gate(0x20, (u32)isr_timer,    0x08, 0x8E);
    idt_set_gate(0x21, (u32)isr_keyboard, 0x08, 0x8E);
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (u32)&idt;
    __asm__ volatile ("lidt (%0)" :: "r"(&idt_ptr));
    kprint("[IDT] OK\n");
}

/* ============================================================
 * PIC 8259
 * ============================================================ */
static void pic_remap(void) {
    u8 m1 = inb(0x21), m2 = inb(0xA1);
    outb(0x20, 0x11); io_wait();
    outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait();
    outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait();
    outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait();
    outb(0xA1, 0x01); io_wait();
    outb(0x21, m1);
    outb(0xA1, m2);
    outb(0x21, 0xFC); /* Habilita só timer + teclado */
    outb(0xA1, 0xFF);
    kprint("[PIC] OK\n");
}

/* ============================================================
 * PIT 8253 — 100 Hz
 * ============================================================ */
static void pit_init(void) {
    u32 div = 1193182 / 100;
    outb(0x43, 0x36);
    outb(0x40, (u8)(div & 0xFF));
    outb(0x40, (u8)((div >> 8) & 0xFF));
    kprint("[PIT] 100Hz\n");
}

/* ============================================================
 * PMM bump allocator simples
 * ============================================================ */
static u32 pmm_next = 0x400000; /* 4MB */

void *pmm_alloc_aligned(u32 size, u32 align) {
    u32 addr = (pmm_next + align - 1) & ~(align - 1);
    pmm_next = addr + size;
    return (void *)addr;
}

/* dma_alloc usado pelos drivers */
void *dma_alloc(u64 size, u64 align, u64 *phys_out) {
    void *p = pmm_alloc_aligned((u32)size, (u32)align);
    if (phys_out) *phys_out = (u64)(u32)p;
    return p;
}

/* ============================================================
 * GPU
 * ============================================================ */
static intel_gpu_t  g_intel;
static nvidia_gpu_t g_nvidia;
static u8 g_gpu = 0;

static void gpu_init(void) {
    kprint("[GPU] Detectando...\n");

    if (intel_gpu_detect(&g_intel) == 0) {
        kprint("[GPU] Intel: ");
        kprint(intel_gpu_device_name(g_intel.device_id));
        kprint("\n");
        if (intel_gpu_init(&g_intel, g_intel.pci_bus,
                           g_intel.pci_slot, g_intel.pci_func) == 0) {
            intel_gpu_set_mode(&g_intel, &INTEL_MODE_1024x768_32);
            intel_gpu_clear(&g_intel, 0x001A1A2E);
            g_gpu = 1;
            kprint("[GPU] Intel OK — 1024x768\n");
            return;
        }
    }

    if (nvidia_gpu_detect(&g_nvidia) == 0) {
        kprint("[GPU] NVIDIA: ");
        kprint(nvidia_gpu_device_name(g_nvidia.device_id));
        kprint("\n");
        if (nvidia_gpu_init(&g_nvidia, g_nvidia.pci_bus,
                            g_nvidia.pci_slot, g_nvidia.pci_func) == 0) {
            nvidia_gpu_set_mode(&g_nvidia, &NVIDIA_MODE_1024x768_32);
            nvidia_gpu_clear(&g_nvidia, 0x001A1A2E);
            g_gpu = 2;
            kprint("[GPU] NVIDIA OK — 1024x768\n");
            return;
        }
    }

    kprint("[GPU] Nenhuma GPU suportada\n");
}

/* ============================================================
 * USB
 * ============================================================ */
static ohci_t g_ohci;
static ehci_t g_ehci;

static void usb_init(void) {
    kprint("[USB] Iniciando...\n");

    if (ohci_detect(&g_ohci) == 0) {
        if (ohci_init(&g_ohci, g_ohci.pci_bus,
                      g_ohci.pci_slot, g_ohci.pci_func) == 0) {
            ohci_scan_ports(&g_ohci);
            kprint("[USB] OHCI OK\n");
        }
    }

    if (ehci_detect(&g_ehci) == 0) {
        if (ehci_init(&g_ehci, g_ehci.pci_bus,
                      g_ehci.pci_slot, g_ehci.pci_func) == 0) {
            ehci_scan_ports(&g_ehci);
            kprint("[USB] EHCI OK\n");
        }
    }
}

/* ============================================================
 * Banner
 * ============================================================ */
static void banner(void) {
    kprint("\n");
    kprint(" _  __           _____  __\n");
    kprint("| |/ /_ __ _   _|  ___|  \\\n");
    kprint("| ' /| '__| | | | |_   \\ \\\n");
    kprint("| . \\| |  | |_| |  _|  / /\n");
    kprint("|_|\\_\\_|   \\__, |_|   /_/\n");
    kprint("            |___/  KryFX OS\n\n");
}

/* ============================================================
 * kernel_main — ponto de entrada
 * ============================================================ */
void kernel_main(void) {
    serial_init();
    banner();
    kprint("[KERNEL] KryFX OS iniciando...\n");

    gdt_install();
    idt_install();
    pic_remap();
    pit_init();

    __asm__ volatile ("sti");
    kprint("[KERNEL] Interrupcoes ON\n");

    gpu_init();
    usb_init();

    kprint("[KERNEL] Boot completo!\n");

    /* Loop principal */
    while (1) __asm__ volatile ("hlt");
}
