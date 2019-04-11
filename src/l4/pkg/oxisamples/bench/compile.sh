#!/bin/sh
set -e
# use micro benchmarking?
if [ "$1" = "-b" ]; then
    export RSFLAGS='--cfg bench_serialisation'
    export CPPFLAGS='-DBENCH_SERIALISATION'
fi

# recompile l4-rust and l4re-rust
touch $RL/* $RL/*/* && make -C $RL
touch $RLR/* && make -C $RLR

# bench1
cd bench1-primitive
touch */*
make
cd ..

# bench2
cd bench2-string
touch */* && make
cd ..

# bench 3
touch bench1-primitive/*client*/* bench3-cap/* && make -C bench3-cap

################################################################################
# C++

# bench1
cd bench1-primitive_cc
touch * && make
cd ..

# bench2
cd bench2-string_cc
touch * && make
cd ..

# bench 3
touch bench1-primitive_cc/* bench3-cap_cc/* && make -C bench3-cap


