#!/bin/bash

DATE=$1

cp ../save/white_noises/before/$DATE/*.wav before.wav && \
cp ../save/white_noises/after/$DATE/*.wav after.wav && \
cp ../save/white_noises/after/$DATE/eqs .
