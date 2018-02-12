
#define DUMMY(name) \
int name () \
{ \
printf("%s unimplemented\n", __FUNCTION__); \
return -1; \
}

#define DUMMY_SILENT(name) \
int name () \
{ \
return -1; \
}


// dummys for python
DUMMY_SILENT(flockfile)
DUMMY_SILENT(funlockfile)
DUMMY_SILENT(__libc_current_sigrtmin)
DUMMY_SILENT(__libc_current_sigrtmax)

DUMMY(ttyname)
