#!/bin/sh

in=$1
out=$(basename $in .prv).csv

printf "\"size_bytes\",\"duration_ns\"\n" >${out}
awk -F: '/^3:/ { printf "%s,%s,%s,%s\n", $14, $2, $8, $13 - $6; }' ${in} >>${out}

# In R:
# df = read.csv('durations.csv')
# aggregate(duration_ns ~ size_bytes, data=df, FUN=mean)
