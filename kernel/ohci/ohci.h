/**
 * ohci.h - Driver OHCI (Open Host Controller Interface) para SO próprio
 * Compatível com USB 1.1 (Full Speed 12 Mbps / Low Speed 1.5 Mbps)
 *
 * Referências:
 *  - OpenHCI Specification 1.0a (Compaq/Microsoft/National Semiconductor)
 *  - Linux ohci-hcd driver (drivers/usb/host/ohci*)
 *  - OSDev Wiki: USB OHCI
 */

#ifndef OHCI_H
#define OHCI_H

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
 * Vendor/Device IDs comuns de controladores OHCI
 * ============================================================ */
#define PCI_CLASS_OHCI              0x0C0310  /* Class/SubClass/ProgIF USB OHCI */

/* ============================================================
 * Registradores MMIO OHCI (offset em relação a BAR0)
 * ============================================================ */

/* Revisão e controle */
#define OHCI_REG_REVISION           0x00  /* Versão do HC */
#define OHCI_REG_CONTROL            0x04  /* Controle geral */
#define OHCI_REG_CMD_STATUS         0x08  /* Comando / Status */
#define OHCI_REG_INTERRUPT_STATUS   0x0C  /* Status de interrupção */
#define OHCI_REG_INTERRUPT_ENABLE   0x10  /* Habilita interrupções */
#define OHCI_REG_INTERRUPT_DISABLE  0x14  /* Desabilita interrupções */

/* HCCA (Host Controller Communications Area) */
#define OHCI_REG_HCCA               0x18  /* Ponteiro físico para HCCA */

/* Listas de transferência */
#define OHCI_REG_PERIOD_CURRENT_ED  0x1C  /* ED periódico atual */
#define OHCI_REG_CONTROL_HEAD_ED    0x20  /* Head da lista Control */
#define OHCI_REG_CONTROL_CURRENT_ED 0x24  /* ED Control atual */
#define OHCI_REG_BULK_HEAD_ED       0x28  /* Head da lista Bulk */
#define OHCI_REG_BULK_CURRENT_ED    0x2C  /* ED Bulk atual */
#define OHCI_REG_DONE_HEAD          0x30  /* Done queue head */

/* Timing */
#define OHCI_REG_FM_INTERVAL        0x34  /* Frame interval */
#define OHCI_REG_FM_REMAINING       0x38  /* Frame remaining */
#define OHCI_REG_FM_NUMBER          0x3C  /* Frame number atual */
#define OHCI_REG_PERIODIC_START     0x40  /* Início do período */
#define OHCI_REG_LS_THRESHOLD       0x44  /* Threshold Low Speed */

/* Root Hub */
#define OHCI_REG_RH_DESCRIPTOR_A    0x48  /* Descritor A do root hub */
#define OHCI_REG_RH_DESCRIPTOR_B    0x4C  /* Descritor B do root hub */
#define OHCI_REG_RH_STATUS          0x50  /* Status do root hub */
#define OHCI_REG_RH_PORT_STATUS(n)  (0x54 + (n) * 4)  /* Status da porta n (0-based) */

/* ============================================================
 * Bits dos registradores
 * ============================================================ */

/* OHCI_REG_CONTROL */
#define OHCI_CTRL_CBSR              (3 << 0)   /* Control/Bulk Service Ratio */
#define OHCI_CTRL_PLE               (1 << 2)   /* Periodic List Enable */
#define OHCI_CTRL_IE                (1 << 3)   /* Isochronous Enable */
#define OHCI_CTRL_CLE               (1 << 4)   /* Control List Enable */
#define OHCI_CTRL_BLE               (1 << 5)   /* Bulk List Enable */
#define OHCI_CTRL_HCFS_MASK         (3 << 6)   /* Host Controller Functional State */
#define OHCI_CTRL_HCFS_RESET        (0 << 6)   /* USB Reset */
#define OHCI_CTRL_HCFS_RESUME       (1 << 6)   /* USB Resume */
#define OHCI_CTRL_HCFS_OPERATIONAL  (2 << 6)   /* USB Operational */
#define OHCI_CTRL_HCFS_SUSPEND      (3 << 6)   /* USB Suspend */
#define OHCI_CTRL_IR                (1 << 8)   /* Interrupt Routing (SMM) */
#define OHCI_CTRL_RWC               (1 << 9)   /* Remote Wakeup Connected */
#define OHCI_CTRL_RWE               (1 << 10)  /* Remote Wakeup Enable */

