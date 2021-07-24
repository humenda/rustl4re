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

#include <string>

#include <l4/cxx/hlist>
#include <l4/sys/vcon>

#include "output_mux.h"

#include <cstring>
#include <l4/cxx/string>

#include <vector>

class Client : public cxx::H_list_item
{
public:
  class Key
  {
  public:
    Key() : _key(0) {}
    explicit Key(int k) : _key(k) {}

    bool is_nil() const { return !_key; }
    bool operator == (Key k) const { return !is_nil() && _key == k._key; }
    void operator = (char v) { _key = v; }
    char v() const { return _key; }

  private:
    char _key;
  };

  class Buf
  {
  public:
    class Index
    {
    public:
      bool operator == (Index v) const { return v.i == i; }
      bool operator != (Index v) const { return v.i != i; }

      Index operator ++ ()
      {
        if (++i == _b->_bufsz)
          i = 0;
        return *this;
      }

      Index operator ++ (int)
      {
        int n = i;
        ++(*this);
        return Index(n, _b);
      }

      Index operator -- ()
      {
        if (i == 0)
          i = _b->_bufsz;
        --i;
        return *this;
      }

      Index operator + (int v)
      {
        int n = i + v;
        if (n > _b->_bufsz || n < 0)
          n %= _b->_bufsz;
        return Index(n, _b);
      }

      Index operator - (int v)
      {
        return this->operator+(-v);
      }

    private:
      friend class Buf;
      explicit Index(int i, Buf const *b) : i(i), _b(b) {}
      int i;
      Buf const *_b;
    };

    explicit Buf(size_t sz)
    : _buf(new char [sz]), _bufsz(sz), _head(0), _tail(0),
      _sum_bytes(0), _sum_lines(0)
    {}

    ~Buf() { delete [] _buf; }

    Index head() const { return Index(_head, this); }
    Index tail() const { return Index(_tail, this); }

    char operator [] (Index const &i) const { return _buf[i.i]; }

    template< typename O >
    int write(Index const &s, Index const &e, O *o) const
    {
      int l = 0;
      if (s.i < e.i)
        l += o->write(_buf + s.i, e.i - s.i);
      else if (s.i > e.i)
        {
          l += o->write(_buf + s.i, _bufsz - s.i);
          l += o->write(_buf, e.i);
        }
      return l;
    }

    bool put_break()
    {
      if (!break_points.size() || break_points.back() != _head)
        break_points.push_back(_head);

      return _tail == _head;
    }

    bool is_next_break(int offset)
    {
      return    break_points.size()
             && ((_head + offset) % _bufsz) == break_points[0];
    }

    void clear_next_break()
    {
      break_points.erase(break_points.begin());
    }

    bool put(char d)
    {
      int was_empty = _tail == _head;
      ++_sum_bytes;

      if (d == '\n')
        ++_sum_lines;

      _buf[_head] = d;

      if (++_head == _bufsz)
        _head = 0;

      if (break_points.size() && break_points[0] == _head)
        clear_next_break();

      if (_head == _tail && ++_tail == _bufsz)
        _tail = 0;

      return was_empty;
    }

    bool put(const char *d, int len)
    {
      int was_empty = _tail == _head;
      _sum_bytes += len;
      while (len--)
        {
          if ((_head + 1) % _bufsz == _tail)
            _tail = (_tail + 1) % _bufsz;

          if (*d == '\n')
            _sum_lines++;

          _buf[_head++] = *d++;
          if(_head == _bufsz)
            _head = 0;

          if (break_points.size() && break_points[0] == _head)
            clear_next_break();
        }
      return was_empty;
    }

    int get(int offset, char const **d) const
    {
      if (offset < 0 || (unsigned)offset >= _sum_bytes)
        return 0;

      offset = (_tail + offset) % _bufsz;
      *d = &_buf[offset];

      if (offset > _head)
        return _bufsz - offset;
      return _head - offset;
    }

    Index find_backwards(char v, Index p) const
    {
      while (p != tail())
        {
          --p;
          if ((*this)[p] == v)
            return p;
        }

      return p;
    }

    Index find_forwards(char v, Index p) const
    {
      while (p != head())
        {
          if ((*this)[p] == v)
            return p;
          ++p;
        }

      return p;
    }

    int distance()
    {
      if (_head >= _tail)
        return _head - _tail;
      return (_head + _bufsz) - _tail;
    }

    void clear(int l)
    {
      if (distance() >= _bufsz)
        _tail = _head;
      else
        _tail = (_tail + l) % _bufsz;
    }

    unsigned long stat_bytes() const { return _sum_bytes; }
    unsigned long stat_lines() const { return _sum_lines; }

  private:
    Buf(Buf const &) = delete;
    Buf &operator = (Buf const &) = delete;
    char *_buf;
    int _bufsz;
    int _head, _tail;
    std::vector<int> break_points;
    unsigned long _sum_bytes, _sum_lines;
  };

  struct Equal_key
  {
    Client::Key k;
    Equal_key(Client::Key const &k) : k(k) {}
    bool operator () (Client const *c) const { return k == c->key(); }
  };

  struct Equal_tag
  {
    cxx::String n;
    Equal_tag(cxx::String const &n) : n(n) {}
    bool operator () (Client const *c) const
    { return n == c->tag().c_str(); }
  };

  struct Equal_tag_idx
  {
    cxx::String n;
    Equal_tag_idx(cxx::String const &n) : n(n) {}
    bool operator () (Client const *c) const
    {
      if (n == c->tag().c_str() && c->idx == 0)
        return true;

      cxx::String::Index r = n.rfind(":");
      if (n.eof(r))
        return false;

      if (n.head(r) != c->tag().c_str())
        return false;

      int idx = 0;
      n.substr(r+1).from_dec(&idx);

      return idx == c->idx;
    }
  };

  Client(std::string const &tag, int color, int rsz, int wsz, Key key);

  virtual ~Client()
  {
    if (output_mux())
      output_mux()->disconnect(this);
  }

  bool collected();
  bool keep() const { return _keep; }
  bool dead() const { return _dead; }
  bool timestamp() const { return _timestamp; }

  void keep(bool keep) { _keep = keep; }
  void timestamp(bool ts) { _timestamp = ts; }

  void output_mux(Output_mux *m) { _output = m; }
  Output_mux *output_mux() const { return _output; }

  int color() const { return _col; }
  std::string const &tag() const { return _tag; }
  Key const key() const { return _key; }
  void key(Key key) { _key = key; }
  bool output_line_preempted() const { return _p; }
  void preempt_output_line() { _p = true; }
  void output_line_done() { _p = false; }

  Buf *rbuf() { return &_rb; }
  Buf const *rbuf() const { return &_rb; }
  Buf *wbuf() { return &_wb; }
  Buf const *wbuf() const { return &_wb; }

  l4_vcon_attr_t const *attr() const { return &_attr; }

  virtual void trigger() const = 0;

  int write(char const *buf, int len)
  { _output->write(this, buf, len); return len; }

  int write(char const *buf)
  { return write(buf, strlen(buf)); }

  void cooked_write(const char *buf, long size = -1) throw();

  int idx;

private:
  int _col;
  std::string _tag;
  bool _p;
  bool _keep;
  bool _timestamp;
  bool _new_line;
  bool _dead;
  Key _key;

  Buf _wb, _rb;

  void print_timestamp();

protected:
  l4_vcon_attr_t _attr;

  Output_mux *_output;
};
