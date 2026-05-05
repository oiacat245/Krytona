/*
 * vixfs.c — VixFS read-only in-memory filesystem (Viuxne OS) used in (YtarOS)
 */
#include "kryfx.h"
#include <stdint.h>

static const vixfs_header_t *g_hdr   = 0;
static const vixfs_entry_t  *g_table = 0;
static const uint8_t        *g_data  = 0;
static uint32_t              g_image_size = 0;

static int vix_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int vixfs_init(const void *image, uint32_t image_size) {
    const vixfs_header_t *h = (const vixfs_header_t *)image;
    if (!image || image_size < sizeof(vixfs_header_t)) return 0;
    if (h->magic   != VIXFS_MAGIC)   return 0;
    if (h->version != VIXFS_VERSION) return 0;
    /* sanity: entry table must fit */
    uint32_t table_end = sizeof(vixfs_header_t) +
                         h->file_count * sizeof(vixfs_entry_t);
    if (table_end > image_size)      return 0;
    if (h->data_offset > image_size) return 0;

    g_hdr        = h;
    g_table      = (const vixfs_entry_t *)((const uint8_t *)image +
                       sizeof(vixfs_header_t));
    g_data       = (const uint8_t *)image + h->data_offset;
    g_image_size = image_size;
    return 1;
}

const void *vixfs_open(const char *path, uint32_t *size_out) {
    uint32_t i;
    if (!g_hdr || !path) return 0;
    for (i = 0; i < g_hdr->file_count; i++) {
        if (vix_strcmp(g_table[i].path, path) == 0) {
            uint32_t off  = g_table[i].offset;
            uint32_t size = g_table[i].size;
            /* bounds check */
            if (off + size > g_image_size - g_hdr->data_offset) return 0;
            if (size_out) *size_out = size;
            return g_data + off;
        }
    }
    return 0;
}

uint32_t vixfs_file_count(void) {
    return g_hdr ? g_hdr->file_count : 0;
}

const vixfs_entry_t *vixfs_entry_by_index(uint32_t index) {
    if (!g_hdr || index >= g_hdr->file_count) return 0;
    return &g_table[index];
}
