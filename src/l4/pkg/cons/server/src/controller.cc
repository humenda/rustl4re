/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <cstdio>

#include "controller.h"
#include <algorithm>
#include <vector>

Controller::Cmd Controller::_cmds[] =
    {
      { "c",       0,                                     &Controller::cmd_connect,         &Controller::complete_console_name_1 },
      { "cat",     "Dump buffer of channel",              &Controller::cmd_cat,             &Controller::complete_console_name_1 },
      { "clear",   "Clear screen",                        &Controller::cmd_clear,           0 },
      { "connect", "Connect to channel",                  &Controller::cmd_connect,         &Controller::complete_console_name_1 },
      { "drop",    "Drop kept client",                    &Controller::cmd_drop,            &Controller::complete_console_name_1 },
      { "grep",    "Search for text",                     &Controller::cmd_grep,            &Controller::complete_console_name_grep },
      { "help",    "Help screen",                         &Controller::cmd_help,            0 },
      { "hide",    "Hide channel output",                 &Controller::cmd_hide,            &Controller::complete_console_name_1 },
      { "hideall", "Hide all channels output",            &Controller::cmd_hideall,         0 },
      { "info",    "Info screen",                         &Controller::cmd_info,            0 },
      { "keep",    "Keep client from garbage collection", &Controller::cmd_keep,            &Controller::complete_console_name_1 },
      { "key",     "Set key shortcut for channel",        &Controller::cmd_key,             &Controller::complete_console_name_1 },
      { "kick",    0,                                     &Controller::cmd_kick,            &Controller::complete_console_name_1 },
      { "list",    "List channels",                       &Controller::cmd_list,            0 },
      { "ls",      0,                                     &Controller::cmd_list,            0 },
      { "show",    "Show channel output",                 &Controller::cmd_show,            &Controller::complete_console_name_1 },
      { "showall", "Show all channels output",            &Controller::cmd_showall,         0 },
      { "tail",    "Show last lines of output",           &Controller::cmd_tail,            &Controller::complete_console_name_1 },
      { "timestamp", "Prefix log with timestamp",         &Controller::cmd_timestamp,       &Controller::complete_console_name_1 },
      { 0, 0, 0, 0 }
    };

namespace {
class Cmd_name_iter : public String_set_iter
{
public:
  explicit Cmd_name_iter(Controller::Cmd const *cmd) : _cmd(cmd) {}

  cxx::String value(cxx::String) const
  { return _cmd->name; }

  void next()
  { ++_cmd; }

  bool has_more() const
  { return _cmd->name; }

private:
  Controller::Cmd const *_cmd;
};

class Client_name_iter : public String_set_iter
{
public:
  explicit Client_name_iter(cxx::H_list<Client> const *c)
  : _client(c->begin()), _e(c->end())
  {}

  cxx::String value(cxx::String buf) const
  {
    if (_client->idx == 0)
      return _client->tag().c_str();

    snprintf((char *)buf.start(), buf.len(), "%s:%d",
             _client->tag().c_str(), _client->idx);
    return buf;
  }

  void next()
  { ++_client; }

  bool has_more() const
  { return _client != _e; }

private:
  cxx::H_list<Client>::Const_iterator _client, _e;
};
}

Client *
Controller::get_client(Mux *mux, int argc, int idx, Arg *a)
{
  if (argc <= idx)
    {
      mux->printf("%.*s: invalid number of arguments (need %d, got %d)\n",
                  a[0].a.len(), a[0].a.start(), idx + 1, argc);
      return 0;
    }

  Client_iter c = std::find_if(clients.begin(), Client_iter(clients.end()),
                               Client::Equal_tag_idx(a[idx].a));
  if (c != clients.end())
    return *c;

  mux->printf("%.*s: console '%.*s' not found\n",
              a[0].a.len(), a[0].a.start(), a[idx].a.len(), a[idx].a.start());
  return 0;
}

Controller::Cmd const *
Controller::match_cmd(Mux *, cxx::String const &s) const
{
  for (Cmd const *i = _cmds; i->name; ++i)
    {
      cxx::String n(i->name);
      if (s == n)
        return i;
    }

  return 0;
}

