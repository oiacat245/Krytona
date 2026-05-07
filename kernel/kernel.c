/**
 * kernel.c - Kernel principal do KryFX OS
 *
 * Ponto de entrada chamado pelo boot.asm após configurar a stack.
 * Inicializa todos os subsistemas na ordem correta:
 *   1. Serial (debug)
 *   2. GDT / IDT
 *   3. PIC + IRQs
 *   4. Timer (PIT)
 *   5. Memória (PMM básico)
 *   6. GPU (Intel / NVIDIA / AMD — detecta automaticamente)
 *   7. USB (OHCI → EHCI → xHCI)
 *   8. PS/2 (teclado / mouse)
 *   9. KryFX subsystem
 */

#include <stdint.h>
#include <stddef.h>

/* ── GPU ─────────────────────────────────────────────── */
#include "gpu/intelgpu.h"
#include "gpu/nvidiagpu.h"

/* ── USB ─────────────────────────────────────────────── */
#include "ohci/ohci.h"
#include "ehci/ehci.h"

/* ── KryFX ───────────────────────────────────────────── */
#include "kryfx/kryfx.h"

/* ── PS/2 ────────────────────────────────────────────── */
#include "PS2/ps2.h"

/* ============================================================
 * Tipos básicos
 * ============================================================ */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/* ============================================================
 * Serial (porta COM1 — debug)
 * ============================================================ */
#define COM1 0x3F8

static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outl(u16 port, u32 val) {
    __asm__ volatile ("outl %0, %1" :: "a"(val), "Nd"(port));
}
static inline u32 inl(u16 port) {
    u32 ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void io_wait(void) { outb(0x80, 0); }

static void serial_init(void) {
    outb(COM1 + 1, 0x00); /* Desabilita interrupções */
    outb(COM1 + 3, 0x80); /* DLAB on */
    outb(COM1 + 0, 0x03); /* Baud 38400 (divisor lo) */
    outb(COM1 + 1, 0x00); /* Baud divisor hi */
    outb(COM1 + 3, 0x03); /* 8N1, DLAB off */
    outb(COM1 + 2, 0xC7); /* FIFO, clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B); /* IRQs enable, RTS/DSR set */
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20));
    outb(COM1, (u8)c);
}

static void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

/* Mini kprint para debug */
static void kprint(const char *s) { serial_puts(s); }

static void kprint_hex(u32 v) {
    const char *h = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    int i;
    for (i = 9; i >= 2; i--) {
        buf[i] = h[v & 0xF];
        v >>= 4;
    }
    kprint(buf);
}

/* ============================================================
 * GDT — Global Descriptor Table (flat 32-bit)
 * ============================================================ */
typedef struct __attribute__((packed)) {
    u16 limit_lo;
    u16 base_lo;
    u8  base_mid;
    u8  access;
    u8  flags_limit_hi;
    u8  base_hi;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    u16 limit;
    u32 base;
} gdt_ptr_t;

static gdt_entry_t gdt[5];
static gdt_ptr_t   gdt_ptr;

static void gdt_set(int idx, u32 base, u32 limit, u8 access, u8 flags) {
    gdt[idx].base_lo         = (u16)(base & 0xFFFF);
    gdt[idx].base_mid        = (u8)((base >> 16) & 0xFF);
    gdt[idx].base_hi         = (u8)((base >> 24) & 0xFF);
    gdt[idx].limit_lo        = (u16)(limit & 0xFFFF);
    gdt[idx].flags_limit_hi  = (u8)(((limit >> 16) & 0x0F) | (flags & 0xF0));
    gdt[idx].access          = access;
}

static void gdt_install(void) {
    gdt_set(0, 0, 0x00000, 0x00, 0x00); /* Null */
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF); /* Kernel Code (ring 0) */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF); /* Kernel Data (ring 0) */
    gdt_set(3, 0, 0xFFFFF, 0xFA, 0xCF); /* User Code  (ring 3) */
    gdt_set(4, 0, 0xFFFFF, 0xF2, 0xCF); /* User Data  (ring 3) */

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (u32)&gdt;

    __asm__ volatile (
        "lgdt (%0)\n\t"
        "mov $0x10, %%ax\n\t"  /* Kernel Data selector */
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"  /* Far jump para Kernel Code selector */
        "1:\n\t"
        :: "r"(&gdt_ptr) : "eax"
    );
    kprint("[GDT] OK\n");
}

