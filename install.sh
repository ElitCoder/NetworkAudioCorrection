#!/bin/bash

# clean before deleting old builds
./clean.sh

# build dependencies
sudo apt-get update && sudo apt-get install g++ libcurl4-openssl-dev cmake

# number of cores available
cores=`grep --count ^processor /proc/cpuinfo`

# install libcurlpp
git clone https://github.com/jpbarrette/curlpp.git
cd curlpp/ && mkdir build/ && cd build/ && cmake .. && make -j $cores && sudo make install && cd ../../

# install libnessh
git clone https://github.com/ElitCoder/nessh.git
cd nessh/ && ./install.sh && cd ../

# install sigpack
sudo cp -r dependencies/sigpack/ /usr/include/

# install libarmadillo
git clone -b 8.500.x https://github.com/conradsnicta/armadillo-code.git
cd armadillo-code/ && cmake . && make -j $cores && sudo make install && cd ../

# clean repo
./clean.sh