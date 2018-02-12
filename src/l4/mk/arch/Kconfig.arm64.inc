# ARCH:       BUILD_ARCH_arm64 arm64
# ARCHDESCR:  ARM64 architecture (AArch64)
# ARCHSELECT: ARCH_ENABLE_STACK_PROTECTOR

choice
	prompt "CPU variant"
	default CPU_ARM_ARMV8A
	
config CPU_ARM_ARMV8A
	bool "ARMv8-A type CPU"
	depends on BUILD_ARCH_arm64

endchoice

config CPU
	string
	default "armv8a"   if CPU_ARM_ARMV8A