/* OHCI_REG_CMD_STATUS */
#define OHCI_STATUS_HCR             (1 << 0)   /* Host Controller Reset */
#define OHCI_STATUS_CLF             (1 << 1)   /* Control List Filled */
#define OHCI_STATUS_BLF             (1 << 2)   /* Bulk List Filled */
#define OHCI_STATUS_OCR             (1 << 3)   /* Ownership Change Request */
#define OHCI_STATUS_SOC_MASK        (3 << 16)  /* Scheduling Overrun Count */

/* OHCI_REG_INTERRUPT_STATUS / ENABLE / DISABLE */
#define OHCI_INTR_SO                (1 << 0)   /* Scheduling Overrun */
#define OHCI_INTR_WDH               (1 << 1)   /* Writeback Done Head */
#define OHCI_INTR_SF                (1 << 2)   /* Start of Frame */
#define OHCI_INTR_RD                (1 << 3)   /* Resume Detected */
#define OHCI_INTR_UE                (1 << 4)   /* Unrecoverable Error */
#define OHCI_INTR_FNO               (1 << 5)   /* Frame Number Overflow */
#define OHCI_INTR_RHSC              (1 << 6)   /* Root Hub Status Change */
#define OHCI_INTR_OC                (1 << 30)  /* Ownership Change */
#define OHCI_INTR_MIE               (1u << 31) /* Master Interrupt Enable */

/* OHCI_REG_FM_INTERVAL */
#define OHCI_FMI_FI_MASK            0x00003FFF /* Frame Interval */
#define OHCI_FMI_FSMPS_SHIFT        16         /* FS Max Packet Size */
#define OHCI_FMI_FIT                (1u << 31) /* Frame Interval Toggle */
#define OHCI_FM_INTERVAL_DEFAULT    0x2EDF     /* 11999 — USB 1.1 padrão */

/* OHCI_REG_PERIODIC_START */
#define OHCI_PERIODIC_START_DEFAULT 0x2A2F     /* 90% do frame interval */

/* OHCI_REG_RH_DESCRIPTOR_A */
#define OHCI_RHA_NDP_MASK           0xFF       /* Number of Downstream Ports */
#define OHCI_RHA_PSM                (1 << 8)   /* Power Switching Mode */
#define OHCI_RHA_NPS                (1 << 9)   /* No Power Switching */
#define OHCI_RHA_DT                 (1 << 10)  /* Device Type (compound) */
#define OHCI_RHA_OCPM               (1 << 11)  /* Overcurrent Protection Mode */
#define OHCI_RHA_NOCP               (1 << 12)  /* No Overcurrent Protection */
#define OHCI_RHA_POTPGT_SHIFT       24         /* PowerOn-to-PowerGood Time */

/* OHCI_REG_RH_STATUS */
#define OHCI_RHS_LPS                (1 << 0)   /* Local Power Status */
#define OHCI_RHS_OCI                (1 << 1)   /* Overcurrent Indicator */
#define OHCI_RHS_DRWE               (1u << 15) /* Device Remote Wakeup Enable */
#define OHCI_RHS_LPSC               (1u << 16) /* Local Power Status Change */
#define OHCI_RHS_OCIC               (1u << 17) /* Overcurrent Indicator Change */
#define OHCI_RHS_CRWE               (1u << 31) /* Clear Remote Wakeup Enable */

