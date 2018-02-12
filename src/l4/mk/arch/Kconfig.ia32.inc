# ARCH:       BUILD_ARCH_x86 x86
# ARCHDESCR:  X86-32 architecture
# ARCHSELECT: ARCH_ENABLE_STACK_PROTECTOR

# ARCH:       BUILD_ARCH_amd64 amd64
# ARCHDESCR:  AMD64 architecture
# ARCHSELECT: ARCH_ENABLE_STACK_PROTECTOR

config CPU
	string "CPU variant identifier"
	default "gen"
