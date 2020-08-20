#!/bin/bash

for ehoa in "examples/"*
do
    cat $ehoa | ./hoacheck
    res=$?
    if [ $res -eq 0 ]; then
        echo $ehoa
        cp $ehoa "parity-examples/"
    fi
done
