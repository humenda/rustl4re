" Vim syntax file for Simics command line scripts
" Language:    Simics command line
" Maintainer:  Bjoern Doebel
" Last Change: 2010 Oct 29

if exists("b:current_syntax")
  finish
endif

syn clear
syn case match

setlocal iskeyword+=-
syn keyword simicsStatement       a alias api-search add-data-to-script-pipe and apropos add-directory api-apropos auto-partition-configuration add-module-directory api-help
syn keyword simicsStatement       b bookmark break-cr break-heap break-log bin break break-exception break-io
syn keyword simicsStatement       c cb cba cc cd change-namespace check-cell-partitioning clear-directories clear-recorder cmdline. cmdline_term. cn command-history
syn keyword simicsStatement       command-list configuration-shortest-paths connect connect-central connect-components connect-real-network connect-real-network-bridge connect-real-network-host connect-real-network-napt
syn keyword simicsStatement       connect-real-network-port-in connect-real-network-port-out connect-real-network-router continue continue-cycles continue-seconds copy-connector
syn keyword simicsStatement       copyright cpu-switch-time create-script-barrier create-script-pipe current-namespace current-processor cycle-break cycle-break-absolute
syn keyword simicsStatement       da date dec default-port-forward-target defined delete delete-bookmark devs digit-grouping dirs disable disable-hypersim disable-magic-breakpoint disable-mtprof
syn keyword simicsStatement       disable-multithreading disable-page-sharing disable-real-time-mode disable-reverse-execution disassemble disassemble-settings disconnect disconnect-real-network 
syn keyword simicsStatement       disconnect-real-network-port-in disconnect-real-network-port-out display dstc-disable dstc-enable
syn keyword simicsStatement       echo else enable enable-core2-bugfix enable-hypersim enable-magic-breakpoint enable-mtprof enable-multithreading enable-page-sharing
syn keyword simicsStatement       enable-real-time-mode  enable-reverse-execution env except exec exit expect
syn keyword simicsStatement       foreach
syn keyword simicsStatement       get get-breakpoint-list get-class-list get-component-list get-component-prefix get-error-command
syn keyword simicsStatement       get-error-file get-error-line get-error-message get-object-list get-set-no-inquiry
syn keyword simicsStatement       h help help-search hex hl hypersim-status
syn keyword simicsStatement       ib if ifm ignore in in-list info-breakpoints instantiate-components instruction-fetch-mode
syn keyword simicsStatement       interrupt-script interrupt-script-branch io-stats iostc-disable iostc-enable istc-disable istc-enable
syn keyword simicsStatement       l2p license list-attributes list-bookmarks list-breakpoints list-classes list-components list-directories list-failed-modules list-hap-callbacks list-haps list-hypersim-patterns
syn keyword simicsStatement       list-length list-modules list-namespaces list-objects list-port-forwarding-setup list-preferences list-script-branches list-variables list-vars
syn keyword simicsStatement       load-binary load-file load-module load-persistent-state local log log-level log-setup log-size log-type logical-to-physical lookup-file ls
syn keyword simicsStatement       magic-breakpoint-enabled match-string man max min module-list module-list-failed module-list-refresh move-object
syn keyword simicsStatement       native-path network-helper new-attr-meter new-central-server new-context new-context-switcher new-cpu-mode-tracker
syn keyword simicsStatement       new-etg new-file-cdrom new-freescale-hv-tracker new-gdb-remote new-glink new-hap-meter new-host-cdrom new-hypersim-pattern-matcher
syn keyword simicsStatement       new-linux-process-tracker new-mem-traffic-meter new-ose-process-tracker new-qnx-process-tracker new-realtime new-serial-link new-symtable
syn keyword simicsStatement       new-time-server new-tracer new-usb-disk-from-image new-vxworks-process-tracker new-wdb-remote new-wr-hyper-tracker not
syn keyword simicsStatement       object-exists  oct  or  output-file-start  output-file-stop  output-radix
syn keyword simicsStatement       p pdisable penable peq pid pipe popd pow prefs. pregs print print-event-queue
syn keyword simicsStatement       print-time psel pselect pstatus ptime pushd pwd python python.
syn keyword simicsStatement       q quit
syn keyword simicsStatement       r range rc read-configuration read-reg read-variable readme resolve-file restart-simics rev reverse reverse-cycles reverse-step-instruction
syn keyword simicsStatement       reverse-to revto rexec-> rexec-limit rexec.get-attribute-list rexec.get-interface-list rexec.get-interface-port-list rexec.list-attributes
syn keyword simicsStatement       rexec.list-interfaces rexec.log rexec.log-group rexec.log-level rexec.log-size rexec.log-type rlimit rstepi run run-command-file
syn keyword simicsStatement       run-cycles run-python-file run-seconds
syn keyword simicsStatement       save-component-template save-persistent-state save-preferences sb sba sc script-branch script-pipe-has-data search set set-bookmark
syn keyword simicsStatement       set-component-prefix set-context set-memory-limit set-min-latency set-pattern set-pc set-prefix set-substr set-thread-limit shell si
syn keyword simicsStatement       signed signed16 signed32 signed64 signed8 sim-> sim-break sim-break-absolute sim.get-attribute-list sim.get-interface-list sim.get-interface-port-list
syn keyword simicsStatement       sim.info sim.list-attributes sim.list-interfaces sim.log sim.log-group sim.log-level sim.log-size sim.log-type sim.status skip-to
syn keyword simicsStatement       split-string stc-status step-break step-break-absolute step-cycle step-instruction stepi stop sync-info system-info system-perfmeter system-perfmeter-plot
syn keyword simicsStatement       telnet-frontend trace-breakpoint trace-cr trace-exception trace-hap  trace-io try
syn keyword simicsStatement       unbreak unbreak-cr unbreak-exception unbreak-hap unbreak-io undisplay unload-module unset unstep-instruction
syn keyword simicsStatement       untrace-breakpoint untrace-cr untrace-exception untrace-hap use-old-bool untrace-io
syn keyword simicsStatement       version
syn keyword simicsStatement       wait-for-breakpoint wait-for-hap wait-for-script-pipe wait-for-script-barrier wait-for-variable while win-about win-command-line
syn keyword simicsStatement       win-command-list win-configuration-view win-console win-cpu-registers win-control win-device-registers win-disassembly win-hap-list
syn keyword simicsStatement       win-help win-memory win-memory-browser win-object-browser win-plot win-preferences win-stack-trace win-source-view win-user-plot write-configuration write-reg
syn keyword simicsStatement       x

