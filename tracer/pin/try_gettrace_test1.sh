#!/bin/sh

$PIN_ROOT/pin -t obj-intel64/champsim_tracer.so -o /home/funkytaiga/ChampSim-Ramulator/tmp_trace/try_print_function_test1.champsim -s 100000 -t 1000000 -- ../../tmp_benchmark/./a.out
xz -f /home/funkytaiga/ChampSim-Ramulator/tmp_trace/try_print_function_test1.champsim