unsigned
Controller::split_input(cxx::String const &s, Arg *args,
                        const unsigned max_args,
                        bool ignore_empty_last_arg)
{
  cxx::String::Index e = s.start();
  unsigned num_args = 0;
  unsigned prespace;

  while (!s.eof(e))
    {
      prespace = 0;
      while (!s.eof(e) && isspace(s[e]))
        {
          ++e;
          ++prespace;
        }

      if (s.eof(e))
        {
          if (num_args < max_args)
            {
              args[num_args].prespace = prespace;
              args[num_args++].a = cxx::String();
            }
          break;
        }

      cxx::String::Index arg_start = e;
      while (!s.eof(e) && !isspace(s[e]))
        ++e;

      if (num_args < max_args)
        {
          args[num_args].prespace = prespace;
          args[num_args++].a = cxx::String(arg_start, e);
        }
      else
        break;
    }

  if (ignore_empty_last_arg
      && num_args && args[num_args - 1].a.len() == 0)
    --num_args;

  return num_args;
}

int
Controller::complete(Mux *mux, unsigned cur_pos, Input_buf *ibuf)
{
  (void)cur_pos;
  enum { Max_args = 16 };
  Arg args[Max_args];
  int num_args = split_input(ibuf->string(), args, Max_args, false);

  char outbuf[100];
  cxx::String outarg = cxx::String(outbuf, sizeof(outbuf));

  int arg_to_complete;
  int cnt;
  if (num_args < 2)
    {
      Cmd_name_iter i(_cmds);
      cnt = complete(mux, args[0].a, &i, outarg);
      arg_to_complete = 0;
    }
  else
    {
      Cmd const *c = match_cmd(mux, args[0].a);
      if (!c || !c->complete_func)
        return 0;

      arg_to_complete = num_args - 1;

      cnt = (this->*(c->complete_func))(mux, num_args, arg_to_complete,
                                        args, outarg);
    }

  if (cnt > 0)
    {
      if (num_args == 0)
        ibuf->string().len(0);
      else
        {
          if (cnt == 1 && outarg.len() < (int)sizeof(outbuf))
            {
              char *e = (char *)outarg.end();
              *e = ' ';
              outarg.len(outarg.len() + 1);
            }
          ibuf->replace(args[arg_to_complete].a, outarg);
        }
    }
  return cnt;
}

int
Controller::cmd(Mux *mux, cxx::String const &s)
{
  cxx::String::Index i = s.start();
  while (!s.eof(i) && isspace(s[i]))
    ++i;

  cxx::String::Index e = cxx::String(i, s.end()).find_match(isspace);
  cxx::String cmd(cxx::String(i, e));

  if (cmd.empty())
    return 0;

  enum { Max_args = 16 };
  Arg args[Max_args];
  unsigned num_args = split_input(s, args, Max_args, true);

  if (Cmd const *c = match_cmd(mux, cmd))
    (this->*(c->func))(mux, num_args, args);
  else
    mux->printf("Unknown command '%.*s'. Use 'help'.\n", cmd.len(), cmd.start());

  return 0;
}

int
Controller::cmd_info(Mux *mux, int, Arg *)
{
  mux->printf("Cons -- Vcon multiplexer\n");
  return 0;
}

int
Controller::cmd_help(Mux *mux, int, Arg *)
{
  for (Cmd const *i = _cmds; i->name; ++i)
    if (i->helptext)
      mux->printf("%15s - %s\n", i->name, i->helptext);
  mux->printf("\nKey shortcuts when connected:\n");
  mux->printf("   Ctrl-E .     - Disconnect\n");
  mux->printf("   Ctrl-E e     - Inject Ctrl-E\n");
  mux->printf("   Ctrl-E c     - Inject Ctrl-C\n");
  mux->printf("   Ctrl-E z     - Inject Ctrl-Z\n");
  mux->printf("   Ctrl-E q     - Inject ESC\n");
  mux->printf("   Ctrl-E l     - Inject Break sequence\n");
  mux->printf("\nGlobal key shortcuts:\n");
  mux->printf("   Ctrl-E h     - Hide all output (except current)\n");
  mux->printf("   Ctrl-E s     - Show all output\n");

  bool first = true;
  for (Client_iter i = clients.begin(); i != clients.end(); ++i)
    if (!i->key().is_nil())
      {
        if (first)
          {
            mux->printf("\nUser defined key shortcuts:\n");
            first = false;
          }
        mux->printf("   Ctrl-E %c     - Connect to console '%s'\n",
                    i->key().v(), i->tag().c_str());
      }
  return 0;
}

