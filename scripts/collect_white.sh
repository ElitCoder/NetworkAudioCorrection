#!/bin/bash

SOUND="../Server/data/white_noise_30s.wav"
arecord -r 48000 -f S16_LE -c 1 -d 34 before.wav &
procar=$1
sleep 2
aplay -r 48000 -f S16_LE -c 2 $SOUND
wait $procar