#!/bin/bash

# clean before deleting old builds
./clean.sh

# build dependencies
sudo pacman -Syy  && sudo pacman -S --needed gcc libcurl-compat cmake

# number of cores available
cores=`grep --count ^processor /proc/cpuinfo`

# install libcurlpp
#git clone https://github.com/jpbarrette/curlpp.git
#cd curlpp/ && mkdir build/ && cd build/ && cmake .. && make -j $cores && sudo make install && cd ../../

# install libnessh
git clone https://github.com/ElitCoder/nessh.git
cd nessh/ && ./install_pacman.sh && cd ../

# install sigpack
sudo cp -r dependencies/sigpack/ /usr/include/

# install libarmadillo
git clone -b 8.500.x https://github.com/conradsnicta/armadillo-code.git
cd armadillo-code/ && cmake . && make -j $cores && sudo make install && cd ../

# clean repo
./clean.sh
