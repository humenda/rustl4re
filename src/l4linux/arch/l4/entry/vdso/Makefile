include arch/x86/entry/vdso/Makefile

$(shell mkdir -p $(objtree)/arch/x86/entry/vdso)

include arch/l4/Makefile.lib

$(call L4X_FILEREF, ../../../../arch/x86/entry/vdso, \
       vma.o vdso32-setup.o vdso-note.o)