/* OHCI_REG_RH_PORT_STATUS */
#define OHCI_PORT_CCS               (1 << 0)   /* Current Connect Status */
#define OHCI_PORT_PES               (1 << 1)   /* Port Enable Status */
#define OHCI_PORT_PSS               (1 << 2)   /* Port Suspend Status */
#define OHCI_PORT_POCI              (1 << 3)   /* Port Overcurrent Indicator */
#define OHCI_PORT_PRS               (1 << 4)   /* Port Reset Status */
#define OHCI_PORT_PPS               (1 << 8)   /* Port Power Status */
#define OHCI_PORT_LSDA              (1 << 9)   /* Low Speed Device Attached */
#define OHCI_PORT_CSC               (1 << 16)  /* Connect Status Change */
#define OHCI_PORT_PESC              (1 << 17)  /* Port Enable Status Change */
#define OHCI_PORT_PSSC              (1 << 18)  /* Port Suspend Status Change */
#define OHCI_PORT_OCIC              (1 << 19)  /* Overcurrent Indicator Change */
#define OHCI_PORT_PRSC              (1 << 20)  /* Port Reset Status Change */

/* Comandos de escrita no port status */
#define OHCI_PORT_CCS_SET           (1 << 0)   /* SetPortEnable */
#define OHCI_PORT_PES_SET           (1 << 1)
#define OHCI_PORT_PSS_SET           (1 << 2)   /* SetPortSuspend */
#define OHCI_PORT_PRS_SET           (1 << 4)   /* SetPortReset */
#define OHCI_PORT_PPS_SET           (1 << 8)   /* SetPortPower */
#define OHCI_PORT_LSDA_CLR          (1 << 9)   /* ClearPortPower */
#define OHCI_PORT_CSC_CLR           (1 << 16)  /* ClearConnectStatusChange */

/* ============================================================
 * Estruturas de dados alinhadas (DMA — devem estar em memória física contígua)
 * ATENÇÃO: Todas as structs DMA devem estar alinhadas em 16 bytes!
 * ============================================================ */

/**
 * Transfer Descriptor (TD) — descreve uma transação USB.
 * Tamanho: 16 bytes, alinhamento: 16 bytes.
 */
typedef struct __attribute__((packed, aligned(16))) ohci_td {
    /* Word 0 — Flags */
    u32 flags;
    /* Word 1 — Buffer Pointer (início do buffer de dados) */
    u32 cbp;       /* Current Buffer Pointer */
    /* Word 2 — Next TD (ponteiro físico para o próximo TD) */
    u32 next_td;
    /* Word 3 — Buffer End */
    u32 be;        /* Buffer End */
} ohci_td_t;

/* Bits do campo flags do TD */
#define TD_CC_MASK          (0xF << 28)  /* Condition Code (resultado) */
#define TD_CC_NOERR         (0x0 << 28)
#define TD_CC_CRC           (0x1 << 28)
#define TD_CC_BITSTUFF      (0x2 << 28)
#define TD_CC_TOGGLE        (0x3 << 28)
#define TD_CC_STALL         (0x4 << 28)
#define TD_CC_NOTRESPONSE   (0x5 << 28)
#define TD_CC_PIDCHECK      (0x6 << 28)
#define TD_CC_DATAOVERRUN   (0x8 << 28)
#define TD_CC_DATAUNDERRUN  (0x9 << 28)
#define TD_CC_BUFFEROVER    (0xC << 28)
#define TD_CC_BUFFERUNDER   (0xD << 28)
#define TD_CC_NOTACCESSED   (0xF << 28)  /* Valor inicial (não processado) */
#define TD_EC_MASK          (3 << 26)    /* Error Count */
#define TD_T_MASK           (3 << 24)    /* Data Toggle */
#define TD_T_DATA0          (2 << 24)
#define TD_T_DATA1          (3 << 24)
#define TD_T_FROM_ED        (0 << 24)    /* Toggle herdado do ED */
#define TD_DI_MASK          (7 << 21)    /* Delay Interrupt */
#define TD_DI_NONE          (7 << 21)    /* Sem interrupção */
#define TD_DI_IMMEDIATE     (0 << 21)    /* Interrupção imediata */
#define TD_DP_MASK          (3 << 19)    /* Direction/PID */
#define TD_DP_SETUP         (0 << 19)    /* SETUP */
#define TD_DP_OUT           (1 << 19)    /* OUT */
#define TD_DP_IN            (2 << 19)    /* IN */
#define TD_R                (1 << 18)    /* Buffer Rounding */

