#!/bin/sh
set -e
# use micro benchmarking
IS_RUST=0
if [ "$1" = "-b" ]; then
    echo -n 'Enabled microbenchmarking'
    export RSFLAGS='--cfg bench_serialisation'
    export CPPFLAGS='-DBENCH_SERIALISATION'
    shift
fi
if [ "${1%_cc}" = "$1" ]; then # rust
    IS_RUST=1
    echo ' for l4rust'
    touch $RL/* $RL/*/* && make --no-print-directory -C $RL
    touch $RLR/* && make --no-print-directory -C $RLR
else
    echo " for C++"
fi

# recompile server
if [ $IS_RUST -eq 1 ]; then
    touch bench1-primitive/server/*
    make --no-print-directory -C bench1-primitive/server
else
    touch bench1-primitive_cc/server.cc
    make --no-print-directory -C bench1-primitive_cc
fi
touch "$1"/*
make --no-print-directory -C "$1"

