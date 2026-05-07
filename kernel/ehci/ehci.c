/**
 * ehci.c - Implementação do driver EHCI USB 2.0 para SO próprio
 *
 * Referências:
 *  - EHCI Specification 1.0 (Intel)
 *  - Linux ehci-hcd (drivers/usb/host/ehci-hcd.c)
 *  - OSDev Wiki: USB EHCI
 */

#include "ehci.h"

/* ============================================================
 * Stubs do kernel — substitua pelas suas implementações
 * ============================================================ */

static inline u32 pci_read32(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 addr = (1u << 31) |
               ((u32)bus  << 16) |
               ((u32)slot << 11) |
               ((u32)func <<  8) |
               (offset & 0xFC);
    (void)addr;
    /* outl(addr, 0xCF8); return inl(0xCFC); */
    return 0;
}
static inline void pci_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 val) {
    u32 addr = (1u << 31) |
               ((u32)bus  << 16) |
               ((u32)slot << 11) |
               ((u32)func <<  8) |
               (offset & 0xFC);
    (void)addr; (void)val;
}

static inline void *dma_alloc(u64 size, u64 align, u64 *phys_out) {
    (void)size; (void)align;
    *phys_out = 0;
    return (void *)0; /* substituir: pmm_alloc_aligned(size, align) */
}
static inline void *phys_to_virt(u64 p) { return (void *)(uintptr_t)p; }
static inline u64   virt_to_phys(const void *v) { return (u64)(uintptr_t)v; }

static inline void udelay(u32 us) {
    volatile u32 i;
    for (i = 0; i < us * 1000; i++) __asm__ volatile("nop");
}
static inline void mdelay(u32 ms) { udelay(ms * 1000); }

static void *ehci_memset(void *d, int v, size_t n) {
    u8 *p = (u8 *)d; while (n--) *p++ = (u8)v; return d;
}
static void *ehci_memcpy(void *d, const void *s, size_t n) {
    u8 *dp=(u8*)d; const u8 *sp=(const u8*)s; while(n--) *dp++=*sp++; return d;
}

/* ============================================================
 * PCI helpers
 * ============================================================ */
#define PCI_VENDOR_ID   0x00
#define PCI_COMMAND     0x04
#define PCI_CLASS_REV   0x08
#define PCI_BAR0        0x10
#define PCI_CMD_MEMORY  (1<<1)
#define PCI_CMD_MASTER  (1<<2)

/* ============================================================
 * Leitura / escrita operacional
 * ============================================================ */

u32 ehci_read32(const ehci_t *ctrl, u32 reg) {
    return *((volatile u32 *)(ctrl->op_base + reg));
}
void ehci_write32(ehci_t *ctrl, u32 reg, u32 val) {
    *((volatile u32 *)(ctrl->op_base + reg)) = val;
    __asm__ volatile("" ::: "memory");
}

static u32 ehci_cap_read32(const ehci_t *ctrl, u32 reg) {
    return *((volatile u32 *)(ctrl->cap_base + reg));
}
static u16 ehci_cap_read16(const ehci_t *ctrl, u32 reg) {
    return *((volatile u16 *)(ctrl->cap_base + reg));
}
static u8 ehci_cap_read8(const ehci_t *ctrl, u32 reg) {
    return *((volatile u8 *)(ctrl->cap_base + reg));
}

static int ehci_wait_reg(ehci_t *ctrl, u32 reg,
                         u32 mask, u32 expected, u32 timeout_ms)
{
    u32 i;
    for (i = 0; i < timeout_ms; i++) {
        if ((ehci_read32(ctrl, reg) & mask) == expected) return 0;
        mdelay(1);
    }
    return -1;
}

/* ============================================================
 * Pool de qTD / QH
 * ============================================================ */

