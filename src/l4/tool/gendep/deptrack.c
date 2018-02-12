/*
  deptrack.c -- high-level routines for gendepend.

  (c) Han-Wen Nienhuys <hanwen@cs.uu.nl> 1998
  (c) Michael Roitzsch <mroi@os.inf.tu-dresden.de> 2007
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <regex.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "gendep.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

extern int errno;

static FILE * output;
static struct
{
  regex_t *re_array;
  char *invert_array;
  int max, no;
} regexps;

static char * executable_name;
static char * wanted_executable_name;
static char * target;
static char * depfile_name;

/* Special mode: Detection mode. In this mode, we watch for a binary opening
a file specified in the environment variable GENDEP_SOURCE (stored in
target). The binary name is written into $(GENDEP_OUTPUT) (depfile_name)
then. */
static int detection_mode;
static int detection_done;

/* List of dependencies. We first collect all, then write them out. */
static struct strlist{
	const char*name;
	struct strlist *next;
} *dependencies; 

#define STRLEN 1024

/*
  simplistic wordwrap
 */
static void
write_word (char const * word)
{
  static int column;
  int l = strlen (word);
  if (l + column > 75 && column>1)
    {
      fputs ("\\\n\t",output);
      column = 8;
    }

  column += l;
  fputs (word, output);
  fputs (" ", output);
}

static void
xnomem(void *p)
{
  if (!p)
    {
      errno = ENOMEM;
      perror ("libgendep.so");
    }
}

static char *
xstrdup (char *s)
{
  char *c = strdup (s);
  xnomem (c);
  return c;
}

static void *
xrealloc (void *s, int sz)
{
  void *c = realloc (s, sz);
  xnomem (c);
  return c;
}

static void *
xmalloc (int sz)
{
  void *c;
  c = malloc (sz);
  xnomem (c);
  return c;
}

/*!\brief Trim the filename
 *
 * Remove redundant "./" at the beginning of a filename.
 *
 * We are not very minimal about memory usage, because this would require
 * scanning the filename twice, instead of once.
 */
static char* trim (const char*fn){
  char *name;

  /* remove trailing "./" */
  while(fn[0]=='.' && fn[1]=='/') fn+=2;
  name = xmalloc(strlen(fn)+1);
  if(name==0) return 0;

  strcpy(name, fn);
  return name;
}

/*!\brief add a dependency
 *
 * Add a dependency to the list of dependencies for the current target.
 * If the name is already registered, it won't be added.
 */
static void gendep__adddep(const char*name){
  struct strlist *dep;
  char *namecopy;

  if (!(namecopy = trim(name)))
    return;

  /* special detection mode ? */
  if (detection_mode)
    {
      if (!strcmp(namecopy, target))
        detection_done = 1;

      free(namecopy);
      return;
    }

  if (!strcmp(target, name))
    return;

  for(dep=dependencies; dep; dep=dep->next){
    if(!strcmp(dep->name, namecopy)){
	    free(namecopy);
	    return;
	}
  }
  dep = xmalloc(sizeof(struct strlist));
  dep->name = namecopy;
  dep->next = dependencies;
  dependencies = dep;
}

/*!\brief Delete a dependency
 *
 * Delete a dependency from the list of dependencies for the current target.
 * This is used if an already registered file is removed later.
 */
static void gendep__deldep(const char*name){
  struct strlist *dep, *old;
  char *namecopy;

  old = 0;
  if((namecopy = trim(name))==0) return;
  for(dep=dependencies; dep; dep=dep->next){
  	if(!strcmp(dep->name, namecopy)){
	    if(old) old->next = dep->next;
	    else dependencies=dep->next;
	    free(namecopy);
	    free((char*)dep->name);
	    free(dep);
	    return;
	}
	old = dep;
  }
  free(namecopy);
}