int
Controller::cmd_list(Mux *mux, int argc, Arg *args)
{
  bool long_mode = false;

  if (argc > 1 && args[1].a.len() > 1
               && args[1].a[0] == '-' && args[1].a[1] == 'l')
    long_mode = true;

  for (Client_iter i = clients.begin(); i != clients.end(); ++i)
    {
      mux->printf("%14s%s%.0d %c%c%c [%8s] out:%5ld/%6ld in:%5ld/%5ld%s",
                  i->tag().c_str(), i->idx ? ":" : "", i->idx,
                  i->key().is_nil() ? ' ': '(',
                  i->key().is_nil() ? ' ': i->key().v(),
                  i->key().is_nil() ? ' ': ')',
                  i->output_mux() ?  i->output_mux()->name() : "",
                  i->wbuf()->stat_lines(), i->wbuf()->stat_bytes(),
                  i->rbuf()->stat_lines(), i->rbuf()->stat_bytes(),
                  i->dead() ? " [X]" : "");

      if (long_mode)
        mux->printf(" pend=%d attr:o=%lo,i=%lo,l=%lo",
                    i->rbuf()->distance(),
                    i->attr()->o_flags, i->attr()->i_flags,
                    i->attr()->l_flags);
      mux->printf("\n");
    }

  return L4_EOK;
}

int
Controller::complete_console_name(Mux *mux, unsigned, unsigned argnr, Arg *arg,
                                  cxx::String &completed_arg,
                                  unsigned num_arg) const
{
  if (argnr != num_arg)
    return 0;

  Client_name_iter i(&clients);
  return complete(mux, arg[argnr].a, &i, completed_arg);
}

int
Controller::complete_console_name_1(Mux *mux, unsigned argc, unsigned argnr,
                                    Arg *arg,
                                    cxx::String &completed_arg) const
{
  return complete_console_name(mux, argc, argnr, arg, completed_arg, 1);
}

int
Controller::complete(Mux *mux, cxx::String arg, String_set_iter *i,
                     cxx::String &completed_arg) const
{
  int cnt = 0, l = 0;
  char buf[40], first[40];
  cxx::String found;
  for (; i->has_more(); i->next())
    {
      cxx::String cname
        = i->value(cxx::String(found.empty() ? first : buf,
                               found.empty() ? sizeof(first) : sizeof(buf)));
      if (arg.empty() || cname.starts_with(arg))
        {
          ++cnt;

          if (cnt == 2) // oh at least two hits, print the first one too
            l += mux->printf("\n%.*s ", found.len(), found.start());

          if (cnt > 1)
            l += mux->printf("%.*s ", cname.len(), cname.start());

          if (l > 68)
            {
              l = 0;
              mux->printf("\n");
            }

          if (!found.empty())
            {
              int x;
              for (x = 0; x < found.len() && x < cname.len(); ++x)
                if (found[x] != cname[x])
                  break;
              found.len(x);
            }
          else
            found = cname;
        }
    }

  int len = found.len();
  if (len > completed_arg.len())
    return 0;

  memcpy((void *)completed_arg.start(), found.start(), len);
  completed_arg.len(len);

  if (cnt > 1)
    mux->printf("\n");

  return cnt;
}

int
Controller::cmd_cat(Mux *mux, int argc, Arg *a)
{
  if (Client *v = get_client(mux, argc, 1, a))
    mux->cat(v, true);
  return 0;
}

int
Controller::cmd_tail(Mux *mux, int argc, Arg *a)
{
  Client *c = get_client(mux, argc, 1, a);
  if (c)
    {
      int numlines = 20;

      if (argc > 2)
        {
          if (!a[2].a.from_dec(&numlines))
            {
              mux->printf("Invalid argument '%.*s'\n",
                          a[2].a.len(), a[2].a.start());
              return 0;
            }
        }

      mux->tail(c, numlines, true);
    }
  return 0;
}

