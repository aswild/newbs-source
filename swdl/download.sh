#!/bin/bash

url=https://awild.cc/pub/archlinux/package-key.txt

exec 6< <(curl -s $url)

while true; do
    read -u 6 -N 200 data
    [[ -z "$data" ]] && break
    echo -n "$data"
    sleep 1
done
