#pragma once
#include <l4/sys/cxx/ipc_types>

extern "C" char *write_long_float_bool();
extern "C" char *write_bool_long_float();
extern "C" void write_cxx_snd_fpage(char *c, l4_fpage_t fp);