/**
 * Isochronous Transfer Descriptor (ITD).
 * Tamanho: 32 bytes, alinhamento: 32 bytes.
 */
typedef struct __attribute__((packed, aligned(32))) ohci_itd {
    u32 flags;
    u32 bp0;       /* Buffer Page 0 */
    u32 next_td;
    u32 be;
    u16 offset[8]; /* Offsets/PSW para até 8 pacotes */
} ohci_itd_t;

/**
 * Endpoint Descriptor (ED) — descreve um endpoint USB.
 * Tamanho: 16 bytes, alinhamento: 16 bytes.
 */
typedef struct __attribute__((packed, aligned(16))) ohci_ed {
    /* Word 0 — Informações do endpoint */
    u32 flags;
    /* Word 1 — TailP: último TD na fila (físico) */
    u32 tail_td;
    /* Word 2 — HeadP: próximo TD a processar (físico) + bits de controle */
    u32 head_td;
    /* Word 3 — NextED: próximo ED na lista (físico) */
    u32 next_ed;
} ohci_ed_t;

/* Bits do campo flags do ED */
#define ED_FA_MASK          (0x7F << 0)   /* Function Address (endereço USB) */
#define ED_FA_SHIFT         0
#define ED_EN_MASK          (0xF << 7)    /* Endpoint Number */
#define ED_EN_SHIFT         7
#define ED_DIR_MASK         (3 << 11)     /* Direction */
#define ED_DIR_FROM_TD      (0 << 11)     /* Usa direção do TD */
#define ED_DIR_OUT          (1 << 11)
#define ED_DIR_IN           (2 << 11)
#define ED_SPEED_FULL       (0 << 13)     /* Full Speed */
#define ED_SPEED_LOW        (1 << 13)     /* Low Speed */
#define ED_SKIP             (1 << 14)     /* Pula este ED */
#define ED_FORMAT_ISO       (1 << 15)     /* Isochronous */
#define ED_MPS_MASK         (0x7FF << 16) /* Max Packet Size */
#define ED_MPS_SHIFT        16

/* Bits do campo head_td do ED */
#define ED_HEADP_HALT       (1 << 0)      /* Endpoint parado (erro) */
#define ED_HEADP_CARRY      (1 << 1)      /* Data Toggle carry */
#define ED_HEADP_PTR_MASK   (~0xFu)       /* Máscara do ponteiro */

/**
 * HCCA — Host Controller Communications Area.
 * Deve estar alinhada em 256 bytes e em memória física contígua.
 * Tamanho: 256 bytes.
 */
typedef struct __attribute__((packed, aligned(256))) ohci_hcca {
    u32 interrupt_table[32]; /* Tabela de EDs para interrupções periódicas */
    u16 frame_no;            /* Frame number atual (escrito pelo HC) */
    u16 pad1;
    u32 done_head;           /* Done queue head (escrito pelo HC) */
    u8  reserved[116];       /* Padding para 256 bytes */
    u32 frame_no_high;       /* Extensão do frame number */
} ohci_hcca_t;

/* ============================================================
 * Estrutura de transferência (abstração de alto nível)
 * ============================================================ */

#define OHCI_MAX_TDS_PER_TRANSFER   16

typedef enum {
    OHCI_PIPE_CONTROL     = 0,
    OHCI_PIPE_BULK        = 1,
    OHCI_PIPE_INTERRUPT   = 2,
    OHCI_PIPE_ISOCHRONOUS = 3,
} ohci_pipe_type_t;

