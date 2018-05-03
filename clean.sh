#!/bin/bash

# clean client & server
cd Client/ && make clean && cd ../
cd Server/ && make clean && cd ../

# clean libnessh
rm -rf nessh

# clean libcurlpp
rm -rf curlpp

# clean libarmadillo
rm -rf armadillo-code

# clean matlab
rm -f matlab/*.wav matlab/eqs