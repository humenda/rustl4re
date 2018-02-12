#!/bin/bash

nothreads=$(nm $1 | grep pthread_mutex_lock | grep ' w pthread');
havekip=$(nm $1 | grep libc_backend_rt_clock_gettime)

echo "# Auto-generated for '$1'"
echo ""
echo "[general]"
echo "   page_fault_handling = rw"
if [ -z "$nothreads" ]; then
	echo "  threads             = yes"
fi
if [ "$havekip" ]; then
	echo "  intercept_kip       = yes"
fi
echo "#  redundancy          = none"
echo "#  redundancy          = dual"
echo "#  redundancy          = triple"
echo ""
echo "#  logbuf               = 16"
echo "#  logreplica           = true"
echo "#  logrdtsc             = true"
echo "#  logtimeout           = 15"
echo ""
echo "#  print_vcpu_state    = y"
echo "#  log                 = all"
echo ""

echo "#[watchdog]"
echo "#  enable 				= y"
echo "#	 timeout 				= 1000000"
echo "#	 singlestepping = n"
echo ""

if [ -z "$nothreads" ]; then
	echo "[threads]"
	function_list="__pthread_lock __pthread_unlock pthread_mutex_lock pthread_mutex_unlock"
	for f in $function_list; do
		nm $1 | grep -E "\ $f" | sed -re "s/([0-9a-f]+) [TW] ($f(_rep)?)/  \2 = 0x\1/" | sed -re "s/[_]*pthread_//g"
	done
	echo ""
fi

if [ "$havekip" ]; then
	echo "[kip-time]"
	for v in libc_backend_rt_clock_gettime mono_clock_gettime; do
	echo -n "  "; nm $1 | grep $v |  sed -re 's/([0-9a-f]+) [wWtT] (.*)/\2 = 0x\1/g';
	done
fi
