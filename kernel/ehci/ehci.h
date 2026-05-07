/**
 * ehci.h - Driver EHCI (Enhanced Host Controller Interface) para SO próprio
 * Compatível com USB 2.0 (High Speed 480 Mbps)
 *
 * Referências:
 *  - EHCI Specification 1.0 (Intel)
 *  - Linux ehci-hcd driver (drivers/usb/host/ehci*)
 *  - OSDev Wiki: USB EHCI
 */

#ifndef EHCI_H
#define EHCI_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * Tipos básicos
 * ============================================================ */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;

/* ============================================================
 * PCI Class Code EHCI
 * ============================================================ */
#define PCI_CLASS_EHCI              0x0C0320  /* Class 0x0C / Sub 0x03 / ProgIF 0x20 */

/* ============================================================
 * Registradores de Capability (offset fixo em BAR0)
 * São read-only e descrevem as capacidades do HC
 * ============================================================ */
#define EHCI_CAP_CAPLENGTH          0x00  /* u8  — tamanho da região capability */
#define EHCI_CAP_HCIVERSION         0x02  /* u16 — versão BCD da interface (0x0100) */
#define EHCI_CAP_HCSPARAMS          0x04  /* u32 — Structural Parameters */
#define EHCI_CAP_HCCPARAMS          0x08  /* u32 — Capability Parameters */
#define EHCI_CAP_HCSP_PORTROUTE     0x0C  /* u64 — companion port routing */

/* Bits de HCSPARAMS */
#define EHCI_HCS_N_PORTS(x)        ((x) & 0xF)         /* Número de portas */
#define EHCI_HCS_PPC                (1 << 4)            /* Port Power Control */
#define EHCI_HCS_PRR                (1 << 7)            /* Port Routing Rules */
#define EHCI_HCS_N_PCC(x)          (((x) >> 8) & 0xF)  /* Portas por companion */
#define EHCI_HCS_N_CC(x)           (((x) >> 12) & 0xF) /* Número de companions */
#define EHCI_HCS_P_INDICATOR        (1 << 16)           /* Port Indicators */

/* Bits de HCCPARAMS */
#define EHCI_HCC_64BIT              (1 << 0)  /* 64-bit addressing */
#define EHCI_HCC_PFLF               (1 << 1)  /* Programmable Frame List Flag */
#define EHCI_HCC_ASPC               (1 << 2)  /* Async Schedule Park Capability */
#define EHCI_HCC_IST(x)             (((x) >> 4) & 0xF)  /* Isochronous Scheduling Threshold */
#define EHCI_HCC_EECP(x)            (((x) >> 8) & 0xFF) /* Extended Capabilities Pointer */

/* ============================================================
 * Registradores Operacionais
 * Base = BAR0 + CAPLENGTH (lido em runtime)
 * ============================================================ */
#define EHCI_OP_USBCMD              0x00  /* USB Command */
#define EHCI_OP_USBSTS              0x04  /* USB Status */
#define EHCI_OP_USBINTR             0x08  /* USB Interrupt Enable */
#define EHCI_OP_FRINDEX             0x0C  /* Frame Index (microframe counter) */
#define EHCI_OP_CTRLDSSEGMENT       0x10  /* Control Data Structure Segment (64-bit) */
#define EHCI_OP_PERIODICLISTBASE    0x14  /* Periodic Frame List Base Address */
#define EHCI_OP_ASYNCLISTADDR       0x18  /* Async List Address (ponteiro para QH head) */
#define EHCI_OP_CONFIGFLAG          0x40  /* Configure Flag */
#define EHCI_OP_PORTSC(n)           (0x44 + (n) * 4)  /* Port Status/Control porta n */

/* ============================================================
 * Bits dos registradores operacionais
 * ============================================================ */