static ehci_qtd_t *qtd_alloc(ehci_t *ctrl, u64 *phys_out) {
    u32 i;
    for (i = 0; i < EHCI_QTD_POOL_SIZE; i++) {
        if (!ctrl->qtd_used[i]) {
            ctrl->qtd_used[i] = 1;
            ehci_qtd_t *q = &ctrl->qtd_pool[i];
            ehci_memset(q, 0, sizeof(ehci_qtd_t));
            q->next     = QTD_NEXT_TERMINATE;
            q->alt_next = QTD_NEXT_TERMINATE;
            *phys_out = ctrl->qtd_pool_phys + i * sizeof(ehci_qtd_t);
            return q;
        }
    }
    return (void *)0;
}
static void qtd_free(ehci_t *ctrl, ehci_qtd_t *q) {
    u32 idx = (u32)(q - ctrl->qtd_pool);
    if (idx < EHCI_QTD_POOL_SIZE) ctrl->qtd_used[idx] = 0;
}

static ehci_qh_t *qh_alloc(ehci_t *ctrl, u64 *phys_out) {
    u32 i;
    for (i = 0; i < EHCI_QH_POOL_SIZE; i++) {
        if (!ctrl->qh_used[i]) {
            ctrl->qh_used[i] = 1;
            ehci_qh_t *q = &ctrl->qh_pool[i];
            ehci_memset(q, 0, sizeof(ehci_qh_t));
            *phys_out = ctrl->qh_pool_phys + i * sizeof(ehci_qh_t);
            return q;
        }
    }
    return (void *)0;
}
static void qh_free(ehci_t *ctrl, ehci_qh_t *q) {
    u32 idx = (u32)(q - ctrl->qh_pool);
    if (idx < EHCI_QH_POOL_SIZE) ctrl->qh_used[idx] = 0;
}

/* ============================================================
 * Detecção PCI
 * ============================================================ */

int ehci_detect(ehci_t *ctrl) {
    u8 bus, slot, func;
    for (bus=0; bus<8; bus++) {
        for (slot=0; slot<32; slot++) {
            for (func=0; func<8; func++) {
                u32 id = pci_read32(bus, slot, func, PCI_VENDOR_ID);
                if ((id & 0xFFFF) == 0xFFFF) continue;
                u32 cls = pci_read32(bus, slot, func, PCI_CLASS_REV);
                u8 base    = (u8)(cls >> 24);
                u8 sub     = (u8)((cls >> 16) & 0xFF);
                u8 prog_if = (u8)((cls >>  8) & 0xFF);
                if (base == 0x0C && sub == 0x03 && prog_if == 0x20) {
                    ctrl->vendor_id = (u16)(id & 0xFFFF);
                    ctrl->device_id = (u16)(id >> 16);
                    ctrl->pci_bus   = bus;
                    ctrl->pci_slot  = slot;
                    ctrl->pci_func  = func;
                    return 0;
                }
            }
        }
    }
    return -1;
}

/* ============================================================
 * BIOS Handoff — toma posse do HC via USBLEGSUP
 * ============================================================ */

static void ehci_bios_handoff(ehci_t *ctrl) {
    if (!ctrl->eecp) return;

    u32 legsup = pci_read32(ctrl->pci_bus, ctrl->pci_slot,
                             ctrl->pci_func, ctrl->eecp);

    /* Verifica se é realmente USBLEGSUP (cap ID = 1) */
    if ((legsup & 0xFF) != EHCI_USBLEGSUP_ID) return;

    if (legsup & EHCI_USBLEGSUP_BIOS_SEM) {
        /* Pede posse: seta OS semaphore */
        pci_write32(ctrl->pci_bus, ctrl->pci_slot, ctrl->pci_func,
                    ctrl->eecp, legsup | EHCI_USBLEGSUP_OS_SEM);

        /* Aguarda BIOS liberar (até 1s) */
        u32 timeout = 1000;
        while (timeout--) {
            legsup = pci_read32(ctrl->pci_bus, ctrl->pci_slot,
                                ctrl->pci_func, ctrl->eecp);
            if (!(legsup & EHCI_USBLEGSUP_BIOS_SEM)) break;
            mdelay(1);
        }
        /* Força: desabilita SMI legado no USBLEGCTLSTS */
        u32 ctlsts = pci_read32(ctrl->pci_bus, ctrl->pci_slot,
                                ctrl->pci_func, ctrl->eecp + 4);
        ctlsts &= ~0x1F;   /* Desabilita todos os SMI enables */
        ctlsts |=  0x1F << 16; /* Limpa status bits */
        pci_write32(ctrl->pci_bus, ctrl->pci_slot, ctrl->pci_func,
                    ctrl->eecp + 4, ctlsts);
    }
}