typedef struct {
    ohci_pipe_type_t  type;
    u8   dev_addr;    /* Endereço USB do dispositivo */
    u8   ep_num;      /* Número do endpoint */
    u8   speed;       /* 0 = Full, 1 = Low */
    u16  max_packet;  /* Max packet size */
    u8   toggle;      /* Data toggle atual (0 ou 1) */
} ohci_pipe_t;

typedef struct {
    ohci_pipe_t *pipe;
    void        *data;
    u32          length;
    u8           setup[8];  /* Setup packet (apenas Control) */
    i32          status;    /* -1 = pending, 0 = ok, >0 = erro */
} ohci_transfer_t;

/* ============================================================
 * Estado interno do driver
 * ============================================================ */

#define OHCI_MAX_PORTS      15  /* Máximo de portas por root hub */
#define OHCI_TD_POOL_SIZE   64  /* Pool de TDs pré-alocados */
#define OHCI_ED_POOL_SIZE   32  /* Pool de EDs pré-alocados */

typedef struct {
    /* PCI */
    u8  pci_bus;
    u8  pci_slot;
    u8  pci_func;
    u16 vendor_id;
    u16 device_id;

    /* MMIO (BAR0) */
    volatile u8 *mmio_base;
    u64          mmio_size;

    /* HCCA (alocada pelo driver, passada ao HC) */
    ohci_hcca_t *hcca;
    u64          hcca_phys;

    /* Root Hub */
    u8   num_ports;
    u8   port_connected[OHCI_MAX_PORTS];
    u8   port_speed[OHCI_MAX_PORTS];  /* 0 = Full, 1 = Low */

    /* Pools de TD e ED (memória DMA) */
    ohci_td_t   *td_pool;
    u64          td_pool_phys;
    u8           td_used[OHCI_TD_POOL_SIZE];

    ohci_ed_t   *ed_pool;
    u64          ed_pool_phys;
    u8           ed_used[OHCI_ED_POOL_SIZE];

    /* Flags */
    u8   initialized;
    u8   smi_ownership; /* 1 se o BIOS estava usando SMM */
} ohci_t;

/* ============================================================
 * API pública
 * ============================================================ */

/**
 * Detecta controlador OHCI no barramento PCI.
 * Busca por Class Code 0x0C0310.
 *
 * @param ctrl   Estrutura a preencher
 * @return       0 se encontrado, -1 se não encontrado
 */
int ohci_detect(ohci_t *ctrl);

/**
 * Inicializa o controlador OHCI.
 * Reseta o HC, configura HCCA, habilita root hub.
 *
 * @param ctrl   Estado do controlador
 * @param bus    Barramento PCI
 * @param slot   Slot PCI
 * @param func   Função PCI
 * @return       0 em sucesso, negativo em erro
 */
int ohci_init(ohci_t *ctrl, u8 bus, u8 slot, u8 func);

/**
 * Varre as portas do root hub e detecta dispositivos conectados.
 * Chama reset em portas com dispositivo novo.
 *
 * @param ctrl   Estado do controlador
 * @return       Número de dispositivos detectados
 */
int ohci_scan_ports(ohci_t *ctrl);

/**
 * Reseta uma porta específica do root hub.
 *
 * @param ctrl   Estado do controlador
 * @param port   Índice da porta (0-based)
 * @return       0 em sucesso, -1 em timeout
 */
int ohci_port_reset(ohci_t *ctrl, u8 port);

/**
 * Habilita uma porta do root hub.
 *
 * @param ctrl   Estado do controlador
 * @param port   Índice da porta (0-based)
 */
void ohci_port_enable(ohci_t *ctrl, u8 port);

/**
 * Envia uma transferência Control (Setup + Data + Status).
 * Bloqueante — aguarda conclusão com timeout.
 *
 * @param ctrl     Estado do controlador
 * @param pipe     Pipe de destino
 * @param setup    8 bytes do setup packet
 * @param data     Buffer de dados (pode ser NULL para transferências sem dados)
 * @param length   Tamanho dos dados
 * @return         Bytes transferidos em sucesso, negativo em erro
 */
