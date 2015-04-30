#ifndef _HAVE_FPGA_H
#define _HAVE_FPGA_H

#include <cfgfile.h>

void FPGA_init();
void FPGA_program(const struct CFG_FPGA * cfg);

#endif