/* ============================================================
 * Inicialização
 * ============================================================ */

int ehci_init(ehci_t *ctrl, u8 bus, u8 slot, u8 func) {
    ehci_memset(ctrl, 0, sizeof(ehci_t));
    ctrl->pci_bus = bus; ctrl->pci_slot = slot; ctrl->pci_func = func;

    u32 id = pci_read32(bus, slot, func, PCI_VENDOR_ID);
    ctrl->vendor_id = (u16)(id & 0xFFFF);
    ctrl->device_id = (u16)(id >> 16);

    /* Habilita PCI */
    u32 cmd = pci_read32(bus, slot, func, PCI_COMMAND);
    cmd |= PCI_CMD_MEMORY | PCI_CMD_MASTER;
    pci_write32(bus, slot, func, PCI_COMMAND, cmd);

    /* BAR0 = Capability + Operational registers */
    u32 bar0 = pci_read32(bus, slot, func, PCI_BAR0);
    u64 mmio_phys = (u64)(bar0 & ~0xFu);
    ctrl->mmio_size = 4096;
    ctrl->cap_base  = (volatile u8 *)phys_to_virt(mmio_phys);

    /* Lê capability registers */
    ctrl->cap_length  = ehci_cap_read8 (ctrl, EHCI_CAP_CAPLENGTH);
    ctrl->hci_version = ehci_cap_read16(ctrl, EHCI_CAP_HCIVERSION);
    ctrl->hcs_params  = ehci_cap_read32(ctrl, EHCI_CAP_HCSPARAMS);
    ctrl->hcc_params  = ehci_cap_read32(ctrl, EHCI_CAP_HCCPARAMS);

    /* Operational base = cap_base + cap_length */
    ctrl->op_base   = ctrl->cap_base + ctrl->cap_length;
    ctrl->has_64bit = (ctrl->hcc_params & EHCI_HCC_64BIT) ? 1 : 0;
    ctrl->eecp      = (u8)EHCI_HCC_EECP(ctrl->hcc_params);
    ctrl->num_ports = (u8)EHCI_HCS_N_PORTS(ctrl->hcs_params);
    if (ctrl->num_ports > EHCI_MAX_PORTS)
        ctrl->num_ports = EHCI_MAX_PORTS;

    /* Toma posse do BIOS */
    ehci_bios_handoff(ctrl);

    /* ---- Reset do HC ---- */
    /* Para o HC antes de resetar */
    u32 usbcmd = ehci_read32(ctrl, EHCI_OP_USBCMD);
    usbcmd &= ~EHCI_CMD_RUN;
    ehci_write32(ctrl, EHCI_OP_USBCMD, usbcmd);
    ehci_wait_reg(ctrl, EHCI_OP_USBSTS, EHCI_STS_HALTED, EHCI_STS_HALTED, 100);

    /* Reseta */
    ehci_write32(ctrl, EHCI_OP_USBCMD, EHCI_CMD_HCRESET);
    if (ehci_wait_reg(ctrl, EHCI_OP_USBCMD, EHCI_CMD_HCRESET, 0, 100) != 0)
        return -1;

    /* ---- Configura 64-bit segment (sempre 0 para < 4GB) ---- */
    if (ctrl->has_64bit)
        ehci_write32(ctrl, EHCI_OP_CTRLDSSEGMENT, 0);

    /* ---- Aloca Periodic Frame List (1024 entradas × 4 bytes = 4KB) ---- */
    ctrl->frame_list = (ehci_frame_list_t *)dma_alloc(
        sizeof(ehci_frame_list_t), 4096, &ctrl->frame_list_phys);
    if (!ctrl->frame_list) return -2;

    /* Preenche frame list com terminators */
    u32 i;
    for (i = 0; i < EHCI_FRAME_LIST_SIZE; i++)
        (*ctrl->frame_list)[i] = QTD_NEXT_TERMINATE;

    /* ---- Aloca Async List head QH (dummy circular) ---- */
    ctrl->async_head = qh_alloc(ctrl, &ctrl->async_head_phys);
    /* qh_alloc ainda não está disponível — aloca diretamente */
    ctrl->qh_pool = (ehci_qh_t *)dma_alloc(
        sizeof(ehci_qh_t) * EHCI_QH_POOL_SIZE, 32, &ctrl->qh_pool_phys);
    if (!ctrl->qh_pool) return -3;
    ehci_memset(ctrl->qh_pool, 0, sizeof(ehci_qh_t) * EHCI_QH_POOL_SIZE);

    ctrl->async_head = &ctrl->qh_pool[0];
    ctrl->qh_used[0] = 1;
    ctrl->async_head_phys = ctrl->qh_pool_phys;

    /* Dummy QH aponta para si mesmo — lista circular vazia */
    ctrl->async_head->next   = (u32)ctrl->async_head_phys | QH_TYPE_QH;
    ctrl->async_head->epchar = QH_H | QH_MAXPKT(64) | QH_EPS_HIGH | QH_DTC;
    ctrl->async_head->epcap  = QH_MULT(1);
    ctrl->async_head->qtd_next     = QTD_NEXT_TERMINATE;
    ctrl->async_head->qtd_alt_next = QTD_NEXT_TERMINATE;
    ctrl->async_head->qtd_token    = 0;

    /* ---- Aloca pool de qTDs ---- */
    ctrl->qtd_pool = (ehci_qtd_t *)dma_alloc(
        sizeof(ehci_qtd_t) * EHCI_QTD_POOL_SIZE, 32, &ctrl->qtd_pool_phys);
    if (!ctrl->qtd_pool) return -4;
    ehci_memset(ctrl->qtd_pool, 0, sizeof(ehci_qtd_t) * EHCI_QTD_POOL_SIZE);

    /* ---- Configura registradores do HC ---- */
    ehci_write32(ctrl, EHCI_OP_PERIODICLISTBASE, (u32)ctrl->frame_list_phys);
    ehci_write32(ctrl, EHCI_OP_ASYNCLISTADDR,    (u32)ctrl->async_head_phys);

    /* Limpa status pendentes */
    ehci_write32(ctrl, EHCI_OP_USBSTS, 0x3F);

    /* Habilita interrupções */
    ehci_write32(ctrl, EHCI_OP_USBINTR,
                 EHCI_INTR_INT | EHCI_INTR_ERR |
                 EHCI_INTR_PCD | EHCI_INTR_HSE);

    /* Inicia o HC */
    usbcmd = EHCI_CMD_RUN         |
             EHCI_CMD_FLS_1024    |  /* Frame list de 1024 */
             EHCI_CMD_ITC_8MF     |  /* 1 interrupção por ms */
             EHCI_CMD_ASE;           /* Async schedule enable */
    ehci_write32(ctrl, EHCI_OP_USBCMD, usbcmd);
    ehci_wait_reg(ctrl, EHCI_OP_USBSTS, EHCI_STS_HALTED, 0, 100);

    /* ---- Configure Flag — roteia todas as portas para o EHCI ---- */
    ehci_write32(ctrl, EHCI_OP_CONFIGFLAG, EHCI_CF_FLAG);
    udelay(5);

    /* Liga power em todas as portas */
    for (i = 0; i < ctrl->num_ports; i++) {
        u32 ps = ehci_read32(ctrl, EHCI_OP_PORTSC(i));
        if (!(ps & EHCI_PORT_PP))
            ehci_write32(ctrl, EHCI_OP_PORTSC(i), ps | EHCI_PORT_PP);
    }
    mdelay(20); /* PowerOn-to-PowerGood */

    ctrl->initialized = 1;
    return 0;
}

