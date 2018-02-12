#ifndef L4_PPC32_OF_H__
#define L4_PPC32_OF_H__

#include <stdarg.h>
#include <string.h>

namespace L4_drivers
{
class Of
{
private:
  /* declarations */
  struct prom_args
    {
      const char *service;
      int nargs;
      int nret;
      void *args[10];
      prom_args(const char *s, int na, int nr) : service(s), nargs(na), nret(nr) {}
    };

  typedef int (*prom_entry)(struct prom_args *);

  //int prom_call(const char *service, int nargs, int nret, ...) const;
protected:
  enum {
      PROM_ERROR = -1u
  };


  typedef void *ihandle_t;
  typedef void *phandle_t;

  typedef struct
    {
      unsigned long len;
      char          data[];
    } of_item_t;

  /* methods */
  unsigned long prom_call(const char *service, int nargs, int nret, ...) const
    {
      struct prom_args args = prom_args(service, nargs, nret);
      va_list list;

      va_start(list, nret);
      for(int i = 0; i < nargs; i++)
        args.args[i] = va_arg(list, void*);
      va_end(list);

      for(int i = 0; i < nret; i++)
        args.args[nargs + i] = 0;

      if(_prom()(&args) < 0)
        return PROM_ERROR;

      return (nret > 0) ? (unsigned long)args.args[nargs] : 0;
    }


  int prom_getprop(phandle_t node, const char *pname,  void *value, 
      size_t size)
    {
      return prom_call("getprop", 4, 1, node, pname, (unsigned long)value,
          (unsigned long)size);
    }

  int prom_next_node(phandle_t *nodep)
    {
      phandle_t node;

      if ((node = *nodep) != 0
          && (*nodep = (phandle_t)prom_call("child", 1, 1, node)) != 0)
        return 1;
      if ((*nodep = (phandle_t)prom_call("peer", 1, 1, node)) != 0)
        return 1;
      for (;;) {
          if ((node = (phandle_t)prom_call("parent", 1, 1, node)) == 0)
            return 0;
          if ((*nodep = (phandle_t)prom_call("peer", 1, 1, node)) != 0)
            return 1;
      }
    }

  template<typename T>
    static inline bool handle_valid(T p)
      {
        return ((unsigned long)p != 0 && (unsigned long)p != PROM_ERROR);
      }

  static prom_entry _prom(unsigned long prom = 0)
    {
      static prom_entry local_prom;

      if(prom)
        local_prom = reinterpret_cast<prom_entry>(prom);

      return local_prom;
    }

public:
  Of() {};

  static void set_prom(unsigned long prom)
    {
      _prom(prom);
    }

  static unsigned long get_prom()
    {
      return reinterpret_cast<unsigned long>(_prom());
    }

};
}

#endif /* L4_PPC32_OF_H__*/
