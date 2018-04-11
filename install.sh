#!/bin/bash

# install nessh
git clone https://github.com/ElitCoder/nessh.git
cd nessh/ && ./install.sh && cd ../

# clean repo
./clean.sh