/* ============================================================
 * Gerenciamento de portas
 * ============================================================ */

int ehci_port_reset(ehci_t *ctrl, u8 port) {
    if (port >= ctrl->num_ports) return -1;

    u32 ps = ehci_read32(ctrl, EHCI_OP_PORTSC(port));

    /* Limpa enable antes de resetar */
    ps &= ~(EHCI_PORT_PE | EHCI_PORT_CHANGE_BITS);
    ps |= EHCI_PORT_RESET;
    ehci_write32(ctrl, EHCI_OP_PORTSC(port), ps);
    mdelay(50); /* USB spec: >= 10ms */

    /* Desativa reset */
    ps = ehci_read32(ctrl, EHCI_OP_PORTSC(port));
    ps &= ~EHCI_PORT_RESET;
    ehci_write32(ctrl, EHCI_OP_PORTSC(port), ps);

    /* Aguarda reset completar */
    if (ehci_wait_reg(ctrl, EHCI_OP_PORTSC(port),
                      EHCI_PORT_RESET, 0, 100) != 0)
        return -1;

    mdelay(10); /* recovery */
    return 0;
}

int ehci_scan_ports(ehci_t *ctrl) {
    int found = 0;
    u8 i;

    for (i = 0; i < ctrl->num_ports; i++) {
        u32 ps = ehci_read32(ctrl, EHCI_OP_PORTSC(i));

        /* Limpa change bits */
        if (ps & EHCI_PORT_CHANGE_BITS)
            ehci_write32(ctrl, EHCI_OP_PORTSC(i),
                         (ps & ~EHCI_PORT_CHANGE_BITS) | EHCI_PORT_CHANGE_BITS);

        if (!(ps & EHCI_PORT_CCS)) {
            ctrl->port_connected[i] = 0;
            continue;
        }

        /* Verifica line status — K = Low Speed device */
        u32 ls = ps & EHCI_PORT_LS_MASK;
        if (ls == EHCI_PORT_LS_K) {
            /* Low Speed: transfere para companion OHCI/UHCI */
            ctrl->port_speed[i] = 1; /* Low */
            ps = ehci_read32(ctrl, EHCI_OP_PORTSC(i));
            ps |= EHCI_PORT_OWNER; /* Entrega ao companion */
            ehci_write32(ctrl, EHCI_OP_PORTSC(i), ps);
            ctrl->port_connected[i] = 0;
            continue;
        }

        /* Reset da porta para detectar velocidade */
        ehci_port_reset(ctrl, i);

        ps = ehci_read32(ctrl, EHCI_OP_PORTSC(i));

        if (ps & EHCI_PORT_PE) {
            /* Port Enable após reset = High Speed */
            ctrl->port_connected[i] = 1;
            ctrl->port_speed[i]     = 2; /* High Speed */
            found++;
        } else {
            /* Full Speed — transfere para companion */
            ctrl->port_speed[i] = 0;
            ps |= EHCI_PORT_OWNER;
            ehci_write32(ctrl, EHCI_OP_PORTSC(i), ps);
            ctrl->port_connected[i] = 0;
        }
    }
    return found;
}

