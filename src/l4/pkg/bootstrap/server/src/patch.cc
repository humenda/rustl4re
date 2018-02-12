/**
 * \file	bootstrap/server/src/patch.c
 * \brief	Patching of boot modules
 * 
 * \date	09/2005
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */

/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <l4/sys/consts.h>
#include <l4/util/mb_info.h>
#include "panic.h"
#include "macros.h"
#include "types.h"
#include "patch.h"
#include "startup.h"


/* search module in module list */
static l4util_mb_mod_t*
search_module(const char *name, size_t name_len, l4util_mb_info_t *mbi,
              const char **cmdline)
{
  unsigned i;
  const char *c = 0, *ce = 0;
  l4util_mb_mod_t *mod;

  for (i=0; i<mbi->mods_count; i++)
    {
      const char *m, *n;

      mod = (L4_MB_MOD_PTR(mbi->mods_addr)) + i;
      m = c = L4_CONST_CHAR_PTR(mod->cmdline);
      ce = strchr(c, ' ');
      if (!ce)
	ce = c+strlen(c);
      for (;;)
	{
	  if (!(n = strchr(m, name[0])) || n+name_len>ce)
	    break;
	  if (!memcmp(name, n, name_len))
	    {
	      *cmdline = c;
	      return mod;
	    }
	  m = n+1;
	}
    }

  return NULL;
}

/**
 * Handle -patch=<module_name>,<variable>=blah parameter. Overwrite a specific
 * module from command line. This allows to change the boot configuration (e.g.
 * changing parameters of a loader script)
 */
void
patch_module(const char **str, l4util_mb_info_t *mbi)
{
  const char *nam_beg, *nam_end;
  const char *var_beg, *var_end;
  const char *val_beg, *val_end;
  char *mod_beg, *mod_end, *mod_ptr, quote = 0;
  l4_size_t var_size, val_size, max_patch_size;
  const char *cmdline = 0;
  l4util_mb_mod_t *mod;

  /* nam_beg ... nam_end */
  nam_beg = *str+8;
  nam_end = strchr(nam_beg, ',');
  if (!nam_end || strpbrk(nam_beg, "\011 =*")-1 < nam_end)
    panic("-patch: bad module name");

  mod = search_module(nam_beg, nam_end-nam_beg, mbi, &cmdline);
  if (!mod)
    panic("-patch: cannot find module \"%.*s\"",
	  (int)(nam_end-nam_beg), nam_beg);

  mod_beg = L4_CHAR_PTR(mod->mod_start);
  mod_end = L4_CHAR_PTR(mod->mod_end);

  /* How much bytes the module can be enlarged to. The module cannot
   * be extended beyond page boundaries because the next module starts
   * there and we don't want to move the following modules. */
  max_patch_size = l4_round_page((l4_addr_t)mod_end) - (l4_addr_t)mod_end - 1;

  printf("  Patching module \"%s\"\n", cmdline);

  for (var_beg=nam_end; *var_beg==','; var_beg=*str)
    {
      var_beg++;
      /* var_beg ... var_end */
      var_end = strchr(var_beg, '=');
      if (!var_end || strpbrk(var_beg, "\011 ,*")-1 < nam_end)
	panic("-patch: bad variable name");
      var_size = var_end-var_beg;

      /* val_beg ... val_end, consider quotes */
      val_beg = val_end = var_end+1;
      if (*val_end == '"' || *val_end == '\'')
	{
	  val_beg++;
	  quote = *val_end++;
	}
      while (*val_end && ((!quote && !isspace(*val_end) && *val_end!=',') ||
			  (quote && *val_end!=quote)))
	val_end++;
      *str = val_end;
      if (quote)
	(*str)++;
      quote = 0;
      val_size = val_end-val_beg;

      /* replace all occurences of variable with value */
      for (mod_ptr=mod_beg;;)
	{
	  if (!(mod_ptr = (char *)memmem(mod_ptr, mod_end - mod_ptr,
				         var_beg, var_end - var_beg)))
	    break;
	  if (var_size < val_size && max_patch_size < val_size-var_size)
	    panic("-patch: not enough space in module");
	  max_patch_size += var_size - val_size;
	  memmove(mod_ptr+val_size, mod_ptr+var_size, mod_end-mod_ptr-var_size);
	  if (val_size < var_size)
	    memset(mod_end-var_size+val_size, 0, var_size-val_size);
	  memcpy(mod_ptr, val_beg, val_size);
	  mod_ptr += val_size;
	  mod_end += val_size - var_size;
	}
    }

  mod->mod_end = (l4_addr_t)mod_end;
}


/**
 * Handle -arg=<module_name>,blah parameter. Replace old command line
 * parameters of <module_name> by blah. Useful for changing the boot
 * configuration of a bootstrap image.
 *
 * Get a pointer to new argument and return the size.
 */
char *
get_arg_module(char *cmdline, const char *name, unsigned *size)
{
  char *val_beg = NULL, *val_end;
  char *s = cmdline;

  if (!s)
    return 0;

  while (!val_beg && (s = strstr(s, " -arg=")))
    {
      char *a, *name_end;

      s += 6;
      name_end = strchr(s, ',');
      if (!name_end)
	panic("comma missing after modname in -arg=");
      *name_end = 0;

      for (a = s; *a; a++)
	if (isspace(*a))
	  panic("Invalid '-arg=modname,text' parameter");

      // we do a fuzzy name-match here
      if (strstr(name, s))
	val_beg = name_end+1;
      *name_end = ',';
    }
  if (!val_beg)
    return 0;

  // consider quotes
  unsigned char quote = 0;
  if (*val_beg == '"' || *val_beg == '\'')
    quote = *val_beg++;
  val_end = val_beg;

  while (*val_end && ((!quote && !isspace(*val_end)) || *val_end!=quote))
    val_end++;

  *size = val_end - val_beg;
  return val_beg;
}
