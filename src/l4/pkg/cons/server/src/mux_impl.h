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

#include "mux.h"
#include "controller.h"

#include <l4/cxx/string>

#include <cstdarg>
#include <cstring>
#include <cstdio>

class Pbuf
{
public:
  class Sink
  {
  public:
    virtual void write(char const *buf, unsigned size) const = 0;
  };

  explicit Pbuf(Sink *sink) : _p(0), _sink(sink) {}
  unsigned long size() const { return sizeof(_b); }
  void flush();
  void printf(char const *fmt, ...) __attribute__((format(printf, 2, 3)));
  void outnstring(char const *str, unsigned long len);

private:
  bool fits(unsigned l) const { return (_p + l) < sizeof(_b); }
  void checknflush(int n);

  char _b[1024];
  unsigned long _p;
  Sink *_sink;
};

class Mux_i : public Mux, private Pbuf::Sink
{
public:
  explicit Mux_i(Controller *ctl, char const *name);

  void write_tag(Client *client);
  void write(Client *tag, const char *msg, unsigned len_msg);
  void flush(Client *tag);

  int vsys_msg(const char *fmt, va_list args);
  int vprintf(const char *fmt, va_list args);

  void show(Client *c);
  void hide(Client *c);
  void connect(Client *client);
  void disconnect(Client *client, bool show_prompt = true);

  void input(cxx::String const &buf);
  void add_frontend(Frontend *f);
  void cat(Client *c, bool add_nl);
  void tail(Client *tag, int numlines, bool add_nl);

  bool is_connected() const { return _connected != _self_client; }
  char const *name() const { return _name; }

private:
  typedef cxx::H_list<Frontend> Fe_list;
  typedef Fe_list::Iterator Fe_iter;
  typedef Fe_list::Const_iterator Const_fe_iter;

  struct Mux_input_buf : public Input_buf
  {
    char b[512];
    unsigned p;
    unsigned in_cmd_seq;
    struct Esc
    {
      enum Key { None, Left, Right, Up, Down, Page_up, Page_down };
      enum Mod { Normal, CtrlShift, Control, Shift };
      Esc() : key(None), mod(Normal), in_seq(0) {}
      void last_key(Key k) { key = k; in_seq = 0; }
      Key key;
      Mod mod;
      unsigned in_seq;
    };
    Esc esc;

    Mux_input_buf() : p(0), in_cmd_seq(0)  {}
    cxx::String string() const { return cxx::String(b, p); }
    void clear() { p = 0; }
    void add(char c) { if (p < sizeof(b)) b[p++] = c; }
    bool full() const { return p == sizeof(b); }

    int replace(cxx::String &del, cxx::String add)
    {
      if (p + add.len() - del.len() >= sizeof(b))
        return -L4_ENOMEM;
      if (del.len() == 0 && add.len() == 0)
        return -L4_EINVAL;
      if (del.start() == 0)
        return -L4_EINVAL;

      if (add.start() == 0 || add.len() == 0)
        add.start(del.start());

      memmove((char *)del.start() + add.len(), del.end(),
              (char *)(b + p) - del.end());
      memcpy((void *)del.start(), add.start(), add.len());
      p += add.len() - del.len();
      return 0;
    }
  };


  void w(const char *s) const { write(s, strlen(s)); }

  void prompt() const;

  void handle_prompt_input(cxx::String const &buf);
  void handle_vcon_input(cxx::String const &buf);
  bool handle_cmd_seq_global(const char k);
  void clear_seq_print(bool erase);
  bool inject_to_read_buffer(char c);

  void do_client_output(Client *v, int taillines, bool add_nl);

  // Sink::write
  void write(char const *buf, unsigned size) const
  {
    for (Fe_iter i = const_cast<Fe_list&>(_fe).begin(); i != _fe.end(); ++i)
      {
        for (unsigned l = 0; l < size; )
          {
            int r = i->write(buf + l, size - l);
            if (r < 0)
              break;

            l += r;
          }
      }
  }

  Fe_list _fe;

  Client *_self_client;
  Pbuf ob;
  Client *_last_output_client;
  Client *_connected;
  Output_mux *_pre_connect_output;
  unsigned _tag_len;
  Mux_input_buf _inp;
  Controller *_ctl;
  char const *_name;
  char const *_seq_str;
};