/* ============================================================
 * Montagem de QH + qTDs
 * ============================================================ */

/** Configura os buffer pointers do qTD a partir de um endereço físico */
static void qtd_set_buffer(ehci_qtd_t *qtd, u64 phys, u32 length) {
    qtd->buf[0]    = (u32)(phys & 0xFFFFF000) | (u32)(phys & 0xFFF);
    qtd->buf_hi[0] = (u32)(phys >> 32);
    /* Páginas adicionais (para transferências > 4KB) */
    if (length > 0x1000) {
        qtd->buf[1]    = (u32)((phys + 0x1000) & 0xFFFFF000);
        qtd->buf_hi[1] = (u32)((phys + 0x1000) >> 32);
    }
    if (length > 0x2000) {
        qtd->buf[2]    = (u32)((phys + 0x2000) & 0xFFFFF000);
        qtd->buf_hi[2] = (u32)((phys + 0x2000) >> 32);
    }
    if (length > 0x3000) {
        qtd->buf[3]    = (u32)((phys + 0x3000) & 0xFFFFF000);
        qtd->buf_hi[3] = (u32)((phys + 0x3000) >> 32);
    }
    if (length > 0x4000) {
        qtd->buf[4]    = (u32)((phys + 0x4000) & 0xFFFFF000);
        qtd->buf_hi[4] = (u32)((phys + 0x4000) >> 32);
    }
}

