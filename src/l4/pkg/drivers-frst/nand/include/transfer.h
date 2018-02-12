#pragma once

struct Buffer
{
    Buffer()
          : addr(0), size(0)
	        {}

      Buffer(char *a, unsigned s)
	    : addr(a), size(s)
	          {}

        char *addr;
	  unsigned size;
};

struct Transfer
{
  enum
    { Max = 4, };

#if 0
  struct Elem
    {
      unsigned char *addr;
      unsigned size;
    };
#endif

  Transfer()
    : num(0), len(0)
    {}

  unsigned num;
  unsigned len;

  void clear()
    {
      num = 0;
      len = 0;
    }

  void add(char *addr, unsigned size)
    {
      if (num == Max)
	return;

      _elems[num].addr = addr;
      _elems[num].size = size;
      len += size;
      num++;
    }

  void add(Buffer buffer)
    {
      add(buffer.addr, buffer.size);
    }

  Buffer &operator [] (int index)
    {
      //assert(index < Max);
      return _elems[index];
    }
  
private:
  Buffer _elems[Max];
};