/* EHCI_OP_USBCMD */
#define EHCI_CMD_RUN                (1 << 0)   /* Run/Stop */
#define EHCI_CMD_HCRESET            (1 << 1)   /* Host Controller Reset */
#define EHCI_CMD_FLS_MASK           (3 << 2)   /* Frame List Size */
#define EHCI_CMD_FLS_1024           (0 << 2)   /* 1024 entries */
#define EHCI_CMD_FLS_512            (1 << 2)   /* 512 entries */
#define EHCI_CMD_FLS_256            (2 << 2)   /* 256 entries */
#define EHCI_CMD_PSE                (1 << 4)   /* Periodic Schedule Enable */
#define EHCI_CMD_ASE                (1 << 5)   /* Async Schedule Enable */
#define EHCI_CMD_IAAD               (1 << 6)   /* Interrupt on Async Advance Doorbell */
#define EHCI_CMD_LHCR               (1 << 7)   /* Light Host Controller Reset */
#define EHCI_CMD_ASPMC(x)           ((x) << 8) /* Async Schedule Park Mode Count */
#define EHCI_CMD_ASPME              (1 << 11)  /* Async Schedule Park Mode Enable */
#define EHCI_CMD_ITC_MASK           (0xFF << 16) /* Interrupt Threshold Control */
#define EHCI_CMD_ITC_1MF            (0x01 << 16) /* 1 microframe */
#define EHCI_CMD_ITC_8MF            (0x08 << 16) /* 8 microframes = 1ms */

/* EHCI_OP_USBSTS */
#define EHCI_STS_INT                (1 << 0)   /* USB Interrupt */
#define EHCI_STS_ERR                (1 << 1)   /* USB Error Interrupt */
#define EHCI_STS_PCD                (1 << 2)   /* Port Change Detect */
#define EHCI_STS_FLR                (1 << 3)   /* Frame List Rollover */
#define EHCI_STS_HSE                (1 << 4)   /* Host System Error */
#define EHCI_STS_IAA                (1 << 5)   /* Interrupt on Async Advance */
#define EHCI_STS_HALTED             (1 << 12)  /* HC Halted */
#define EHCI_STS_RECLAMATION        (1 << 13)  /* Reclamation */
#define EHCI_STS_PSS                (1 << 14)  /* Periodic Schedule Status */
#define EHCI_STS_ASS                (1 << 15)  /* Async Schedule Status */

/* EHCI_OP_USBINTR */
#define EHCI_INTR_INT               (1 << 0)
#define EHCI_INTR_ERR               (1 << 1)
#define EHCI_INTR_PCD               (1 << 2)
#define EHCI_INTR_FLR               (1 << 3)
#define EHCI_INTR_HSE               (1 << 4)
#define EHCI_INTR_IAA               (1 << 5)

/* EHCI_OP_CONFIGFLAG */
#define EHCI_CF_FLAG                (1 << 0)   /* Configure Flag — roteia portas para EHCI */

/* EHCI_OP_PORTSC */
#define EHCI_PORT_CCS               (1 << 0)   /* Current Connect Status */
#define EHCI_PORT_CSC               (1 << 1)   /* Connect Status Change */
#define EHCI_PORT_PE                (1 << 2)   /* Port Enable */
#define EHCI_PORT_PEC               (1 << 3)   /* Port Enable/Disable Change */
#define EHCI_PORT_OCA               (1 << 4)   /* Over-current Active */
#define EHCI_PORT_OCC               (1 << 5)   /* Over-current Change */
#define EHCI_PORT_FPR               (1 << 6)   /* Force Port Resume */
#define EHCI_PORT_SUSPEND           (1 << 7)   /* Suspend */
#define EHCI_PORT_RESET             (1 << 8)   /* Port Reset */
#define EHCI_PORT_LS_MASK           (3 << 10)  /* Line Status */
#define EHCI_PORT_LS_SE0            (0 << 10)  /* SE0 (nada conectado ou reset) */
#define EHCI_PORT_LS_K              (1 << 10)  /* K-state (Low Speed) */
#define EHCI_PORT_LS_J              (2 << 10)  /* J-state */
#define EHCI_PORT_PP                (1 << 12)  /* Port Power */
#define EHCI_PORT_OWNER             (1 << 13)  /* Port Owner (0=EHCI, 1=companion) */
#define EHCI_PORT_IC_MASK           (3 << 14)  /* Port Indicator Control */
#define EHCI_PORT_TEST_MASK         (0xF << 16)/* Port Test Control */
#define EHCI_PORT_WCE               (1 << 20)  /* Wake on Connect Enable */
#define EHCI_PORT_WDE               (1 << 21)  /* Wake on Disconnect Enable */
#define EHCI_PORT_WKOC_E            (1 << 22)  /* Wake on Over-current Enable */

