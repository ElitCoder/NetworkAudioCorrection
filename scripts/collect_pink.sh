#!/bin/bash

PINK_NOISE="../Server/data/pink_noise_30s.wav"
arecord -r 48000 -f S16_LE -c 1 -d 34 before.wav &
procar=$1
sleep 2
aplay $PINK_NOISE
wait $procar