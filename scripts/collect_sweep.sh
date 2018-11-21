#!/bin/bash

SOUND="../Server/data/sweep_10s.wav"
arecord -r 48000 -f S16_LE -c 1 before.wav &
aplay -r 48000 -f S16_LE -c 2 $SOUND
pkill arecord