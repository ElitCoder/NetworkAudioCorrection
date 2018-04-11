#!/bin/bash

# if there's a parameter, clean the repo first
if [ $# -ne 0 ]; then
	./clean.sh
fi
	
# number of cores available
cores=`grep --count ^processor /proc/cpuinfo`

# build server
cd Server/ && make -j $cores && cd ../

# build client
cd Client/ && make -j $cores && cd ../