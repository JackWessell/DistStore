#!/bin/bash
for i in $(seq 0 50);
do
    ./build/src/client --put ${i} --val "DEFAULT"
done

for i in $(seq 50 200050);
do
    random_bit=$(( $(od -An -N1 -i /dev/urandom) % 2 ))
    if [[ $random_bit -eq 1 ]]; then
        ./build/src/client --put ${i} --val "DEFAULT"
    else
        random_idx=$(( $(od -An -N1 -i /dev/urandom) % 50))
        ./build/src/client --get ${random_idx}
    fi
done