i32 ohci_control_transfer(ohci_t *ctrl, ohci_pipe_t *pipe,
                           const u8 setup[8], void *data, u32 length);

/**
 * Envia uma transferência Bulk.
 * Bloqueante — aguarda conclusão com timeout.
 *
 * @param ctrl     Estado do controlador
 * @param pipe     Pipe de destino
 * @param data     Buffer de dados
 * @param length   Tamanho
 * @param in       1 = IN (device→host), 0 = OUT (host→device)
 * @return         Bytes transferidos em sucesso, negativo em erro
 */
i32 ohci_bulk_transfer(ohci_t *ctrl, ohci_pipe_t *pipe,
                        void *data, u32 length, u8 in);

/**
 * Lê o descritor USB de um dispositivo recém-conectado.
 * Usa Control Transfer para GET_DESCRIPTOR.
 *
 * @param ctrl      Estado do controlador
 * @param dev_addr  Endereço USB do dispositivo (0 = padrão)
 * @param buf       Buffer de destino
 * @param len       Tamanho máximo a ler
 * @return          Bytes lidos, negativo em erro
 */
i32 ohci_get_descriptor(ohci_t *ctrl, u8 dev_addr, void *buf, u16 len);

/**
 * Lê registrador MMIO de 32 bits.
 */
u32  ohci_read32(const ohci_t *ctrl, u32 reg);

/**
 * Escreve registrador MMIO de 32 bits.
 */
void ohci_write32(ohci_t *ctrl, u32 reg, u32 val);

/**
 * Handler de interrupção — chame a partir do seu IRQ handler.
 * Processa WDH (done queue), RHSC (port change), erros.
 *
 * @param ctrl  Estado do controlador
 */
void ohci_irq_handler(ohci_t *ctrl);

/**
 * Desliga o controlador OHCI (suspende o HC).
 *
 * @param ctrl  Estado do controlador
 */
void ohci_shutdown(ohci_t *ctrl);

/* ============================================================
 * Helpers para construção de setup packets USB padrão
 * ============================================================ */

/** Preenche setup packet para GET_DESCRIPTOR */
static inline void usb_setup_get_descriptor(u8 *setup,
                                             u8 type, u8 index, u16 len)
{
    setup[0] = 0x80; /* bmRequestType: Device→Host, Standard, Device */
    setup[1] = 0x06; /* bRequest: GET_DESCRIPTOR */
    setup[2] = index;
    setup[3] = type;
    setup[4] = (u8)(len & 0xFF);
    setup[5] = (u8)(len >> 8);
    setup[6] = 0;
    setup[7] = 0;
}

/** Preenche setup packet para SET_ADDRESS */
static inline void usb_setup_set_address(u8 *setup, u8 addr) {
    setup[0] = 0x00; /* Host→Device, Standard, Device */
    setup[1] = 0x05; /* SET_ADDRESS */
    setup[2] = addr;
    setup[3] = 0;
    setup[4] = 0; setup[5] = 0;
    setup[6] = 0; setup[7] = 0;
}

/** Preenche setup packet para SET_CONFIGURATION */
static inline void usb_setup_set_configuration(u8 *setup, u8 config) {
    setup[0] = 0x00;
    setup[1] = 0x09; /* SET_CONFIGURATION */
    setup[2] = config;
    setup[3] = 0;
    setup[4] = 0; setup[5] = 0;
    setup[6] = 0; setup[7] = 0;
}

/* Tipos de descritor USB */
#define USB_DESC_DEVICE         0x01
#define USB_DESC_CONFIGURATION  0x02
#define USB_DESC_STRING         0x03
#define USB_DESC_INTERFACE      0x04
#define USB_DESC_ENDPOINT       0x05

#endif /* OHCI_H */
