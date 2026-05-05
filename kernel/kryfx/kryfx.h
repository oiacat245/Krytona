#ifndef KRYFX_H
#define KRYFX_H
#include <stdint.h>

/*
 * VixFS — read-only in-memory filesystem (Viuxne OS)
 *
 * Layout (packed, little-endian):
 *
 *   [vixfs_header]
 *   [vixfs_entry x header.file_count]
 *   [raw file data, concatenated]
 *
 * The FS image is linked as a binary blob via objcopy (like cur.bmp).
 * Paths use '/' separator, max 64 chars including null terminator.
 *
 * Build the image with tools/mkfs.py:
 *   python3 tools/mkfs.py \
 *       fonts/default.ttf \
 *       > kernel/ramfs/ramfs.img
 *
 * Then link:
 *   objcopy -I binary -O elf32-i386 -B i386 \
 *     --rename-section .data=.rodata \
 *     --redefine-sym _binary____kernel_ramfs_ramfs_img_start=vixfs_start \
 *     --redefine-sym _binary____kernel_ramfs_ramfs_img_end=vixfs_end \
 *     --redefine-sym _binary____kernel_ramfs_ramfs_img_size=vixfs_size \
 *     kernel/ramfs/ramfs.img kernel/arch/x86_64/ramfs_data.o
 */

#define VIXFS_MAGIC      0x56495846u  /* "VIXF" */
#define VIXFS_VERSION    1u
#define VIXFS_PATH_MAX   64u

typedef struct __attribute__((packed)) {
    uint32_t magic;        /* VIXFS_MAGIC                        */
    uint32_t version;      /* VIXFS_VERSION                      */
    uint32_t file_count;   /* number of vixfs_entry structs       */
    uint32_t data_offset;  /* byte offset from image start to data */
    uint32_t reserved[4];  /* future use, must be 0               */
} vixfs_header_t;          /* 32 bytes                            */

typedef struct __attribute__((packed)) {
    char     path[VIXFS_PATH_MAX]; /* null-terminated, e.g. "fonts/default.ttf" */
    uint32_t offset;               /* byte offset within data region             */
    uint32_t size;                 /* file size in bytes                         */
    uint32_t reserved[2];          /* future use, must be 0                      */
} vixfs_entry_t;                   /* 80 bytes                                   */

/* ── Runtime API ─────────────────────────────────────────────────────── */

/*
 * vixfs_init() — call once with the FS image blob.
 * Returns 1 on success, 0 if the magic/version doesn't match.
 */
int vixfs_init(const void *image, uint32_t image_size);

/*
 * vixfs_open() — look up a file by path.
 * Returns a pointer to the file data and sets *size_out.
 * Returns NULL if not found.
 */
const void *vixfs_open(const char *path, uint32_t *size_out);

/*
 * vixfs_file_count() — number of files in the image.
 */
uint32_t vixfs_file_count(void);

/*
 * vixfs_entry_by_index() — iterate files (for ls-style listing).
 * Returns NULL if index >= file_count.
 */
const vixfs_entry_t *vixfs_entry_by_index(uint32_t index);

#endif /* VIXFS_H */
