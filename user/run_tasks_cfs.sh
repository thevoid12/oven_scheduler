#!/bin/bash

FILE=$1

if [[ -z "$FILE" ]]; then
    echo "Usage: $0 <job file>"
    exit 1
fi

pids=""

while read line; do
    chrt --other 0 ./fibonacci "$line" &
    pids="$pids $!"
done < "$FILE"

echo $pids
wait $pids