; =============================================================
; boot.asm - Bootloader x86 32-bit do KryFX OS
; Pasta: bootloader/boot.asm
;
; Fluxo:
;   BIOS carrega este setor em 0x7C00 (modo real 16-bit)
;   → Habilita A20
;   → Carrega o kernel do disco (LBA)
;   → Entra em modo protegido 32-bit (Protected Mode)
;   → Salta para kernel_main()
; =============================================================

[BITS 16]
[ORG 0x7C00]

; =============================================================
; Ponto de entrada — BIOS salta para cá
; =============================================================
start:
    cli                     ; Desabilita interrupções
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack cresce para baixo a partir de 0x7C00

    ; Salva drive number passado pela BIOS (DL)
    mov [boot_drive], dl

    ; Limpa a tela
    mov ah, 0x00
    mov al, 0x03            ; Modo texto 80x25
    int 0x10

    ; Mensagem de boot
    mov si, msg_boot
    call print_str_16

    ; ── Habilita A20 ─────────────────────────────────────────
    call enable_a20

    ; ── Carrega kernel do disco ───────────────────────────────
    ; Kernel está nos setores 2..N do mesmo disco
    ; Carrega KERNEL_SECTORS setores a partir do LBA 1
    ; para o endereço físico KERNEL_LOAD_ADDR (0x10000 = 64KB)
    call load_kernel

    ; ── Carrega GDT temporária e entra em modo protegido ──────
    lgdt [gdt_descriptor]

    mov eax, cr0
    or  eax, 1              ; PE bit
    mov cr0, eax

    ; Far jump para limpar pipeline e entrar em 32-bit
    jmp 0x08:protected_mode_entry

; =============================================================
; Dados e constantes (16-bit)
; =============================================================
boot_drive      db 0
msg_boot        db "KryFX Bootloader v1.0", 0x0D, 0x0A, 0
msg_a20_ok      db "A20 OK", 0x0D, 0x0A, 0
msg_load_ok     db "Kernel carregado", 0x0D, 0x0A, 0
msg_load_err    db "ERRO: leitura disco", 0x0D, 0x0A, 0

KERNEL_LOAD_ADDR equ 0x10000   ; Endereço físico onde o kernel é carregado
KERNEL_SECTORS   equ 64        ; Número de setores a carregar (64 × 512 = 32 KB)
                                ; Aumente se o kernel crescer

; =============================================================
; print_str_16 — imprime string null-terminated (modo real)
; SI = ponteiro para a string
; =============================================================
print_str_16:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    popa
    ret

; =============================================================
; enable_a20 — habilita linha A20
; Tenta: BIOS int 15h → porta 0x92 (fast A20)
; =============================================================
enable_a20:
    ; Tenta via BIOS primeiro
    mov ax, 0x2401
    int 0x15
    jnc .a20_done

    ; Fallback: Fast A20 via porta 0x92
    in  al, 0x92
    or  al, 0x02
    and al, 0xFE            ; Não aciona reset (bit 0)
    out 0x92, al

.a20_done:
    mov si, msg_a20_ok
    call print_str_16
    ret

; =============================================================
; load_kernel — carrega setores do disco usando BIOS int 13h
; Usa LBA via DAP (Disk Address Packet)
; =============================================================
load_kernel:
    ; Configura DAP
    mov byte  [dap_size],    0x10   ; Tamanho do DAP
    mov byte  [dap_zero],    0x00
    mov word  [dap_sectors], KERNEL_SECTORS
    mov word  [dap_offset],  (KERNEL_LOAD_ADDR & 0xFFFF)
    mov word  [dap_segment], (KERNEL_LOAD_ADDR >> 4) & 0xFFFF
    mov dword [dap_lba_lo],  1      ; Começa no setor LBA 1 (setor 2)
    mov dword [dap_lba_hi],  0

    mov ah, 0x42            ; Extended Read
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc  .load_error

    mov si, msg_load_ok
    call print_str_16
    ret

.load_error:
    mov si, msg_load_err
    call print_str_16
    ; Trava — erro fatal
    cli
    hlt

; Disk Address Packet (DAP)
dap:
dap_size    db 0x10
dap_zero    db 0x00
dap_sectors dw 0
dap_offset  dw 0
dap_segment dw 0
dap_lba_lo  dd 0
dap_lba_hi  dd 0

; =============================================================
; GDT temporária (usada só para entrar em PM)
; A GDT real é instalada pelo kernel_main via gdt_install()
; =============================================================
gdt_temp:
    ; Null descriptor
    dq 0x0000000000000000
    ; Code segment: base=0, limit=4GB, 32-bit, ring 0
    dq 0x00CF9A000000FFFF
    ; Data segment: base=0, limit=4GB, 32-bit, ring 0
    dq 0x00CF92000000FFFF

gdt_descriptor:
    dw (gdt_descriptor - gdt_temp - 1)  ; Limite
    dd gdt_temp                          ; Base

; =============================================================
; Modo Protegido 32-bit
; =============================================================
[BITS 32]
protected_mode_entry:
    ; Configura segmentos de dados
    mov ax, 0x10            ; Data selector (entrada 2 da GDT temp)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Stack definitiva em 0x90000 (abaixo do kernel)
    mov esp, 0x90000

    ; Salta para o kernel carregado em KERNEL_LOAD_ADDR
    ; O kernel_main está no início do binário gerado pelo linker
    jmp KERNEL_LOAD_ADDR

    ; Segurança — nunca deve chegar aqui
    cli
    hlt

; =============================================================
; Padding + assinatura de boot (0x55AA no byte 510-511)
; =============================================================
times 510 - ($ - $$) db 0
dw 0xAA55
