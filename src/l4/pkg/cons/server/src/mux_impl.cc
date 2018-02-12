/*
 * (c) 2012-2013 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "frontend.h"
#include "client.h"
#include "mux_impl.h"

#include <algorithm>
#include <cassert>

void Pbuf::flush()
{
  _sink->write(_b, _p);
  _p = 0;
}

void Pbuf::checknflush(int n)
{
  char *x = 0;
  assert(_p + n <= size());
  x = (char*)memchr(_b, '\n', _p + n);

  if (x)
    {
      int rem = n - (x - _b + 1 - _p);
      _p = x - _b + 1;
      flush();
      if (rem)
        memmove(_b, x + 1, rem);
      _p = rem;
    }
  else
    _p += n;
}

void Pbuf::printf(char const *fmt, ...)
{
  if (!fits(strlen(fmt) + 50))
    flush();
  int n;
  va_list arg;
  va_start(arg, fmt);
  n = vsnprintf(_b + _p, size() - _p, fmt, arg);
  va_end(arg);
  if (n > 0)
    checknflush(std::min<unsigned long>(n, size() - _p));
}

void Pbuf::outnstring(char const *str, unsigned long len)
{
  assert(len <= size());
  if (!fits(len))
    flush();
  memcpy(_b + _p, str, len);
  checknflush(len);
}

namespace {
class Mux_client : public Client
{
public:
  Mux_client(Mux *mux) : Client("CONS", 0, 512, 512, Key()) { output_mux(mux); }
  void trigger() const {}
};
}

Mux_i::Mux_i(Controller *ctl, char const *name)
: _fe(0), _self_client(new Mux_client(this)),
  ob(this), _last_output_client(0),
  _connected(_self_client), _tag_len(8), _ctl(ctl),
  _name(name), _seq_str("[Ctrl-E]")
{
}

void
Mux_i::do_client_output(Client *v, int taillines, bool add_nl)
{
  Client::Buf const *b = v->wbuf();
  Client::Buf::Index p = b->tail();
  if (taillines != -1)
    {
      p = b->head();
      while (taillines--)
        {
          p = b->find_backwards('\n', p);
          if (p == b->tail())
            break;
        }

      if (p != b->head() && (*b)[p] == '\n')
        ++p;
    }

  b->write(p, b->head(), _self_client);
  flush(_self_client);

  if (add_nl)
    {
      p = b->head();
      if (p != b->tail() && (*b)[--p] != '\n')
        write("\r\n", 2);
    }
}

void
Mux_i::cat(Client *v, bool add_nl)
{
  do_client_output(v, -1, add_nl);
}

void
Mux_i::tail(Client *v, int numlines, bool add_nl)
{
  do_client_output(v, numlines, add_nl);
}

void
Mux_i::write_tag(Client *client)
{
  std::string const & n = client->tag();
  ob.outnstring(n.c_str(), cxx::min<unsigned long>(n.size(), _tag_len));
  if (n.size() < _tag_len)
    ob.outnstring("             ", _tag_len - n.size());

  if (client->output_line_preempted())
    ob.printf(": ");
  else
    ob.printf("| ");
}

void
Mux_i::flush(Client *)
{
  ob.flush();
}

void
Mux_i::write(Client *client, const char *msg, unsigned len_msg)
{
  bool tagged = client != _connected;
  int color = client->color();

  if (_last_output_client
      && _last_output_client != client
      && _last_output_client->output_line_preempted()
      && _last_output_client->color()
      && tagged)
    ob.printf("\033[0m");

  int input_check_cnt = 0;
  while (len_msg > 0 && msg[0])
    {
      if (_last_output_client != client)
        {
          if (tagged && color)
            ob.printf("\033[%s3%dm", (color & 8) ? "01;" : "", (color & 7));
          else
            ob.printf("\033[0m");

          if (_last_output_client != 0)
            ob.printf("\r\n");

          if (tagged)
            write_tag(client);
        }

      long i;
      for (i = 0; i < (long)len_msg; ++i)
        if (msg[i] == '\n' || msg[i] == 0 || i == (long)ob.size())
          break;

      ob.outnstring(msg, i);

      if (i < (long)len_msg && msg[i] == '\n')
        {
          if (tagged && color)
            ob.printf("\033[0m\n");
          else
            ob.printf("\n");
          client->output_line_done();
          _last_output_client = 0;
          ++i;
        }
      else
        {
          _last_output_client = client;
          client->preempt_output_line();
        }

      msg += i;
      len_msg -= i;

      input_check_cnt += i;
      if (input_check_cnt > 1500)
        {
          for (Fe_iter i = const_cast<Fe_list&>(_fe).begin();
               i != _fe.end(); ++i)
            if (i->check_input())
              {
                ob.printf("[Got input, stopping output.]\n");
                return;
              }
          input_check_cnt = 0;
        }
    }
}


int
Mux_i::vprintf(const char *fmt, va_list args)
{
  char buffer[1024];
  int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
  if (n > 0)
    _self_client->cooked_write(buffer, n);
  return n;
}

void
Mux_i::add_frontend(Frontend *f)
{
  f->input_mux(this);
  _fe.add(f);
  if (!is_connected())
    prompt();
}

int
Mux_i::vsys_msg(const char *fmt, va_list args)
{
  char buffer[1024];
  buffer[0] = '\n';

  int n = vsnprintf(buffer + 1, sizeof(buffer) - 1, fmt, args);
  if (n > 0)
    {
      _self_client->cooked_write(buffer, n+1);
      ob.flush();
    }
  if (!is_connected())
    prompt();
  return 0;
}

void
Mux_i::connect(Client *client)
{
  tail(client, 10, false);
  _last_output_client = client;

  _pre_connect_output = client->output_mux();

  if (_pre_connect_output)
    _pre_connect_output->disconnect(client);

  _connected = client;
  client->output_mux(this);
}

void
Mux_i::disconnect(Client *client, bool show_prompt)
{
  if (_connected != client || client == _self_client)
    return;

  _connected = _self_client;
  client->output_mux(_pre_connect_output);
  if (show_prompt)
    prompt();
}

void
Mux_i::show(Client *c)
{
  Output_mux *m = c->output_mux();
  if (m == this)
    return;

  if (m)
    m->disconnect(c);

  c->output_mux(this);
}

void
Mux_i::hide(Client *c)
{
  Output_mux *m = c->output_mux();
  if (m)
    {
      m->disconnect(c);
      c->output_mux(0);
    }
}

void Mux_i::prompt() const
{
  char _b[25];
  char *p = _b;
  for (unsigned i = 0; i < sizeof(_b)-2 && _name[i]; ++i, ++p)
    *p = _name[i];

  p[0] = '>';
  p[1] = ' ';

  _self_client->cooked_write(_b, p - _b + 2);
  if (_inp.p)
    _self_client->cooked_write(_inp.b, _inp.p);
}

bool
Mux_i::inject_to_read_buffer(char c)
{
  const l4_vcon_attr_t *a = _connected->attr();

  if (a->i_flags & L4_VCON_INLCR && c == '\n')
    c = '\r';
  if (a->i_flags & L4_VCON_IGNCR && c == '\r')
    return 0;
  if (a->i_flags & L4_VCON_ICRNL && c == '\r')
    c = '\n';

  bool do_trigger = _connected->rbuf()->put(c);
  if (_connected->attr()->l_flags & L4_VCON_ECHO)
    write(&c, 1);
  return do_trigger;
}

bool
Mux_i::handle_cmd_seq_global(const char k)
{
  switch (k)
    {
    case 'h':
    case 's':
        {
          Client *c = is_connected() ? _connected : 0;
          for (Controller::Client_iter i = _ctl->clients.begin();
               i != _ctl->clients.end(); ++i)
            {
              if (k == 's')
                {
                  if (!i->output_mux())
                    show(*i);
                }
              else
                {
                  if (i->output_mux() == this && *i != c)
                    hide(*i);
                }
            }
        }
      break;
    default:
        {
          Controller::Client_iter c
            = std::find_if(_ctl->clients.begin(),
                           Controller::Client_iter(_ctl->clients.end()),
                           Client::Equal_key(Client::Key(k)));

          if (c != _ctl->clients.end())
            {
              if (*c != _connected)
                {
                  disconnect(_connected, false);
                  printf("------------- Connecting to '%s' -------------\n",
                         c->tag().c_str());
                  connect(*c);
                  return true;
                }
            }
        }
    };

  return false;
}

void
Mux_i::clear_seq_print(bool erase)
{
  for (unsigned a = 0; a < strlen(_seq_str); ++a)
    _connected->write(erase ? "\b \b" : "\b");
  flush(_connected);
}

void
Mux_i::handle_vcon_input(cxx::String const &buf)
{
  if (buf.empty())
    return;

  bool do_trigger = false;
  for (cxx::String::Index i = buf.start(); !buf.eof(i); ++i)
    {
      if (_inp.in_cmd_seq == 1)
        {
          _inp.in_cmd_seq = 0;
          switch (buf[i])
            {
            case '.':
              disconnect(_connected);
              handle_prompt_input(buf.substr(i + 1));
              return;
            case 'q':
              clear_seq_print(false);
              do_trigger |= inject_to_read_buffer(27);
              break;
            case 'e':
              clear_seq_print(false);
              do_trigger |= inject_to_read_buffer('');
              break;
            case 'z':
              clear_seq_print(false);
              do_trigger |= inject_to_read_buffer('');
              break;
            case 'c':
              clear_seq_print(false);
              do_trigger |= inject_to_read_buffer(3);
              break;
            case 'l':
              clear_seq_print(true);
              do_trigger |= _connected->rbuf()->put_break();
              _connected->write("[Break]");
              flush(_connected);
              break;
            default:
              clear_seq_print(true);
              if (handle_cmd_seq_global(buf[i]))
                return;

              do_trigger |= inject_to_read_buffer(5);
              break;
            }
        }
      else if (buf[i] == 5) // ctrl-e
        {
          _connected->write(_seq_str);
          flush(_connected);
          _inp.in_cmd_seq++;
        }
      else
        do_trigger |= inject_to_read_buffer(buf[i]);

      if (do_trigger)
        _connected->trigger();
    }
}

void
Mux_i::handle_prompt_input(cxx::String const &buf)
{
  if (buf.empty())
    return;

  for (cxx::String::Index i = buf.start(); !buf.eof(i); ++i)
    {
      if (_inp.in_cmd_seq)
        {
          handle_cmd_seq_global(buf[i]);
          _inp.in_cmd_seq = 0;
        }
      else if (_inp.esc.in_seq)
        {
          if (_inp.esc.in_seq == 1)
            {
              if (buf[i] == '[')
                ++_inp.esc.in_seq;
              else
                _inp.esc.in_seq = 0;
            }
          else if (_inp.esc.in_seq == 2)
            {
              switch (buf[i])
                {
                case 'A': _inp.esc.last_key(Mux_input_buf::Esc::Up); break;
                case 'B': _inp.esc.last_key(Mux_input_buf::Esc::Down); break;
                case 'C': _inp.esc.last_key(Mux_input_buf::Esc::Right); break;
                case 'D': _inp.esc.last_key(Mux_input_buf::Esc::Left); break;
                case '5': _inp.esc.key = Mux_input_buf::Esc::Page_up;
                          ++_inp.esc.in_seq; break;
                case '6': _inp.esc.key = Mux_input_buf::Esc::Page_down;
                          ++_inp.esc.in_seq; break;
                default: if (0) printf("Ukn-%c-\n", buf[i]); break;
                };
            }
          else if (_inp.esc.in_seq == 3)
            {
              switch (buf[i])
                {
                case '~': _inp.esc.mod = Mux_input_buf::Esc::Normal; break;
                case '@': _inp.esc.mod = Mux_input_buf::Esc::CtrlShift; break;
                case '^': _inp.esc.mod = Mux_input_buf::Esc::Control; break;
                case '$': _inp.esc.mod = Mux_input_buf::Esc::Shift; break;
                default: _inp.esc.key = Mux_input_buf::Esc::None; break;
                };

              _inp.esc.in_seq = 0;
            }
          else
            _inp.esc.in_seq = 0;

          if (_inp.esc.in_seq == 0)
            {
              if (0) switch (_inp.esc.key)
                {
                case Mux_input_buf::Esc::Left: printf("LEFT\n"); break;
                case Mux_input_buf::Esc::Right: printf("RIGHT\n"); break;
                case Mux_input_buf::Esc::Up: printf("UP\n"); break;
                case Mux_input_buf::Esc::Down: printf("DOWN\n"); break;
                case Mux_input_buf::Esc::Page_up: printf("PGUP\n"); break;
                case Mux_input_buf::Esc::Page_down: printf("PGDOWN\n"); break;
                case Mux_input_buf::Esc::None: break;
                };
              _inp.esc.key = Mux_input_buf::Esc::None;
            }
        }
      else
        {
          switch (buf[i])
            {
            case 5: // ctrl-E
              _inp.in_cmd_seq++;
              break;
            case 9: // TAB
                {
                  int cnt = _ctl->complete(this, 0, &_inp);
                  if (cnt > 0)
                    {
                      write("\r", 1);
                      prompt();
                    }
                }
              break;
            case 12: // ctrl-L
              printf("\033[H\033[2J");
              prompt();
              break;
            case 10:
              break;
            case 13:
              w("\r\n");
              _ctl->cmd(this, _inp.string());
              _inp.clear();
              if (!is_connected())
                prompt();
              break;
            case 127: // BS
              if (_inp.p)
                {
                  w("\b \b");
                  --_inp.p;
                }
              break;
            case 27:
              _inp.esc.in_seq++;
              break;
            default:
              _inp.add(buf[i]);
              write(&buf[i], 1);
            };
        }

      if (_inp.full())
        break;
    }
}

void
Mux_i::input(cxx::String const &buf)
{
  if (is_connected())
    handle_vcon_input(buf);
  else
    handle_prompt_input(buf);
}
