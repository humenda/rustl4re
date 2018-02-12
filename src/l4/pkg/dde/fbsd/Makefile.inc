# vim: set ft=make:
## configuration switches

# DDE_FBSD_DEFINES += -DDDE_LOCKS_INLINE
# DDE_FBSD_DEFINES += -DWITNESS
# DDE_FBSD_DEFINES += -DINVARIANTS -DINVARIANT_SUPPORT

## config end

DDE_FBSD_INCDIR  += $(addprefix $(PKGDIR_ABS)/fbsd/lib/include/bsd/, src contrib options modified)
DDE_FBSD_INCDIR  += $(addprefix $(PKGDIR_OBJ)/fbsd/lib/include/bsd/, generated contrib modified)

DDE_FBSD_DEFINES += -DDDE_FBSD -D_KERNEL -D__FreeBSD__ -U__linux__ 
DDE_FBSD_CFLAGS  += -fno-merge-constants
DDE_FBSD_CFLAGS  += -fno-builtin -include opt_global.h
DDE_FBSD_ASFLAGS += -DLOCORE  # means "don't declare C stuff"
DDE_FBSD_LIBS    += --whole-archive -ldde-bsd --no-whole-archive -lddekit -lio -lomega0 -llist_alloc

WARNINGS += -Wno-pointer-sign
