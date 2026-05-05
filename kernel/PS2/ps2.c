
#include "ps2.h"
#include <stdint.h>

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1" :: "a"(val), "Nd"(port));
}
static uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(port));
    return v;
}
static void wait_ibf(void) {
    uint32_t t = 100000u;
    while (t-- && (inb(PS2_STS) & PS2_STS_IBF));
}
static void wait_obf(void) {
    uint32_t t = 100000u;
    while (t-- && !(inb(PS2_STS) & PS2_STS_OBF));
}

void ps2_cmd(uint8_t cmd) {
    wait_ibf();
    outb(PS2_CMD, cmd);
}
void ps2_write(uint8_t val) {
    wait_ibf();
    outb(PS2_DATA, val);
}
void ps2_write_mouse(uint8_t val) {
    ps2_cmd(0xD4);
    ps2_write(val);
}
uint8_t ps2_read(void) {
    wait_obf();
    return inb(PS2_DATA);
}
int ps2_can_read(void) {
    uint8_t s = inb(PS2_STS);
    return (s & PS2_STS_OBF) && !(s & PS2_STS_AUX);
}
int ps2_can_read_mouse(void) {
    uint8_t s = inb(PS2_STS);
    return (s & PS2_STS_OBF) && (s & PS2_STS_AUX);
}

void ps2_init(void) {
    /* Disable both ports */
    ps2_cmd(0xAD);
    ps2_cmd(0xA7);
    /* Flush output buffer */
    while (inb(PS2_STS) & PS2_STS_OBF) inb(PS2_DATA);
    /* Read config */
    ps2_cmd(0x20);
    uint8_t cfg = ps2_read();
    /* Enable IRQs and translation, enable aux port */
    cfg |=  0x03u;
    cfg &= ~0x30u;
    ps2_cmd(0x60);
    ps2_write(cfg);
    /* Enable both ports */
    ps2_cmd(0xAE);
    ps2_cmd(0xA8);
}
