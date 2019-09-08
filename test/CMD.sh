#!/bin/bash

#    -xterm -1! libtool --mode=execute gdb \

#    -mca rmaps_base_verbose 100 \
#    -mca rmaps_explicit_verbose 100 \

#    -mca rmaps_explicit_layout [MPI,-,Package,[-,0,-,1]] \

#    -mca rmaps_explicit_layout [MPI,-,Core,[-,0,-,1]] \
#  --host ubuntu-xenial:4 \
#    -mca rmaps_explicit_layout [MPI,-,Core,[-,1,-,3,5]] \

#    -mca rmaps_base_verbose  10 \
#    -mca rmaps_explicit_verbose  100 \

# TJN: Not entirely sure about the 'rmaps_explicit_layout' syntax,
#      but this is close to get started in testing.

orterun \
    -np 2 \
    --report-bindings \
    -mca btl ^openib \
    -mca rmaps_explicit_priority 100 \
    -mca rmaps_explicit_layout [MPI,-,thread,[0,1]] \
    ./simple_test