/** Monta um QH com a lista de qTDs encabeçada por first_qtd_phys */
static ehci_qh_t *build_qh(ehci_t *ctrl, const ehci_pipe_t *pipe,
                             u32 first_qtd_phys, u64 *qh_phys_out)
{
    ehci_qh_t *qh = qh_alloc(ctrl, qh_phys_out);
    if (!qh) return (void *)0;

    /* epchar */
    u32 eps;
    switch (pipe->speed) {
        case 1:  eps = QH_EPS_LOW;  break;
        case 2:  eps = QH_EPS_HIGH; break;
        default: eps = QH_EPS_FULL; break;
    }

    qh->epchar = QH_DEVADDR(pipe->dev_addr) |
                 QH_ENDPT(pipe->ep_num)      |
                 eps                          |
                 QH_DTC                       |  /* toggle controlado pelo qTD */
                 QH_MAXPKT(pipe->max_packet)  |
                 QH_RL(3);

    /* Para Full/Low Speed em Control: seta C bit e info do TT */
    if (pipe->speed < 2 && pipe->type == EHCI_PIPE_CONTROL)
        qh->epchar |= QH_C;

    /* epcap */
    qh->epcap = QH_MULT(1);
    if (pipe->speed < 2) {
        qh->epcap |= QH_HUBADDR(pipe->hub_addr) |
                     QH_PORTNUM(pipe->hub_port);
    }

    /* Aponta para o primeiro qTD */
    qh->qtd_next     = first_qtd_phys;
    qh->qtd_alt_next = QTD_NEXT_TERMINATE;
    qh->qtd_token    = 0;
    qh->current_qtd  = 0;

    return qh;
}

/** Insere QH na async list e aguarda conclusão */
static i32 ehci_submit_async(ehci_t *ctrl, ehci_qh_t *qh, u64 qh_phys,
                              ehci_qtd_t *last_qtd, u32 timeout_ms)
{
    /* Insere depois do head: head → qh → (antigo next do head) */
    qh->next = ctrl->async_head->next;
    ctrl->async_head->next = (u32)qh_phys | QH_TYPE_QH;

    /* Garante que async schedule está rodando */
    u32 usbcmd = ehci_read32(ctrl, EHCI_OP_USBCMD);
    if (!(usbcmd & EHCI_CMD_ASE)) {
        usbcmd |= EHCI_CMD_ASE;
        ehci_write32(ctrl, EHCI_OP_USBCMD, usbcmd);
        ehci_wait_reg(ctrl, EHCI_OP_USBSTS, EHCI_STS_ASS, EHCI_STS_ASS, 50);
    }

    /* Aguarda último qTD ficar inativo (ACTIVE=0) */
    u32 elapsed = 0;
    while (elapsed < timeout_ms) {
        if (!(last_qtd->token & QTD_STATUS_ACTIVE)) break;
        mdelay(1);
        elapsed++;
    }

    /* Remove QH da async list */
    ctrl->async_head->next = qh->next;

    /* Async Advance Doorbell — garante que o HC não está mais usando o QH */
    usbcmd = ehci_read32(ctrl, EHCI_OP_USBCMD);
    usbcmd |= EHCI_CMD_IAAD;
    ehci_write32(ctrl, EHCI_OP_USBCMD, usbcmd);
    ehci_wait_reg(ctrl, EHCI_OP_USBSTS, EHCI_STS_IAA, EHCI_STS_IAA, 50);
    ehci_write32(ctrl, EHCI_OP_USBSTS, EHCI_STS_IAA);

    if (elapsed >= timeout_ms) return -2; /* timeout */

    /* Verifica erro no token do último qTD */
    u32 tok = last_qtd->token;
    if (tok & (QTD_STATUS_HALTED | QTD_STATUS_BUFERR |
               QTD_STATUS_BABBLE | QTD_STATUS_XACTERR))
        return -3;

    return 0;
}

/* ============================================================
 * Transferência Control
 * ============================================================ */

