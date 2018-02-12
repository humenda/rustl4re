
#include <string.h>
#include "internal.h"

/*****************************************************************************/
/**
 * \brief   clear screen
 *          
 * This function fills the screen with the current background color
 */
/*****************************************************************************/
void
contxt_clrscr(void)
{
  l4con_pslim_rect_t rect;
  CORBA_Environment env = dice_default_environment;
  int i;
  
  rect.x = 0; 
  rect.y = 0;
  rect.w = BITX(vtc_cols);
  rect.h = BITY(vtc_lines);
  
  con_vc_pslim_fill_call(&vtc_l4id, &rect, bg_color, &env);
  
  /* clear history buffer */
  for(i = 0; i < vtc_lines; i++) 
    {
      if(bob == sb_y)
	break;
      memset(&vtc_scrbuf[sb_y * vtc_cols], ' ', vtc_cols);
      sb_y = OFS_LINES(sb_y-1);
    }

  sb_y = bob = fline = 0;
}

/*****************************************************************************/
/**
 * \brief   set graphic mode
 * 
 * \param   gmode  ... coded graphic mode
 *
 * \return  0 on success (set graphic mode)
 *          
 * empty
 */
/*****************************************************************************/
int 
contxt_set_graphmode(long gmode)
{
  return 0;
}

/*****************************************************************************/
/**
 * \brief   get graphic mode
 * 
 * \return  gmode (graphic mode)
 *          
 * empty
 */
/*****************************************************************************/
int 
contxt_get_graphmode()
{
  return 0;
}

