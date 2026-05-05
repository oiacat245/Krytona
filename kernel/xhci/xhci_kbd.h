#ifndef XHCI_KBD_H
#define XHCI_KBD_H
#include "xhci.h"

extern xhci_t g_xhci_kbd;

int xhci_kbd_init(void);
int xhci_kbuff(uint8_t *report);

#endif
