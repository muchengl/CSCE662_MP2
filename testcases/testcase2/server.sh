#!/usr/bin/bash

sleep 1
bash ./start.sh --server1 &

sleep 5
echo 'kill server'
pid=$(lsof -t -i:9000)
kill -9 $pid

sleep 7
bash ./start.sh --server1

# exit