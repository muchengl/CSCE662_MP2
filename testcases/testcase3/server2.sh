#!/usr/bin/bash

sleep 1
./tsd -c 2 -s 2 -h 127.0.0.1 -k 3010 -p 9001

sleep 7
echo 'kill server2'
pid=$(lsof -t -i:9001)
killall tsd

sleep 7
./tsd -c 2 -s 2 -h 127.0.0.1 -k 3010 -p 9001