int
Controller::cmd_timestamp(Mux *mux, int argc, Arg *a)
{
  Client *c = get_client(mux, argc, 1, a);
  if (!c)
    return 0;

  c->timestamp(a[2].a != "off");
  return 0;
}

int
Controller::cmd_clear(Mux *mux, int, Arg *)
{
  mux->printf("\033[H\033[2J");
  return 0;
}

int
Controller::cmd_connect(Mux *mux, int argc, Arg *a)
{
  if (Client *v = get_client(mux, argc, 1, a))
    {
      mux->connect(v);
      return 0;
    }
  return -L4_ENODEV;
}

int
Controller::cmd_show(Mux *mux, int argc, Arg *a)
{
  if (Client *v = get_client(mux, argc, 1, a))
    mux->show(v);
  return 0;
}

int
Controller::cmd_hide(Mux *mux, int argc, Arg *a)
{
  if (Client *v = get_client(mux, argc, 1, a))
    mux->hide(v);
  return 0;
}

int
Controller::cmd_showall(Mux *mux, int, Arg *)
{
  // show all not yet displayed clients on /mux/
  for (Client_iter i = clients.begin(); i != clients.end(); ++i)
    if (!i->output_mux())
      mux->show(*i);
  return 0;
}

int
Controller::cmd_hideall(Mux *mux, int, Arg *)
{
  for (Client_iter i = clients.begin(); i != clients.end(); ++i)
    if (i->output_mux() == mux)
      mux->hide(*i);
  return 0;
}


int
Controller::cmd_keep(Mux *mux, int argc, Arg *a)
{
  if (Client *v = get_client(mux, argc, 1, a))
    v->keep(true);
  return 0;
}

int Controller::cmd_key(Mux *mux, int argc, Arg *a)
{
  if (argc < 2)
    {
      mux->printf("Usage: key channel character\n");
      return 0;
    }

  if (Client *v = get_client(mux, argc, 1, a))
    v->key(Client::Key(a[2].a[0]));

  return 0;
}

int
Controller::cmd_kick(Mux *mux, int argc, Arg *a)
{
  if (Client *v = get_client(mux, argc, 1, a))
    v->trigger();
  return 0;
}

int
Controller::cmd_drop(Mux *mux, int argc, Arg *a)
{
  if (Client *v = get_client(mux, argc, 1, a))
    {
      v->keep(false);
      if (v->dead())
	delete v;
    }

  return 0;
}

int
Controller::complete_console_name_grep(Mux *mux, unsigned, unsigned argnr,
                                       Arg *arg,
                                       cxx::String &completed_arg) const
{
  int cnt = 0;
  for (unsigned i = 1; i < argnr + 1; ++i)
    {
      if (!arg[i].a.empty() && arg[i].a[0] == '-')
        continue;

      if (++cnt == 2)
        {
          Client_name_iter cl(&clients);
          return complete(mux, arg[i].a, &cl, completed_arg);
        }
    }

  return 0;
}

