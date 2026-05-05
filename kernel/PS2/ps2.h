#pragma once
#include <stdint.h>

#define PS2_DATA  0x60
#define PS2_CMD   0x64
#define PS2_STS   0x64

#define PS2_STS_OBF  (1u<<0)
#define PS2_STS_IBF  (1u<<1)
#define PS2_STS_AUX  (1u<<5)

void     ps2_init(void);
void     ps2_cmd(uint8_t cmd);
void     ps2_write(uint8_t val);
void     ps2_write_mouse(uint8_t val);
uint8_t  ps2_read(void);
int      ps2_can_read(void);
int      ps2_can_read_mouse(void);
