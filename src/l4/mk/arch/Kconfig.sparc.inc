# ARCH:      BUILD_ARCH_sparc sparc
# ARCHDESCR: Sparc architecture (EXPERIMENTAL)

choice
	prompt "CPU"
   
	config CPU_SPARC_V7
	bool "Generic Sparc V7"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_LEON
	bool "LEON"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_LEON3
	bool "LEON3"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_V8
	bool "Generic Sparc V8"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_V9
	bool "Generic Sparc V9"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_ULTRASPARC
	bool "Ultrasparc"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_ULTRASPARC3
	bool "Ultrasparc3"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_NIAGARA
	bool "Niagara"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_NIAGARA2
	bool "Niagara2"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_NIAGARA3
	bool "Niagara3"
	depends on BUILD_ARCH_sparc

	config CPU_SPARC_NIAGARA4
	bool "Niagara4"
	depends on BUILD_ARCH_sparc
endchoice

config CPU
	string
	default "leon"        if CPU_SPARC_LEON
	default "leon3"       if CPU_SPARC_LEON3
	default "v7"          if CPU_SPARC_V7
	default "v8"          if CPU_SPARC_V8
	default "v9"          if CPU_SPARC_V9
	default "ultrasparc"  if CPU_SPARC_ULTRASPARC
	default "ultrasparc3" if CPU_SPARC_ULTRASPARC3
	default "niagara"     if CPU_SPARC_NIAGARA
	default "niagara2"    if CPU_SPARC_NIAGARA2
	default "niagara3"    if CPU_SPARC_NIAGARA3
	default "niagara4"    if CPU_SPARC_NIAGARA4

