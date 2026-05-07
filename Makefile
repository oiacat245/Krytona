# =============================================================
# Makefile — KryFX OS
# Estrutura:
#   bootloader/boot.asm   → boot.bin  (setor de boot)
#   kernel.c              → kernel.bin
#   gpu/                  → intelgpu, nvidiagpu
#   ohci/                 → ohci
#   ehci/                 → ehci
#   xhci/                 → xhci (compilado pelo seu amigo)
#   PS2/                  → ps2
#   kryfx/                → kryfx
#
# Saída final: kryfx.img  (imagem de disco raw)
# =============================================================

# ── Toolchain ────────────────────────────────────────────────
CC      := i686-elf-gcc
AS      := nasm
LD      := i686-elf-ld
OBJCOPY := i686-elf-objcopy

# ── Flags ────────────────────────────────────────────────────
CFLAGS  := -m32 \
            -std=c99 \
            -O2 \
            -Wall \
            -Wextra \
            -ffreestanding \
            -fno-stack-protector \
            -fno-builtin \
            -fno-pie \
            -nostdinc \
            -I. \
            -Igpu \
            -Iohci \
            -Iehci \
            -Ixhci \
            -IPS2 \
            -Ikryfx

LDFLAGS := -m elf_i386 \
            -T linker.ld \
            --oformat binary

ASFLAGS_BOOT := -f bin
ASFLAGS_OBJ  := -f elf32

# ── Diretórios ────────────────────────────────────────────────
BOOTLOADER_DIR := bootloader
BUILD_DIR      := build

# ── Fontes ───────────────────────────────────────────────────

# Kernel principal
KERNEL_SRCS := kernel.c

# GPU
GPU_SRCS    := gpu/intelgpu.c \
               gpu/nvidiagpu.c

# USB
OHCI_SRCS   := ohci/ohci.c
EHCI_SRCS   := ehci/ehci.c
XHCI_SRCS   := $(wildcard xhci/*.c)

# PS/2
PS2_SRCS    := $(wildcard PS2/*.c)

# KryFX
KRYFX_SRCS  := $(wildcard kryfx/*.c)

# Todos os .c juntos
ALL_SRCS    := $(KERNEL_SRCS) \
               $(GPU_SRCS)    \
               $(OHCI_SRCS)   \
               $(EHCI_SRCS)   \
               $(XHCI_SRCS)   \
               $(PS2_SRCS)    \
               $(KRYFX_SRCS)

# Gera lista de .o em build/
OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(ALL_SRCS))

# ── Alvos principais ─────────────────────────────────────────
.PHONY: all clean run debug iso

all: $(BUILD_DIR)/kryfx.img

# ── Imagem final: boot.bin + kernel.bin ──────────────────────
$(BUILD_DIR)/kryfx.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin
	@echo "[IMG] Gerando kryfx.img..."
	@# Cria imagem de 1.44MB (2880 setores de 512 bytes)
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	@# Escreve bootloader no setor 0
	dd if=$(BUILD_DIR)/boot.bin of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	@# Escreve kernel a partir do setor 1
	dd if=$(BUILD_DIR)/kernel.bin of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "[IMG] kryfx.img pronto! ($$(du -h $@ | cut -f1))"

# ── Bootloader ───────────────────────────────────────────────
$(BUILD_DIR)/boot.bin: $(BOOTLOADER_DIR)/boot.asm | $(BUILD_DIR)
	@echo "[ASM] $<"
	$(AS) $(ASFLAGS_BOOT) $< -o $@
	@# Verifica que ficou exatamente 512 bytes
	@size=$$(wc -c < $@); \
	if [ "$$size" -ne 512 ]; then \
		echo "ERRO: boot.bin tem $$size bytes (esperado 512)"; \
		exit 1; \
	fi
	@echo "[ASM] boot.bin OK (512 bytes)"

# ── Kernel (link de todos os .o) ─────────────────────────────
$(BUILD_DIR)/kernel.bin: $(OBJS) linker.ld | $(BUILD_DIR)
	@echo "[LD] Linkando kernel..."
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[LD] kernel.bin: $$(du -h $@ | cut -f1)"

# ── Compilação de .c → .o ────────────────────────────────────
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

# ── Cria diretório build ──────────────────────────────────────
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/gpu
	@mkdir -p $(BUILD_DIR)/ohci
	@mkdir -p $(BUILD_DIR)/ehci
	@mkdir -p $(BUILD_DIR)/xhci
	@mkdir -p $(BUILD_DIR)/PS2
	@mkdir -p $(BUILD_DIR)/kryfx

# ── Executa no QEMU ──────────────────────────────────────────
run: all
	@echo "[QEMU] Iniciando KryFX OS..."
	qemu-system-i386 \
		-drive format=raw,file=$(BUILD_DIR)/kryfx.img \
		-serial stdio \
		-m 128M \
		-no-reboot \
		-no-shutdown

# ── Debug com GDB ────────────────────────────────────────────
debug: all
	@echo "[QEMU] Modo debug — aguardando GDB na porta 1234..."
	qemu-system-i386 \
		-drive format=raw,file=$(BUILD_DIR)/kryfx.img \
		-serial stdio \
		-m 128M \
		-no-reboot \
		-no-shutdown \
		-s -S &
	i686-elf-gdb \
		-ex "target remote localhost:1234" \
		-ex "symbol-file $(BUILD_DIR)/kernel.elf" \
		-ex "break kernel_main" \
		-ex "continue"

# ── Gera ELF com símbolos (para GDB) ─────────────────────────
$(BUILD_DIR)/kernel.elf: $(OBJS) linker.ld | $(BUILD_DIR)
	@echo "[LD] Gerando kernel.elf com símbolos..."
	$(LD) -m elf_i386 -T linker.ld -o $@ $(OBJS)

symbols: $(BUILD_DIR)/kernel.elf

# ── ISO bootável (requer xorriso + grub-mkrescue) ────────────
iso: all
	@echo "[ISO] Gerando kryfx.iso..."
	@mkdir -p $(BUILD_DIR)/iso/boot
	@cp $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/iso/boot/
	@echo 'set timeout=0'                        > $(BUILD_DIR)/iso/boot/grub.cfg
	@echo 'set default=0'                       >> $(BUILD_DIR)/iso/boot/grub.cfg
	@echo 'menuentry "KryFX OS" {'              >> $(BUILD_DIR)/iso/boot/grub.cfg
	@echo '    multiboot /boot/kernel.bin'      >> $(BUILD_DIR)/iso/boot/grub.cfg
	@echo '}'                                   >> $(BUILD_DIR)/iso/boot/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/kryfx.iso $(BUILD_DIR)/iso
	@echo "[ISO] kryfx.iso pronto!"

# ── Limpeza ──────────────────────────────────────────────────
clean:
	@echo "[CLEAN] Removendo build/..."
	@rm -rf $(BUILD_DIR)
	@echo "[CLEAN] Pronto"

# ── Info do ambiente ─────────────────────────────────────────
info:
	@echo "CC      = $$($(CC) --version | head -1)"
	@echo "AS      = $$($(AS) --version | head -1)"
	@echo "LD      = $$($(LD) --version | head -1)"
	@echo "Fontes  = $$(echo $(ALL_SRCS) | wc -w) arquivos"
	@echo "Objetos = $$(echo $(OBJS) | wc -w) arquivos"
