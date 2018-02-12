/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "debug.h"

#include <cstdio>
#include <cstdarg>

static unsigned _debug_level = 1;
static unsigned _trace_mask;

void set_debug_level(unsigned level)
{
  _debug_level = level;
}

bool dlevel(unsigned level)
{
  return _debug_level >= level;
}

void d_printf(unsigned level, char const *fmt, ...)
{
  if (_debug_level < level)
    return;

  va_list a;
  va_start(a, fmt);
  vprintf(fmt, a);
  va_end(a);
}

/**
 * \brief Set the mask for event tracing (See debug.h for possible trace
 * events).
 */
void set_trace_mask(unsigned mask)
{
  _trace_mask = mask;
}

/**
 * \brief Trace an event.
 *
 * \param event Event to be traced
 */
void trace_event(unsigned event, char const *fmt, ...)
{
  if ((_trace_mask & event) == 0)
    return;

  va_list a;
  va_start(a, fmt);
  vprintf(fmt, a);
  va_end(a);
}

/**
 * \brief Determine if an event was selected for tracing.
 *
 * \param event Event to be queried.
 *
 * \returns true if event was selected for tracing, false otherwise
 */
bool trace_event_enabled(unsigned event)
{
  return (_trace_mask & event);
}

// Set the ACPICA debug flags
// This function is overridden in acpi/acpi.cc.
void __attribute__((weak)) acpi_set_debug_level(unsigned) {}

