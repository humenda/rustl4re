# ARCH:       BUILD_ARCH_mips mips
# ARCHDESCR:  MIPS architecture
# ARCHSELECT: ARCH_ENABLE_STACK_PROTECTOR

choice
	prompt "CPU variant"
	default CPU_MIPS_32R2

config CPU_MIPS_32R2
	bool "MIPS32 Release 2"

config CPU_MIPS_32R6
	bool "MIPS32 Release 6"

config CPU_MIPS_64R2
	bool "MIPS64 release 2"

config CPU_MIPS_64R6
	bool "MIPS64 release 6"

endchoice

config CPU
	string
	default "32r2"    if CPU_MIPS_32R2
	default "32r6"    if CPU_MIPS_32R6
	default "64r2"    if CPU_MIPS_64R2
	default "64r6"    if CPU_MIPS_64R6

config CPU_ABI
	string
	default "32" if CPU_MIPS_32R2
	default "32" if CPU_MIPS_32R6
	default "64" if CPU_MIPS_64R2
	default "64" if CPU_MIPS_64R6

choice
	prompt "Page Size"
	default PAGE_SIZE_16KB

config PAGE_SIZE_16KB
	bool "16KiB Page size"

config PAGE_SIZE_4KB
	bool "4KiB Page size"

endchoice