/* Bits de escrita no PORTSC (write-1-to-clear) */
#define EHCI_PORT_CHANGE_BITS       (EHCI_PORT_CSC | EHCI_PORT_PEC | EHCI_PORT_OCC)

/* ============================================================
 * Extended Capability — BIOS/OS Handoff (USBLEGSUP)
 * Acessado via PCI config space no offset EECP
 * ============================================================ */
#define EHCI_USBLEGSUP_ID           0x01      /* Cap ID = 1 */
#define EHCI_USBLEGSUP_BIOS_SEM     (1 << 16) /* BIOS owned semaphore */
#define EHCI_USBLEGSUP_OS_SEM       (1 << 24) /* OS owned semaphore */

/* ============================================================
 * Estruturas DMA
 * ATENÇÃO: Devem estar em memória física contígua e alinhadas!
 * ============================================================ */

/**
 * Queue Element Transfer Descriptor (qTD)
 * Tamanho: 32 bytes, alinhamento: 32 bytes
 */
typedef struct __attribute__((packed, aligned(32))) ehci_qtd {
    u32 next;           /* Ponteiro físico para próximo qTD (bit0 = Terminate) */
    u32 alt_next;       /* Alternate next qTD (usado para short packet) */
    u32 token;          /* Status, PID, toggle, tamanho */
    u32 buf[5];         /* Buffer Page Pointers (4KB cada) */
    /* Extensão 64 bits (high 32 bits de cada buf) */
    u32 buf_hi[5];
} ehci_qtd_t;

/* Bits do campo next / alt_next do qTD */
#define QTD_NEXT_TERMINATE          (1 << 0)  /* Lista termina aqui */

/* Bits do campo token do qTD */
#define QTD_STATUS_MASK             0xFF
#define QTD_STATUS_ACTIVE           (1 << 7)  /* HC deve processar */
#define QTD_STATUS_HALTED           (1 << 6)  /* Endpoint parado */
#define QTD_STATUS_BUFERR           (1 << 5)  /* Data Buffer Error */
#define QTD_STATUS_BABBLE           (1 << 4)  /* Babble Detected */
#define QTD_STATUS_XACTERR          (1 << 3)  /* Transaction Error */
#define QTD_STATUS_MISSED_MF        (1 << 2)  /* Missed Micro-Frame */
#define QTD_STATUS_SPLIT_XSTATE     (1 << 1)  /* Split Transaction State */
#define QTD_STATUS_PING             (1 << 0)  /* Ping State */
#define QTD_PID_OUT                 (0 << 8)
#define QTD_PID_IN                  (1 << 8)
#define QTD_PID_SETUP               (2 << 8)
#define QTD_CERR(n)                 ((n) << 10)  /* Error Counter (2 bits) */
#define QTD_CERR_3                  QTD_CERR(3)
#define QTD_C_PAGE(n)               ((n) << 12)  /* Current Page */
#define QTD_IOC                     (1 << 15)    /* Interrupt on Complete */
#define QTD_BYTES(n)                ((n) << 16)  /* Total Bytes to Transfer */
#define QTD_BYTES_MASK              (0x7FFF << 16)
#define QTD_TOGGLE                  (1u << 31)   /* Data Toggle */
#define QTD_TOGGLE_0                (0u << 31)
#define QTD_TOGGLE_1                (1u << 31)

/**
 * Queue Head (QH)
 * Tamanho: 48 bytes, alinhamento: 32 bytes
 */
typedef struct __attribute__((packed, aligned(32))) ehci_qh {
    /* Word 0 — Queue Head Horizontal Link Pointer */
    u32 next;       /* Ponteiro físico para próximo QH/ITD (bits 1:0 = type) */

    /* Word 1 — Endpoint Characteristics */
    u32 epchar;

    /* Word 2 — Endpoint Capabilities */
    u32 epcap;

    /* Word 3 — Current qTD Pointer */
    u32 current_qtd;

    /* Transfer Overlay — estado interno do HC (espelha o qTD atual) */
    u32 qtd_next;
    u32 qtd_alt_next;
    u32 qtd_token;
    u32 qtd_buf[5];
    u32 qtd_buf_hi[5];
} ehci_qh_t;

