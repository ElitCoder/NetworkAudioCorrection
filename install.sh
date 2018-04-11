#!/bin/bash

# build dependencies
sudo apt-get update && sudo apt-get install g++ libcurl4-openssl-dev

# install nessh
git clone https://github.com/ElitCoder/nessh.git
cd nessh/ && ./install.sh && cd ../

# clean repo
./clean.sh