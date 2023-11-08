#!/bin/sh

$PIN_ROOT/pin -t obj-intel64/champsim_tracer.so -o /home/funkytaiga/ChampSim-Ramulator/tmp_trace/ls_trace.champsim -s 100000 -t 200000 -- ls
xz -f /home/funkytaiga/ChampSim-Ramulator/tmp_trace/ls_trace.champsim