static int
gendep_getenv (char **dest, char *what)
{
  char var[STRLEN]="GENDEP_";
  strcat (var, what);
  *dest = getenv (var);

  return !! (*dest);
}

/*
  Do hairy stuff with regexps and environment variables.
 */
static void
setup_regexps (void)
{
  char *regexp_val =0;
  char *end, *start;

  if(gendep_getenv(&target, "SOURCE")){
    /* we have to watch for the binary that opens our file first */
    detection_mode=1;
    gendep_getenv(&depfile_name, "OUTPUT");
    return;
  }

  /* standard mode of operation: catch dependencies */
  if (!gendep_getenv (&wanted_executable_name, "BINARY"))
    return;

  if (strcmp (wanted_executable_name, executable_name))
    {
      /* support two binary names (e.g. for arch-foo-X and X) */
      if (!gendep_getenv (&wanted_executable_name, "BINARY_ALT1")
          || strcmp (wanted_executable_name, executable_name))
	return;
    }

  gendep_getenv(&depfile_name, "DEPFILE");

  if (!gendep_getenv (&target, "TARGET"))
    return;

  if (!gendep_getenv (&regexp_val, executable_name))
    return;

  /*
    let's hope malloc (0) does the sensible thing (not return NULL, that is)
  */
  regexps.re_array = xmalloc (0);
  regexps.invert_array = xmalloc (0);

  regexp_val = xstrdup (regexp_val);
  start = regexp_val;
  end = regexp_val + strlen (regexp_val);

  /*
    Duh.  The strength of C : string handling. (not).   Pull apart the whitespace delimited
    list of regexps, and try to compile them.
    Be really gnuish with dynamic allocation of arrays. 
   */
  do {
    char * end_re = 0;
    int in_out;
    while (*start && *start != '+' && *start != '-')
      start ++;

    in_out= (*start == '+') ;
    start ++;
    end_re = strchr (start, ' ');
    if (end_re)
      *end_re = 0;

    if (*start)
      {
	regex_t regex;
	int result= regcomp (&regex, start, REG_NOSUB);

	if (result)
	  {
	    fprintf (stderr, "libgendep.so: Bad regular expression `%s\'\n", start);
	    goto duh;		/* ugh */
	  }

	if (regexps.no >= regexps.max)
	  {
	    /* max_regexps = max_regexps * 2 + 1; */
	    regexps.max ++;
	    regexps.re_array = xrealloc (regexps.re_array, regexps.max * sizeof (regex_t));
	    regexps.invert_array = xrealloc (regexps.invert_array, regexps.max * sizeof (char));	
	  }

	regexps.re_array[regexps.no] = regex;
	regexps.invert_array[regexps.no++] = in_out;
      }

  duh:
    start = end_re ? end_re + 1 :  end;
  } while (start < end);
}

/*
  Try to get the name of the binary.  Is there a portable way to do this?
 */
