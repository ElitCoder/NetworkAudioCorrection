#!/bin/bash

# clean client & server
cd Client/ && make clean && cd ../
cd Server/ && make clean && cd ../

# clean nessh
rm -rf nessh

# clean libcurlpp
rm -rf curlpp

# clean matlab
rm -f matlab/*.wav matlab/eqs