/* Bits do campo next do QH (horizontal link) */
#define QH_NEXT_TERMINATE           (1 << 0)
#define QH_TYPE_ITD                 (0 << 1)   /* Isochronous TD */
#define QH_TYPE_QH                  (1 << 1)   /* Queue Head */
#define QH_TYPE_SITD                (2 << 1)   /* Split-transaction Isochronous TD */
#define QH_TYPE_FSTN                (3 << 1)   /* Frame Span Traversal Node */

/* Bits de epchar (Endpoint Characteristics) */
#define QH_DEVADDR(n)               ((n) & 0x7F)
#define QH_INACTIVE                 (1 << 7)   /* Inativo no próximo microframe */
#define QH_ENDPT(n)                 (((n) & 0xF) << 8)
#define QH_EPS_FULL                 (0 << 12)  /* Full Speed */
#define QH_EPS_LOW                  (1 << 12)  /* Low Speed */
#define QH_EPS_HIGH                 (2 << 12)  /* High Speed */
#define QH_DTC                      (1 << 14)  /* Data Toggle Control (1=usa toggle do qTD) */
#define QH_H                        (1 << 15)  /* Head of Reclamation List */
#define QH_MAXPKT(n)                (((n) & 0x7FF) << 16)
#define QH_C                        (1 << 27)  /* Control Endpoint (Low/Full apenas) */
#define QH_RL(n)                    (((n) & 0xF) << 28) /* Nak Count Reload */

/* Bits de epcap (Endpoint Capabilities) */
#define QH_SMASK(n)                 ((n) & 0xFF)         /* Interrupt Schedule Mask */
#define QH_CMASK(n)                 (((n) & 0xFF) << 8)  /* Split Completion Mask */
#define QH_HUBADDR(n)               (((n) & 0x7F) << 16) /* Hub Address (TT) */
#define QH_PORTNUM(n)               (((n) & 0x7F) << 23) /* Port Number (TT) */
#define QH_MULT(n)                  (((n) & 0x3) << 30)  /* High-BW Pipe Multiplier */

/**
 * Isochronous Transfer Descriptor (iTD) — USB 2.0 High Speed
 * Tamanho: 64 bytes, alinhamento: 32 bytes
 */
typedef struct __attribute__((packed, aligned(32))) ehci_itd {
    u32 next;
    u32 transaction[8]; /* Um por microframe */
    u32 buf[7];
    u32 buf_hi[7];
} ehci_itd_t;

/**
 * Periodic Frame List — array de 1024 ponteiros físicos
 * Alinhamento: 4096 bytes (página)
 */
#define EHCI_FRAME_LIST_SIZE        1024
typedef u32 ehci_frame_list_t[EHCI_FRAME_LIST_SIZE];

/* ============================================================
 * Estruturas internas do driver
 * ============================================================ */

#define EHCI_MAX_PORTS      15
#define EHCI_QTD_POOL_SIZE  64
#define EHCI_QH_POOL_SIZE   32

typedef enum {
    EHCI_PIPE_CONTROL     = 0,
    EHCI_PIPE_BULK        = 1,
    EHCI_PIPE_INTERRUPT   = 2,
    EHCI_PIPE_ISOCHRONOUS = 3,
} ehci_pipe_type_t;

typedef struct {
    ehci_pipe_type_t type;
    u8   dev_addr;
    u8   ep_num;
    u8   speed;        /* 0=Full, 1=Low, 2=High */
    u16  max_packet;
    u8   toggle;       /* Data toggle atual */
    u8   hub_addr;     /* Para TT (Transaction Translator) */
    u8   hub_port;     /* Porta no hub TT */
} ehci_pipe_t;

