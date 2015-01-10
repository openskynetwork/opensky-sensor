#ifndef _HAVE_FPGA_H
#define _HAVE_FPGA_H

#include <stdint.h>

void FPGA_init();
void FPGA_reset(uint32_t timeout);
void FPGA_program(const char * file, uint32_t timeout, uint32_t retries);

#endif
