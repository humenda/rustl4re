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

#include "frontend.h"
#include "client.h"
#include "mux.h"

#include <l4/cxx/hlist>
#include <l4/cxx/string>

class String_set_iter
{
public:
  virtual cxx::String value(cxx::String buf) const = 0;
  virtual void next() = 0;
  virtual bool has_more() const = 0;
};

class Input_buf
{
public:
  virtual int replace(cxx::String &del, cxx::String add) = 0;
  virtual cxx::String string() const = 0;
};


class Controller
{
public:
  int cmd(Mux *mux, cxx::String const &s);

  struct Arg
  {
    cxx::String a;
    unsigned prespace;
  };

  typedef int (Controller::*Cmd_func)(Mux *mux, int, Arg *);
  typedef int (Controller::*Cmd_complete_func)(Mux *mux, unsigned argc,
                                               unsigned argnr, Arg *arg,
                                               cxx::String &completed_arg) const;

  struct Cmd
  {
    const char *name;
    const char *helptext;
    Cmd_func func;
    Cmd_complete_func complete_func;
  };

  int complete(Mux *mux, unsigned cur_pos, Input_buf *ibuf);

private:
  Client *get_client(Mux *mux, int argc, int idx, Arg *a);

  int cmd_cat(Mux *mux, int, Arg *);
  int cmd_clear(Mux *mux, int, Arg *);
  int cmd_connect(Mux *mux, int, Arg *);
  int cmd_drop(Mux *mux, int, Arg *);
  int cmd_help(Mux *mux, int, Arg *);
  int cmd_hide(Mux *mux, int, Arg *);
  int cmd_hideall(Mux *mux, int, Arg *);
  int cmd_grep(Mux *mux, int, Arg *);
  int cmd_info(Mux *mux, int, Arg *);
  int cmd_keep(Mux *mux, int, Arg *);
  int cmd_key(Mux *mux, int, Arg *);
  int cmd_kick(Mux *mux, int, Arg *);
  int cmd_list(Mux *mux, int, Arg *);
  int cmd_show(Mux *mux, int, Arg *);
  int cmd_showall(Mux *mux, int, Arg *);
  int cmd_tail(Mux *mux, int, Arg *);
  int cmd_timestamp(Mux *mux, int, Arg *);

  int complete_console_name(Mux *mux, unsigned argc, unsigned argnr,
                            Arg *arg, cxx::String &completed_arg,
                            unsigned num_arg) const;
  int complete_console_name_1(Mux *mux, unsigned argc, unsigned argnr,
                              Arg *arg, cxx::String &completed_arg) const;
  int complete_console_name_grep(Mux *mux, unsigned argc, unsigned argnr,
                                 Arg *arg, cxx::String &completed_arg) const;

  int complete(Mux *mux, cxx::String arg, String_set_iter *i,
               cxx::String &completed_arg) const;

  Cmd const *match_cmd(Mux *mux, cxx::String const &s) const;

  unsigned split_input(cxx::String const &s, Arg *args,
                       const unsigned max_args, bool ignore_empty_last_arg);

  static Cmd _cmds[];

  typedef cxx::H_list<Client> Client_list;
  typedef Client_list::Const_iterator Client_const_iter;

  struct Grep_lineinfo
  {
    explicit Grep_lineinfo(Client::Buf::Index l) : line(l), flags(0) {}
    Client::Buf::Index line;
    unsigned           flags;
  };
public:
  typedef Client_list::Iterator Client_iter;

  Client_list clients;
};

namespace std
{
  template<>
  struct iterator_traits<Controller::Client_iter>
  {
    typedef forward_iterator_tag iterator_category;
  };
};