i32 ehci_control_transfer(ehci_t *ctrl, ehci_pipe_t *pipe,
                           const u8 setup[8], void *data, u32 length)
{
    if (!ctrl->initialized) return -1;

    u64 setup_qtd_phys, data_qtd_phys, status_qtd_phys;

    /* ---- qTD SETUP ---- */
    ehci_qtd_t *setup_qtd = qtd_alloc(ctrl, &setup_qtd_phys);
    if (!setup_qtd) return -1;

    u64 setup_phys = virt_to_phys(setup);
    qtd_set_buffer(setup_qtd, setup_phys, 8);
    setup_qtd->token = QTD_STATUS_ACTIVE |
                       QTD_PID_SETUP     |
                       QTD_CERR_3        |
                       QTD_BYTES(8)      |
                       QTD_TOGGLE_0;

    /* ---- qTD DATA (opcional) ---- */
    ehci_qtd_t *data_qtd = (void *)0;
    if (data && length > 0) {
        data_qtd = qtd_alloc(ctrl, &data_qtd_phys);
        if (!data_qtd) { qtd_free(ctrl, setup_qtd); return -1; }

        u64 data_phys = virt_to_phys(data);
        qtd_set_buffer(data_qtd, data_phys, length);
        u32 pid = (setup[0] & 0x80) ? QTD_PID_IN : QTD_PID_OUT;
        data_qtd->token = QTD_STATUS_ACTIVE |
                          pid               |
                          QTD_CERR_3        |
                          QTD_BYTES(length) |
                          QTD_TOGGLE_1;     /* Data sempre começa em DATA1 */
    }

    /* ---- qTD STATUS ---- */
    ehci_qtd_t *status_qtd = qtd_alloc(ctrl, &status_qtd_phys);
    if (!status_qtd) {
        if (data_qtd) qtd_free(ctrl, data_qtd);
        qtd_free(ctrl, setup_qtd);
        return -1;
    }
    u32 status_pid = (setup[0] & 0x80) ? QTD_PID_OUT : QTD_PID_IN;
    status_qtd->token = QTD_STATUS_ACTIVE |
                        status_pid         |
                        QTD_CERR_3         |
                        QTD_BYTES(0)       |
                        QTD_TOGGLE_1       |
                        QTD_IOC;           /* Interrupção ao completar */

    /* ---- Encadeia qTDs ---- */
    if (data_qtd) {
        setup_qtd->next  = (u32)data_qtd_phys;
        data_qtd->next   = (u32)status_qtd_phys;
    } else {
        setup_qtd->next  = (u32)status_qtd_phys;
    }
    status_qtd->next = QTD_NEXT_TERMINATE;

    /* ---- Monta QH ---- */
    u64 qh_phys;
    ehci_qh_t *qh = build_qh(ctrl, pipe, (u32)setup_qtd_phys, &qh_phys);
    if (!qh) {
        qtd_free(ctrl, status_qtd);
        if (data_qtd) qtd_free(ctrl, data_qtd);
        qtd_free(ctrl, setup_qtd);
        return -1;
    }

    /* ---- Submete e aguarda ---- */
    i32 ret = ehci_submit_async(ctrl, qh, qh_phys, status_qtd, 500);

    /* ---- Libera ---- */
    qh_free(ctrl, qh);
    qtd_free(ctrl, status_qtd);
    if (data_qtd) qtd_free(ctrl, data_qtd);
    qtd_free(ctrl, setup_qtd);

    if (ret < 0) return ret;
    return (i32)length;
}

/* ============================================================
 * Transferência Bulk
 * ============================================================ */

i32 ehci_bulk_transfer(ehci_t *ctrl, ehci_pipe_t *pipe,
                        void *data, u32 length, u8 in)
{
    if (!ctrl->initialized || !data || length == 0) return -1;

    u64 qtd_phys;
    ehci_qtd_t *qtd = qtd_alloc(ctrl, &qtd_phys);
    if (!qtd) return -1;

    u64 data_phys = virt_to_phys(data);
    qtd_set_buffer(qtd, data_phys, length);

    u32 pid    = in ? QTD_PID_IN : QTD_PID_OUT;
    u32 toggle = pipe->toggle ? QTD_TOGGLE_1 : QTD_TOGGLE_0;

    qtd->token = QTD_STATUS_ACTIVE |
                 pid               |
                 QTD_CERR_3        |
                 QTD_BYTES(length) |
                 toggle            |
                 QTD_IOC;
    qtd->next  = QTD_NEXT_TERMINATE;

    u64 qh_phys;
    ehci_qh_t *qh = build_qh(ctrl, pipe, (u32)qtd_phys, &qh_phys);
    if (!qh) { qtd_free(ctrl, qtd); return -1; }

    i32 ret = ehci_submit_async(ctrl, qh, qh_phys, qtd, 500);

    /* Atualiza toggle */
    pipe->toggle ^= 1;

    qh_free(ctrl, qh);
    qtd_free(ctrl, qtd);

    if (ret < 0) return ret;
    return (i32)length;
}

