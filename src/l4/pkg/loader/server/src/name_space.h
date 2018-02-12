/*
 * (c) 2008-2009 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/re/util/name_space_svr>
#include <l4/sys/cxx/ipc_epiface>

namespace Ldr {

namespace Names { using namespace L4Re::Util::Names; }

class Entry : public Names::Entry
{
public:
  Entry(Names::Name const &n, Names::Obj const &o, bool dyn = false)
  : Names::Entry(n, o, dyn) {}

  void * operator new (size_t s) { return ::operator new(s); }
  void operator delete(void *b) { ::operator delete(b); }
};

class Name_space : public L4::Epiface_t<Name_space, L4Re::Namespace>,
                   public Names::Name_space
{
public:
  Name_space();
  ~Name_space();

  // server support ----------------------------------------
  int get_epiface(l4_umword_t data, bool is_local, L4::Epiface **lo) override;
  int copy_receive_cap(L4::Cap<void> *cap) override;
  void free_capability(L4::Cap<void> cap) override;
  void free_epiface(L4::Epiface *epiface) override;
  Entry *alloc_dynamic_entry(Names::Name const &n, unsigned flags) override;
  void free_dynamic_entry(Names::Entry *e) override;

  // internally used to register bootfs files, name spaces...
  int register_obj(Names::Name const &name, Names::Obj const &o,
                   bool dyn = false)
  {
    Entry *n = new Entry(name, o, dyn);
    int err = insert(n);
    if (err < 0)
      delete n;

    return err;
  }
};

}

inline
L4::BasicOStream &operator << (L4::BasicOStream &s, L4Re::Util::Names::Name const &n)
{
  for (int l = 0; l < n.len(); ++l)
    s << n.name()[l];

  return s;
}
