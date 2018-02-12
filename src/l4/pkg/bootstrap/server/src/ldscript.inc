/* vi:ft=c
   Common functionality for linker scripts in bootstrap */

#define CTORS                             \
    . = ALIGN(8);                         \
    PROVIDE (__CTORS_BEGIN = .);          \
    KEEP (*(SORT(.ctors.*)))              \
    KEEP (*(.ctors))                      \
    PROVIDE (__CTORS_END = .);            \
    PROVIDE (__preinit_array_start = .);  \
    KEEP (*(SORT(.preinit_array.*)))      \
    KEEP (*(.preinit_array))              \
    PROVIDE (__preinit_array_end = .);    \
    PROVIDE (__init_array_start = .);     \
    KEEP (*(SORT(.init_array.*)))         \
    KEEP (*(.init_array))                 \
    PROVIDE (__init_array_end = .);


#define PLATFORMS                      \
    PROVIDE (__PLATFORMS_BEGIN = .);   \
    KEEP (*(.platformdata))            \
    PROVIDE (__PLATFORMS_END = .);