/* ============================================================
 * IDT — Interrupt Descriptor Table
 * ============================================================ */
typedef struct __attribute__((packed)) {
    u16 offset_lo;
    u16 selector;
    u8  zero;
    u8  type_attr;
    u16 offset_hi;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    u16 limit;
    u32 base;
} idt_ptr_t;

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

/* Handler padrão (stub) */
static void __attribute__((interrupt)) isr_default(void *frame) {
    (void)frame;
    outb(0x20, 0x20); /* EOI PIC master */
    outb(0xA0, 0x20); /* EOI PIC slave  */
}

/* Handler de timer (IRQ0) */
static volatile u32 g_ticks = 0;
static void __attribute__((interrupt)) isr_timer(void *frame) {
    (void)frame;
    g_ticks++;
    outb(0x20, 0x20);
}

/* Handler de teclado (IRQ1) */
static void __attribute__((interrupt)) isr_keyboard(void *frame) {
    (void)frame;
    u8 scancode = inb(0x60);
    (void)scancode;
    /* Encaminha para driver PS/2 se disponível */
#ifdef PS2_H
    ps2_keyboard_irq(scancode);
#endif
    outb(0x20, 0x20);
}

static void idt_install(void) {
    u32 i;
    /* Preenche todos os gates com o handler padrão */
    for (i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate((u8)i, (u32)isr_default, 0x08, 0x8E);

    /* IRQ0 = Timer */
    idt_set_gate(0x20, (u32)isr_timer,    0x08, 0x8E);
    /* IRQ1 = Teclado */
    idt_set_gate(0x21, (u32)isr_keyboard, 0x08, 0x8E);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (u32)&idt;
    __asm__ volatile ("lidt (%0)" :: "r"(&idt_ptr));
    kprint("[IDT] OK\n");
}

/* ============================================================
 * PIC 8259 — remapeia IRQs para 0x20-0x2F
 * ============================================================ */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

static void pic_remap(void) {
    /* Salva máscaras */
    u8 m1 = inb(PIC1_DATA);
    u8 m2 = inb(PIC2_DATA);

    /* Inicialização em cascata */
    outb(PIC1_CMD,  0x11); io_wait();
    outb(PIC2_CMD,  0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait(); /* IRQ0-7  → INT 0x20-0x27 */
    outb(PIC2_DATA, 0x28); io_wait(); /* IRQ8-15 → INT 0x28-0x2F */
    outb(PIC1_DATA, 0x04); io_wait(); /* Slave em IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait(); /* Modo 8086 */
    outb(PIC2_DATA, 0x01); io_wait();

    /* Restaura máscaras */
    outb(PIC1_DATA, m1);
    outb(PIC2_DATA, m2);

    /* Máscara: habilita só timer (IRQ0) e teclado (IRQ1) por ora */
    outb(PIC1_DATA, 0xFC); /* 1111 1100 */
    outb(PIC2_DATA, 0xFF);

    kprint("[PIC] Remapeado 0x20-0x2F\n");
}

/* ============================================================
 * PIT 8253 — timer a 100 Hz
 * ============================================================ */
#define PIT_CH0   0x40
#define PIT_CMD   0x43
#define PIT_HZ    100
#define PIT_BASE  1193182

static void pit_init(void) {
    u32 divisor = PIT_BASE / PIT_HZ;
    outb(PIT_CMD, 0x36);              /* Canal 0, modo 3, binário */
    outb(PIT_CH0, (u8)(divisor & 0xFF));
    outb(PIT_CH0, (u8)((divisor >> 8) & 0xFF));
    kprint("[PIT] 100 Hz\n");
}

/* Delay simples baseado em ticks */
void kernel_sleep_ms(u32 ms) {
    u32 target = g_ticks + (ms / 10);
    while (g_ticks < target)
        __asm__ volatile ("hlt");
}

/* ============================================================
 * PMM básico — Physical Memory Manager (bump allocator)
 * Substitua por um PMM real com bitmap quando tiver mmap do boot
 * ============================================================ */
#define PMM_START   0x400000  /* 4 MB — abaixo reservado para kernel */
#define PMM_END     0x4000000 /* 64 MB */
#define PAGE_SIZE   4096

static u32 pmm_next = PMM_START;

void *pmm_alloc_aligned(u64 size, u64 align) {
    u32 addr = pmm_next;
    /* Alinha */
    if (align > 1)
        addr = (u32)((addr + (u32)align - 1) & ~((u32)align - 1));
    if (addr + (u32)size > PMM_END)
        return (void *)0; /* Sem memória */
    pmm_next = addr + (u32)size;
    return (void *)addr;
}

/* Wrapper para os drivers (identity map) */
void *dma_alloc_impl(u64 size, u64 align, u64 *phys_out) {
    void *ptr = pmm_alloc_aligned(size, align);
    if (phys_out) *phys_out = (u64)(u32)ptr;
    return ptr;
}

/* ============================================================
 * Detecção e inicialização de GPU
 * ============================================================ */
static intel_gpu_t  g_intel_gpu;
static nvidia_gpu_t g_nvidia_gpu;
static u8 g_gpu_type = 0; /* 0=nenhuma, 1=intel, 2=nvidia */

static void gpu_init(void) {
    kprint("[GPU] Detectando...\n");

    /* Tenta Intel */
    if (intel_gpu_detect(&g_intel_gpu) == 0) {
        kprint("[GPU] Intel detectada: ");
        kprint(intel_gpu_device_name(g_intel_gpu.device_id));
        kprint("\n");
        if (intel_gpu_init(&g_intel_gpu,
                           g_intel_gpu.pci_bus,
                           g_intel_gpu.pci_slot,
                           g_intel_gpu.pci_func) == 0)
        {
            intel_gpu_set_mode(&g_intel_gpu, &INTEL_MODE_1024x768_32);
            intel_gpu_clear(&g_intel_gpu, 0x001A1A2E); /* azul escuro */
            g_gpu_type = 1;
            kprint("[GPU] Intel inicializada 1024x768\n");
            return;
        }
    }

    /* Tenta NVIDIA */
    if (nvidia_gpu_detect(&g_nvidia_gpu) == 0) {
        kprint("[GPU] NVIDIA detectada: ");
        kprint(nvidia_gpu_device_name(g_nvidia_gpu.device_id));
        kprint("\n");
        if (nvidia_gpu_init(&g_nvidia_gpu,
                            g_nvidia_gpu.pci_bus,
                            g_nvidia_gpu.pci_slot,
                            g_nvidia_gpu.pci_func) == 0)
        {
            nvidia_gpu_set_mode(&g_nvidia_gpu, &NVIDIA_MODE_1024x768_32);
            nvidia_gpu_clear(&g_nvidia_gpu, 0x001A1A2E);
            g_gpu_type = 2;
            kprint("[GPU] NVIDIA inicializada 1024x768\n");
            return;
        }
    }

    kprint("[GPU] Nenhuma GPU suportada encontrada\n");
}

/* ============================================================
 * Detecção e inicialização USB
 * ============================================================ */
static ohci_t g_ohci;
static ehci_t g_ehci;
static u8 g_has_ohci = 0;
static u8 g_has_ehci = 0;

static void usb_init(void) {
    kprint("[USB] Inicializando...\n");

    /* OHCI (USB 1.1) */
    if (ohci_detect(&g_ohci) == 0) {
        kprint("[USB] OHCI encontrado\n");
        if (ohci_init(&g_ohci,
                      g_ohci.pci_bus,
                      g_ohci.pci_slot,
                      g_ohci.pci_func) == 0)
        {
            g_has_ohci = 1;
            int devs = ohci_scan_ports(&g_ohci);
            kprint("[USB] OHCI OK — portas: ");
            serial_putc('0' + g_ohci.num_ports);
            kprint(", devices HS: ");
            serial_putc('0' + devs);
            kprint("\n");
        }
    } else {
        kprint("[USB] OHCI nao encontrado\n");
    }

    /* EHCI (USB 2.0) */
    if (ehci_detect(&g_ehci) == 0) {
        kprint("[USB] EHCI encontrado\n");
        if (ehci_init(&g_ehci,
                      g_ehci.pci_bus,
                      g_ehci.pci_slot,
                      g_ehci.pci_func) == 0)
        {
            g_has_ehci = 1;
            int devs = ehci_scan_ports(&g_ehci);
            kprint("[USB] EHCI OK — portas: ");
            serial_putc('0' + g_ehci.num_ports);
            kprint(", devices HS: ");
            serial_putc('0' + devs);
            kprint("\n");

            /* Tenta ler descritor do primeiro dispositivo HS */
            if (devs > 0) {
                u8 desc[18];
                i32 r = ehci_get_descriptor(&g_ehci, 0, 2, desc, 18);
                if (r > 0) {
                    kprint("[USB] Descritor lido — VID=");
                    kprint_hex((u32)(desc[8] | (desc[9] << 8)));
                    kprint(" PID=");
                    kprint_hex((u32)(desc[10] | (desc[11] << 8)));
                    kprint("\n");
                }
            }
        }
    } else {
        kprint("[USB] EHCI nao encontrado\n");
    }
}

/* ============================================================
 * PS/2
 * ============================================================ */
static void ps2_subsystem_init(void) {
    kprint("[PS2] Inicializando...\n");
#ifdef PS2_H
    if (ps2_init() == 0)
        kprint("[PS2] OK\n");
    else
        kprint("[PS2] Falhou\n");
#else
    kprint("[PS2] Header nao incluido\n");
#endif
}

/* ============================================================
 * KryFX
 * ============================================================ */
static void kryfx_subsystem_init(void) {
    kprint("[KRYFX] Inicializando...\n");
#ifdef KRYFX_H
    kryfx_init();
    kprint("[KRYFX] OK\n");
#else
    kprint("[KRYFX] Header nao incluido\n");
#endif
}

/* ============================================================
 * Banner de boot
 * ============================================================ */
static void print_banner(void) {
    kprint("\n");
    kprint("  ██╗  ██╗██████╗ ██╗   ██╗███████╗██╗  ██╗\n");
    kprint("  ██║ ██╔╝██╔══██╗╚██╗ ██╔╝██╔════╝╚██╗██╔╝\n");
    kprint("  █████╔╝ ██████╔╝ ╚████╔╝ █████╗   ╚███╔╝ \n");
    kprint("  ██╔═██╗ ██╔══██╗  ╚██╔╝  ██╔══╝   ██╔██╗ \n");
    kprint("  ██║  ██╗██║  ██║   ██║   ██║      ██╔╝╚██╗\n");
    kprint("  ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚══════╝╚═╝  ╚═╝\n");
    kprint("  KryFX OS — x86 32-bit\n\n");
}

/* ============================================================
 * Loop principal do kernel
 * ============================================================ */
static void kernel_loop(void) {
    kprint("[KERNEL] Entrando no loop principal\n");

    u32 last_tick = 0;
    while (1) {
        __asm__ volatile ("hlt"); /* Aguarda interrupção */

        /* A cada segundo (100 ticks = 1s a 100Hz) */
        if (g_ticks - last_tick >= 100) {
            last_tick = g_ticks;
            /* Aqui você pode chamar um scheduler, atualizar display, etc */
        }
    }
}

/* ============================================================
 * Ponto de entrada — chamado pelo boot.asm
 * extern "C" para evitar name mangling caso use C++
 * ============================================================ */
void kernel_main(void) {
    /* 1. Serial de debug — primeiro de tudo */
    serial_init();
    print_banner();
    kprint("[KERNEL] Iniciando KryFX OS...\n");

    /* 2. GDT */
    gdt_install();

    /* 3. IDT */
    idt_install();

    /* 4. PIC */
    pic_remap();

    /* 5. Timer */
    pit_init();

    /* 6. Habilita interrupções */
    __asm__ volatile ("sti");
    kprint("[KERNEL] Interrupcoes habilitadas\n");

    /* 7. GPU */
    gpu_init();

    /* 8. USB */
    usb_init();

    /* 9. PS/2 */
    ps2_subsystem_init();

    /* 10. KryFX */
    kryfx_subsystem_init();

    kprint("[KERNEL] Boot completo!\n");

    /* Loop principal */
    kernel_loop();

    /* Nunca deve chegar aqui */
    __asm__ volatile ("cli; hlt");
}
