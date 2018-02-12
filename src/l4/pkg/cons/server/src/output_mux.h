/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once
#include <cstdarg>

class Client;

class Output_mux
{
public:
  virtual void write(Client *tag, char const *buffer, unsigned size) = 0;
  virtual void cat(Client *tag, bool add_nl) = 0;
  virtual void tail(Client *tag, int numlines, bool add_nl) = 0;
  virtual void flush(Client *tag) = 0;
  virtual int vsys_msg(const char *fmt, va_list args) = 0;
  int sys_msg(const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  virtual int vprintf(const char *fmt, va_list args) = 0;
  int printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
  virtual char const *name() const = 0;

  virtual void show(Client *c) = 0;
  virtual void hide(Client *c) = 0;
  virtual void connect(Client *c) = 0;
  virtual void disconnect(Client *c, bool show_prompt = true) = 0;
  virtual ~Output_mux() = 0;
};

inline Output_mux::~Output_mux() {}

inline int
Output_mux::sys_msg(const char *fmt, ...)
{
  int l;
  va_list a;
  va_start(a, fmt);
  l = vsys_msg(fmt, a);
  va_end(a);
  return l;
}

inline int
Output_mux::printf(const char *fmt, ...)
{
  int l;
  va_list a;
  va_start(a, fmt);
  l = vprintf(fmt, a);
  va_end(a);
  return l;
}