/* ============================================================
 * GET_DESCRIPTOR
 * ============================================================ */

i32 ehci_get_descriptor(ehci_t *ctrl, u8 dev_addr, u8 speed,
                         void *buf, u16 len)
{
    ehci_pipe_t pipe;
    ehci_memset(&pipe, 0, sizeof(pipe));
    pipe.type       = EHCI_PIPE_CONTROL;
    pipe.dev_addr   = dev_addr;
    pipe.ep_num     = 0;
    pipe.speed      = speed;
    pipe.max_packet = (speed == 2) ? 64 : 8;

    u8 setup[8];
    usb_setup_get_descriptor(setup, USB_DESC_DEVICE, 0, len);
    return ehci_control_transfer(ctrl, &pipe, setup, buf, len);
}

/* ============================================================
 * IRQ Handler
 * ============================================================ */

void ehci_irq_handler(ehci_t *ctrl) {
    if (!ctrl->initialized) return;

    u32 sts = ehci_read32(ctrl, EHCI_OP_USBSTS);
    u32 en  = ehci_read32(ctrl, EHCI_OP_USBINTR);
    sts &= en;
    if (!sts) return;

    /* Limpa interrupções */
    ehci_write32(ctrl, EHCI_OP_USBSTS, sts);

    if (sts & EHCI_STS_INT) {
        /* Transferência completada — wakeup de threads esperando */
    }

    if (sts & EHCI_STS_ERR) {
        /* Erro de transferência */
    }

    if (sts & EHCI_STS_PCD) {
        /* Port Change Detect — alguma porta mudou */
        u8 i;
        for (i = 0; i < ctrl->num_ports; i++) {
            u32 ps = ehci_read32(ctrl, EHCI_OP_PORTSC(i));
            if (ps & EHCI_PORT_CSC) {
                ctrl->port_connected[i] = (ps & EHCI_PORT_CCS) ? 1 : 0;
                /* Limpa CSC */
                ehci_write32(ctrl, EHCI_OP_PORTSC(i),
                             (ps & ~EHCI_PORT_CHANGE_BITS) | EHCI_PORT_CSC);
            }
        }
    }

    if (sts & EHCI_STS_HSE) {
        /* Host System Error — reseta o HC */
        ehci_write32(ctrl, EHCI_OP_USBCMD, EHCI_CMD_HCRESET);
    }

    if (sts & EHCI_STS_IAA) {
        /* Async Advance — QH foi removido com segurança */
    }
}

/* ============================================================
 * Shutdown
 * ============================================================ */

void ehci_shutdown(ehci_t *ctrl) {
    if (!ctrl->initialized) return;

    /* Desabilita interrupções */
    ehci_write32(ctrl, EHCI_OP_USBINTR, 0);

    /* Para o HC */
    u32 usbcmd = ehci_read32(ctrl, EHCI_OP_USBCMD);
    usbcmd &= ~EHCI_CMD_RUN;
    ehci_write32(ctrl, EHCI_OP_USBCMD, usbcmd);
    ehci_wait_reg(ctrl, EHCI_OP_USBSTS, EHCI_STS_HALTED, EHCI_STS_HALTED, 100);

    /* Devolve portas para companions */
    u8 i;
    for (i = 0; i < ctrl->num_ports; i++) {
        u32 ps = ehci_read32(ctrl, EHCI_OP_PORTSC(i));
        ehci_write32(ctrl, EHCI_OP_PORTSC(i), ps | EHCI_PORT_OWNER);
    }

    /* Limpa configure flag */
    ehci_write32(ctrl, EHCI_OP_CONFIGFLAG, 0);

    ctrl->initialized = 0;
}
