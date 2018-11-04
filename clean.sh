#!/bin/bash

# clean client & server
cd Client/ && make clean && cd ../
cd Server/ && make clean && cd ../

# clean test files
rm -f Server/*.wav Server/eqs

# clean libnessh
rm -rf nessh

# clean libcurlpp
rm -rf curlpp

# clean libarmadillo
rm -rf armadillo-code

# clean matlab
rm -f matlab/*.wav matlab/eqs matlab/gain

# clean saved files
rm -rf save/white_noises/after/*
rm -rf save/white_noises/before/*