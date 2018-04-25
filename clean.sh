#!/bin/bash

# clean client & server
cd Client/ && make clean && cd ../
cd Server/ && make clean && cd ../

# clean nessh
rm -rf nessh

# clean matlab
rm -f matlab/*.wav