syn keyword simicsTodo XXX TODO FIXME

syn region String   start=+"+  skip=+\\"+  end=+"+

syn match   simicsComment         /#.*/ contains=simicsTodo

syn match simicsVariable   "$[a-zA-Z_][a-zA-Z0-9_]*"
syn match simicsVariable   "%[a-zA-Z_][a-zA-Z0-9_]*%"

syn match simicsNumber     "\d\+"
syn match simicsNumber     "0x\x\+"

" x86 registers
syn match simicsPlatformRegister "%e\?\(ax\|bx\|cx\|di\|dx\|flags\|si\|bp\|ip\|sp\)"
syn match simicsPlatformRegister "%\(ah\|al\|bh\|bl\|ch\|cl\|dh\|dl\)"
syn match simicsPlatformRegister "%\(cpl\|cr0\|cr2\|cr3\|cr4\)"
syn match simicsPlatformRegister "%[cdefgs]s\(_\(attr\|base\|limit\)\)*"

" XXX: add other platforms

hi def link simicsStatement        Statement
hi def link simicsComment          Comment
hi def link String                 String
hi def link simicsVariable         Identifier
hi def link simicsPlatformRegister Special
hi def link simicsTodo             Todo
hi def link simicsNumber           Number

let b:current_syntax = "simics"
