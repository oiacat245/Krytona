/* =============================================================
 * linker.ld — Script de linker para o KryFX OS
 * Kernel carregado em 0x10000 (64KB) pelo bootloader
 * ============================================================= */

ENTRY(kernel_main)

SECTIONS
{
    /* Endereço de carga: bootloader salta para cá */
    . = 0x10000;

    /* Código executável */
    .text :
    {
        *(.text)
        *(.text.*)
    }

    /* Dados inicializados */
    .data :
    {
        *(.data)
        *(.data.*)
        *(.rodata)
        *(.rodata.*)
    }

    /* Dados não inicializados (zerado pelo startup) */
    .bss :
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Descarta seções desnecessárias */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.debug*)
    }
}