int
Controller::cmd_grep(Mux *mux, int argc, Arg *a)
{
  Client *given_client = 0;
  cxx::String pattern;

  bool opt_line      = false;
  bool opt_word      = false;
  bool opt_igncase   = false;
  bool opt_count     = false;
  bool opt_inv       = false;
  unsigned opt_ctx_b = 0;
  unsigned opt_ctx_a = 0;

  for (int i = 1; i < argc; ++i)
    {
      if (a[i].a[0] == '-')
        {
          for (int idx = 1; idx < a[i].a.len(); ++idx)
            {
              switch (a[i].a[idx])
                {
                case 'n': opt_line    = true; break;
                case 'w': opt_word    = true; break;
                case 'i': opt_igncase = true; break;
                case 'c': opt_count   = true; break;
                case 'v': opt_inv     = true; break;
                case 'A':
                case 'B':
                case 'C':
                          {
                            if (i + 1 == argc || idx + 1 < a[i].a.len())
                              {
                                mux->printf("grep: Missing parameter for option '%c'\n",
                                            a[i].a[idx]);
                                return 1;
                              }

                            switch (a[i].a[idx])
                              {
                              case 'A': a[i + 1].a.from_dec(&opt_ctx_a); break;
                              case 'B': a[i + 1].a.from_dec(&opt_ctx_b); break;
                              case 'C': a[i + 1].a.from_dec(&opt_ctx_a);
                                        opt_ctx_b = opt_ctx_a;
                                        break;
                              }
                            ++i;
                            break;
                          }
                default: mux->printf("grep: Unknown option '%c'\n",
                                     a[i].a[idx]);
                         return 1;
                };
            }
        }
      else if (pattern.empty())
        pattern = a[i].a;
      else
        given_client = get_client(mux, argc, i, a);
      // we should support multiple clients here
    }

  if (opt_igncase)
    for (int i = 0; i < pattern.len(); ++i)
      const_cast<char &>(pattern[i]) = tolower(pattern[i]);

  for (Client_const_iter v = clients.begin(); v != clients.end(); ++v)
    {
      if (given_client && *v != given_client)
        continue;

      Client::Buf const *b = v->wbuf();
      Client::Buf::Index end = b->head();

      enum { Flag_none = 0, Flag_print = 1 << 0, Flag_match = 1 << 1, };
      typedef std::vector<Grep_lineinfo> Line_vector;
      Line_vector lines;

      unsigned print_next_lines = 0;
      Grep_lineinfo curline = Grep_lineinfo(b->tail());
      bool is_match = false;
      unsigned count = 0;
      Client::Buf::Index i = b->tail();
      bool exit_loop = false;
      while (1)
        {
          if (((*b)[i] == '\n' && i + 1 != end)
              || i == end)
            {
              if (print_next_lines)
                {
                  curline.flags |= Flag_print;
                  --print_next_lines;
                }

              if (opt_inv ^ !!is_match)
                {
                  ++count;

                  curline.flags |= Flag_match | Flag_print;

                  unsigned o = cxx::min(lines.size(),
                                        (Line_vector::size_type)opt_ctx_b);

                  for (; o; o--)
                    lines[lines.size() - o].flags |= Flag_print;

                  if (opt_ctx_a > print_next_lines)
                    print_next_lines = opt_ctx_a;
                }

              lines.push_back(curline);
              curline = Grep_lineinfo(i + 1);
              is_match = false;
            }

          if (exit_loop)
            break;

          int idx = 0;
          if (opt_igncase)
            {
              for (; idx < pattern.len(); ++idx)
                if (pattern[idx] != tolower((*b)[i + idx]))
                  break;
            }
          else
            {
              for (; idx < pattern.len(); ++idx)
                if (pattern[idx] != (*b)[i + idx])
                  break;
            }

          if (idx == pattern.len() && (*b)[i] != '\n')
            {
              if (!opt_word ||
                  (!isalnum((*b)[i- 1]) && !isalnum((*b)[i + idx])))
                is_match = true;
            }

          ++i;
          exit_loop = i == end;
        }

      unsigned last_output_idx = ~0U;

      if (!opt_count)
        for (unsigned linenr = 0; linenr < lines.size(); ++linenr)
          {
            if (lines[linenr].flags & Flag_print)
              {
                if (last_output_idx < linenr
                    && last_output_idx != linenr - 1
                    && (opt_ctx_a || opt_ctx_b))
                  mux->printf("--\n");

                if (!given_client)
                  mux->printf("%s%c", v->tag().c_str(),
                              lines[linenr].flags & Flag_match ? ':' : '-');
                if (opt_line)
                  mux->printf("%d%c", linenr + 1,
                              lines[linenr].flags & Flag_match ? ':' : '-');

                Client::Buf::Index le = lines[linenr].line;

                while ((*b)[le] != '\n' && le != end)
                  mux->printf("%c", (*b)[le++]);

                if ((*b)[le] == '\n')
                  mux->printf("\n");

                last_output_idx = linenr;
              }
          }
      else
        {
          if (!given_client)
            mux->printf("%s:", v->tag().c_str());
          mux->printf("%d\n", count);
        }
    }

  return 0;
}
