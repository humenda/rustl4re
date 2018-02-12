#include "internal.h"

/** contxt_write
 *
 * \param s   output string
 * \param len string length
 *
 * print a string (low-level)
 */
int
contxt_write(const char *s, int len)
{
  l4_uint8_t c, strbuf[vtc_cols];
  int sidx, bidx = 0, x = sb_x;
    
  for(sidx = 0; sidx < len; sidx++) 
    {
      c = s[sidx];
    
      if(c == '\n') 
	{
	  _flush(strbuf, bidx, 1);
	  bidx = 0;
	  x    = 0;
	  continue;
	}
      if(bidx >= vtc_cols || x == vtc_cols)
	{
	  _flush(strbuf, bidx, 1);
	  bidx = 0;
	  x    = 0;
	}
      strbuf[bidx++] = c;
      x++;
    }
  
  if(bidx != 0)
    _flush(strbuf, bidx, 0);

  return len;
}

