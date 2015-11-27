#!/bin/bash

gcc -g -o wand wand.c -I /usr/local/include/ImageMagick-6/ -L /usr/local/lib -lMagickWand-6.Q16 -DMAGICKCORE_QUANTUM_DEPTH=16 -DMAGICKCORE_HDRI_ENABLE=1