typedef struct {
    /* PCI */
    u8   pci_bus;
    u8   pci_slot;
    u8   pci_func;
    u16  vendor_id;
    u16  device_id;

    /* Capability registers (BAR0) */
    volatile u8 *cap_base;
    u8           cap_length;    /* Offset para operational regs */
    u16          hci_version;
    u32          hcs_params;
    u32          hcc_params;

    /* Operational registers (BAR0 + cap_length) */
    volatile u8 *op_base;
    u64          mmio_size;

    /* Periodic Frame List */
    ehci_frame_list_t *frame_list;
    u64                frame_list_phys;

    /* Async list head QH (dummy head da lista circular) */
    ehci_qh_t *async_head;
    u64        async_head_phys;

    /* Pools DMA */
    ehci_qtd_t *qtd_pool;
    u64         qtd_pool_phys;
    u8          qtd_used[EHCI_QTD_POOL_SIZE];

    ehci_qh_t  *qh_pool;
    u64         qh_pool_phys;
    u8          qh_used[EHCI_QH_POOL_SIZE];

    /* Root Hub */
    u8   num_ports;
    u8   port_connected[EHCI_MAX_PORTS];
    u8   port_speed[EHCI_MAX_PORTS];  /* 0=Full,1=Low,2=High */

    /* Flags */
    u8   initialized;
    u8   has_64bit;   /* HC suporta endereçamento 64-bit */
    u8   eecp;        /* Extended Capability Pointer */
} ehci_t;

/* ============================================================
 * API pública
 * ============================================================ */

/**
 * Detecta controlador EHCI no barramento PCI.
 * Busca por Class Code 0x0C0320.
 */
int ehci_detect(ehci_t *ctrl);

/**
 * Inicializa o controlador EHCI.
 * Toma posse do BIOS, reseta o HC, configura frame list e async list.
 */
int ehci_init(ehci_t *ctrl, u8 bus, u8 slot, u8 func);

/**
 * Varre e inicializa portas do root hub.
 * Dispositivos Low/Full Speed são transferidos para companion (OHCI/UHCI).
 */
int ehci_scan_ports(ehci_t *ctrl);

/**
 * Reseta uma porta (somente High Speed permanece no EHCI).
 */
int ehci_port_reset(ehci_t *ctrl, u8 port);

/**
 * Transferência Control (bloqueante).
 */
i32 ehci_control_transfer(ehci_t *ctrl, ehci_pipe_t *pipe,
                           const u8 setup[8], void *data, u32 length);

/**
 * Transferência Bulk (bloqueante).
 * @param in  1 = IN (device→host), 0 = OUT (host→device)
 */
i32 ehci_bulk_transfer(ehci_t *ctrl, ehci_pipe_t *pipe,
                        void *data, u32 length, u8 in);

/**
 * Lê descritor USB do dispositivo (GET_DESCRIPTOR via Control).
 */
i32 ehci_get_descriptor(ehci_t *ctrl, u8 dev_addr, u8 speed,
                         void *buf, u16 len);

/**
 * Lê registrador operacional de 32 bits.
 */
u32  ehci_read32(const ehci_t *ctrl, u32 reg);

/**
 * Escreve registrador operacional de 32 bits.
 */
void ehci_write32(ehci_t *ctrl, u32 reg, u32 val);

/**
 * Handler de interrupção — chame do seu IRQ handler.
 */
void ehci_irq_handler(ehci_t *ctrl);

/**
 * Desliga o controlador EHCI.
 */
void ehci_shutdown(ehci_t *ctrl);

/* ============================================================
 * Helpers USB — setup packets padrão
 * ============================================================ */

static inline void usb_setup_get_descriptor(u8 *s, u8 type, u8 idx, u16 len) {
    s[0]=0x80; s[1]=0x06; s[2]=idx; s[3]=type;
    s[4]=(u8)(len&0xFF); s[5]=(u8)(len>>8); s[6]=0; s[7]=0;
}
static inline void usb_setup_set_address(u8 *s, u8 addr) {
    s[0]=0x00; s[1]=0x05; s[2]=addr; s[3]=0;
    s[4]=0; s[5]=0; s[6]=0; s[7]=0;
}
static inline void usb_setup_set_configuration(u8 *s, u8 cfg) {
    s[0]=0x00; s[1]=0x09; s[2]=cfg; s[3]=0;
    s[4]=0; s[5]=0; s[6]=0; s[7]=0;
}

#define USB_DESC_DEVICE         0x01
#define USB_DESC_CONFIGURATION  0x02
#define USB_DESC_STRING         0x03
#define USB_DESC_INTERFACE      0x04
#define USB_DESC_ENDPOINT       0x05

#endif /* EHCI_H */