static void get_executable_name(void)
{
  char *basename_p;
#ifdef __linux
  const char * const proc_cmdline = "/proc/self/cmdline";
  FILE *cmdline = fopen (proc_cmdline, "r");
  char cmd[STRLEN];
  int i=0;
  int c;
  cmd[STRLEN-1] = 0;

  if (!cmdline)
    {
      fprintf(stderr, "libgendep.o: cannot open %s\n", proc_cmdline);
      exit(-1);
    }

  while ((c = fgetc(cmdline))!=EOF && c && i < STRLEN-1)
    cmd[i++] = c;

  cmd[i++] = 0;
#elif defined(__APPLE__) || defined(__FreeBSD__)
  int mib[3], arglen;
  size_t size;
  char *procargs, *cmd;

  /* allocate process argument space */
  mib[0] = CTL_KERN;
  mib[1] = KERN_ARGMAX;
  size = sizeof(arglen);
  if (sysctl(mib, 2, &arglen, &size, NULL, 0) == -1)
    exit(-1);
  if (!(procargs = xmalloc(arglen)))
    exit(-1);

  /* get a copy of the process argument space */
  mib[0] = CTL_KERN;
#if defined(__APPLE__)
  mib[1] = KERN_PROCARGS2;
  mib[2] = getpid();
  size = (size_t)arglen;
  if (sysctl(mib, 3, procargs, &size, NULL, 0) == -1) {
    free(procargs);
    exit(-1);
  }
#else
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_ARGS;
  mib[3] = getpid();
  size = (size_t)arglen;
  if (sysctl(mib, 4, procargs, &size, NULL, 0) == -1) {
    free(procargs);
    exit(-1);
  }
#endif

  /* jump over the argument count */
  cmd = procargs + sizeof(int);
#else
#  error "Retrieving the executable name is not implemented for your platform."
#endif

  /* ugh.  man 3 basename -> ?  */
  basename_p =  strrchr(cmd, '/');
  if (basename_p)
    basename_p++;
  else
    basename_p = cmd;

  executable_name = xstrdup(basename_p);

#if defined(__APPLE__) || defined(__FreeBSD__)
  free(procargs);
#endif
}

static void initialize (void) __attribute__ ((constructor));

static void initialize (void)
{
  get_executable_name ();
  setup_regexps ();

  if (target && !detection_mode)
    {
      char fn[STRLEN];
      char *slash;

      fn[0] = '\0';
      if(depfile_name){
          strncpy(fn, depfile_name, STRLEN);
      } else {
          slash = strrchr(target, '/');
          /* copy the path */
          strncat (fn, target, min(STRLEN, slash?slash-target+1:0));
          strncat (fn, ".", STRLEN);
          /* copy the name */
          strncat(fn, slash?slash+1:target, STRLEN);
          strncat (fn, ".d", STRLEN);
      }
      fn[STRLEN-1]=0;

      if((output = fopen (fn, "w"))==0){
        fprintf(stderr, "libgendep.so: cannot open %s for writing\n", fn);
        return;
      }
      write_word (target);
      write_word (":");
    }
}

/* Someone is opening a file.  If it is opened for reading, and
  matches the regular expressions, write it to the dep-file.

  Note that we explicitly ignore accesses to /etc/mtab and /proc/...
  as these files are inspected by libc for Linux and tend to change.
 */
void
gendep__register_open (char const *fn, int flags)
{
  if ((detection_mode || output) && !(flags & (O_WRONLY | O_RDWR)))
    {
      int i;

      if (fn == strstr(fn, "/etc/mtab") ||
	  fn == strstr(fn, "/proc/"))
	return;

      for (i = 0; i < regexps.no; i++)
	{
	  int not_matched = regexec (regexps.re_array +i, fn, 0, NULL, 0);

	  if (!(not_matched ^ regexps.invert_array[i]))
	    return;
	}

      gendep__adddep(fn);
    }
}

void
gendep__register_unlink (char const *fn)
{
  gendep__deldep(fn);
}


static void finish (void) __attribute__ ((destructor));

static void finish (void)
{
  if (output)
    {
      struct strlist *dep;

      for(dep=dependencies; dep; dep=dep->next){
        write_word(dep->name);
      }
      fprintf (output, "\n");
      for(dep=dependencies; dep; dep=dep->next){
	fputs(dep->name, output);
        fputs(":\n", output);
      }
      fclose (output);
      return;
    }

  if (detection_mode && detection_done && depfile_name)
    {
      FILE *file;

      /*
       * libgendep is called in a nested fashion (fork+execve in various
       * wrappers), so we only write out the most inner occurence, which
       * means we only write as long as depfile_name is not there
       */
      if (access(depfile_name, W_OK) == -1 && errno == ENOENT)
        {
          file = fopen(depfile_name, "w");
          if (!file)
            return;
          fwrite(executable_name, strlen(executable_name), 1, file);
          fclose(file);
        }
      return;
    }
}
