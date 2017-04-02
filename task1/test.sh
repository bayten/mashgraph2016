#!/bin/bash

for x in `ls ../pictures`
do
    ./build/bin/align ../pictures/$x ../results/res$x --align;
done
