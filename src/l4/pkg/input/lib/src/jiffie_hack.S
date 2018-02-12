
.global jiffies
#ifdef ARCH_amd64
jiffies = 0x6ffff000 + 0xa0
#else
jiffies = 0xaffff000 + 0xa0
#endif
