#!/bin/bash

# clean before deleting old builds
./clean.sh

# build dependencies
sudo pacman -Syy  && sudo pacman -S --needed gcc libcurl-compat cmake yaourt
# dependencies from aur
yaourt -S --noconfirm --needed libcurlpp

# number of cores available
cores=`grep --count ^processor /proc/cpuinfo`

# install libnessh
git clone https://github.com/ElitCoder/nessh
cd nessh/ && ./install_pacman.sh && cd ../

# install sigpack
sudo cp -r dependencies/sigpack/ /usr/include/

# install libarmadillo
git clone -b 8.500.x https://gitlab.com/conradsnicta/armadillo-code
cd armadillo-code/ && cmake . && make -j $cores && sudo make install && cd ../

# clean repo
./clean.sh