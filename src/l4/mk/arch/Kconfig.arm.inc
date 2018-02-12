# ARCH:       BUILD_ARCH_arm arm
# ARCHDESCR:  ARM architecture
# ARCHSELECT: ARCH_ENABLE_STACK_PROTECTOR

choice
	prompt "CPU variant"
	default CPU_ARM_ARMV7A
	
config CPU_ARM_ARMV4
	bool "ARMv4 type CPU"
	depends on BUILD_ARCH_arm

config CPU_ARM_ARMV4T
	bool "ARMv4T type CPU"
	depends on BUILD_ARCH_arm

config CPU_ARM_ARMV5
	bool "ARMv5 type CPU"
	depends on BUILD_ARCH_arm

config CPU_ARM_ARMV5T
	bool "ARMv5T type CPU"
	depends on BUILD_ARCH_arm

config CPU_ARM_ARMV5TE
	bool "ARMv5TE type CPU"
	depends on BUILD_ARCH_arm

config CPU_ARM_ARMV6
	bool "ARMv6 type CPU"
	depends on BUILD_ARCH_arm

config CPU_ARM_ARMV6T2
	bool "ARMv6t2 type CPU"
	depends on BUILD_ARCH_arm

config CPU_ARM_ARMV6ZK
	bool "ARMv6zk type CPU"
	depends on BUILD_ARCH_arm

config CPU_ARM_ARMV7A
	bool "ARMv7A type CPU"
	depends on BUILD_ARCH_arm

endchoice

config CPU
	string
	default "armv4"    if CPU_ARM_ARMV4
	default "armv4t"   if CPU_ARM_ARMV4T
	default "armv5"    if CPU_ARM_ARMV5
	default "armv5t"   if CPU_ARM_ARMV5T
	default "armv5te"  if CPU_ARM_ARMV5TE
	default "armv6"    if CPU_ARM_ARMV6
	default "armv6t2"  if CPU_ARM_ARMV6T2
	default "armv6zk"  if CPU_ARM_ARMV6ZK
	default "armv7a"   if CPU_ARM_ARMV7A

config CPU_ARMV6KPLUS
	bool
	default y if CPU_ARM_ARMV6ZK || CPU_ARM_ARMV7A || CPU_ARM_ARMV7R

config CPU_ARMV6PLUS
	bool
	default y if CPU_ARM_ARMV6 || CPU_ARM_ARMV6T2 || CPU_ARMV6KPLUS


