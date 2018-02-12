#pragma once

#include <l4/sys/l4int.h>
#include <pthread.h>

#define ACPI_MACHINE_WIDTH L4_MWORD_BITS
#define COMPILER_DEPENDENT_INT64    l4_int64_t
#define COMPILER_DEPENDENT_UINT64   l4_uint64_t
#define ACPI_FLUSH_CPU_CACHE()

#define ACPI_USE_SYSTEM_CLIBRARY 1
#define ACPI_USE_STANDARD_HEADERS 1

#ifndef ACPI_USE_NATIVE_DIVIDE
#define ACPI_USE_NATIVE_DIVIDE
#endif
#define ACPI_DEBUG_OUTPUT 1
#define ACPI_USE_LOCAL_CACHE
//#define ACPI_DEBUGGER 1
//#define ACPI_DISASSEMBLER 1

#include "platform/acgcc.h"

