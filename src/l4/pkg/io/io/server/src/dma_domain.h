#pragma once

#include "resource.h"

#include <l4/sys/task>
#include <l4/re/dma_space>

class Dma_domain_set;
class Dma_domain_group;

class Dma_domain_if
{
protected:
  L4::Cap<L4::Task> _kern_dma_space = L4::Cap<L4::Task>::Invalid;
  static bool _supports_remapping;
  bool _managed_kern_dma_space = false;

public:
  bool managed_kern_dma_space() const
  { return _managed_kern_dma_space; }

  L4::Cap<L4::Task> kern_dma_space() const
  { return _kern_dma_space; }

  virtual void set_managed_kern_dma_space(L4::Cap<L4::Task> s)
  {
    _kern_dma_space = s;
    _managed_kern_dma_space = true;
  }

  virtual int create_managed_kern_dma_space() = 0;
  virtual int set_dma_space(bool set, L4::Cap<L4Re::Dma_space> dma_space);
  virtual int set_dma_task(bool set, L4::Cap<L4::Task> dma_task) = 0;
  virtual ~Dma_domain_if() = default;
};

class Dma_domain : public Resource, public Dma_domain_if
{
  friend class Dma_domain_group;
private:
  static unsigned _next_free_domain;
  /**
   * Virtual DMA domain that domain belongs to.
   *
   * Usually there is one virtual DMA domain per virtual bus, and all physical
   * DMA domains that are assigned to that virtual bus belong to the same
   * virtual DMA domain. If a physical DMA domain is shared among multiple
   * virtual buses these buses also share their virtual DMA domain.
   */
  Dma_domain_set *_v_domain = 0;
  void add_to_set(Dma_domain_set *s);

public:
  Dma_domain(Dma_domain const &) = delete;
  Dma_domain &operator = (Dma_domain const &) = delete;

  Dma_domain()
  : Resource("DMAD", Dma_domain_res, _next_free_domain, _next_free_domain)
  { ++_next_free_domain; }

  void add_to_group(Dma_domain_group *g);
};

class Dma_domain_set : public Dma_domain_if
{
  friend class Dma_domain;
  friend class Dma_domain_group;
private:
  typedef std::vector<Dma_domain *> Domains;
  typedef std::vector<Dma_domain_group *> Groups;
  Domains _domains;
  Groups _groups;

  void add_group(Dma_domain_group *g)
  { _groups.push_back(g); }

  bool rm_group(Dma_domain_group *g)
  {
    for (auto i = _groups.begin(); i != _groups.end(); ++i)
      if (*i == g)
        {
          _groups.erase(i);
          return true;
        }

    return false;
  }

  void add_domain(Dma_domain *d)
  { _domains.push_back(d); }

  bool rm_domain(Dma_domain *d)
  {
    for (auto i = _domains.begin(); i != _domains.end(); ++i)
      if (*i == d)
        {
          _domains.erase(i);
          return true;
        }

    return false;
  }

public:
  int set_dma_task(bool, L4::Cap<L4::Task>) override;

  int create_managed_kern_dma_space() override;
};

class Dma_domain_group
{
  friend class Dma_domain;

private:
  Dma_domain_set *_set = 0;
  void assign(Dma_domain_group const &g)
  {
    if (_set == g._set)
      return;

    if (_set)
      _set->rm_group(this);

    _set = g._set;

    if (_set)
      _set->add_group(this);
  }

  void assign_set(Dma_domain_set *s)
  {
    s->add_group(this);
    _set = s;
  }

public:
  Dma_domain_group() = default;
  Dma_domain_group(Dma_domain_group const &) = delete;
  Dma_domain_group &operator = (Dma_domain_group const &) = delete;

  ~Dma_domain_group()
  {
    if (!_set)
      return;
    _set->rm_group(this);
  }

  Dma_domain_if *operator -> () const { return _set; }
  Dma_domain_if *get() const { return _set; }

private:
  void merge(Dma_domain_set *s);
};

inline void
Dma_domain::add_to_set(Dma_domain_set *s)
{
  s->add_domain(this);
  _v_domain = s;
}

inline void
Dma_domain_group::merge(Dma_domain_set *s)
{
  for (auto g: s->_groups)
    g->assign_set(_set);

  for (auto d: s->_domains)
    d->add_to_set(_set);

  delete s;
}
