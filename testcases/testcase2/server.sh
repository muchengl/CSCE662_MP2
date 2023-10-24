#!/usr/bin/bash

sleep 0.1
bash start.sh --server1


sleep 5
echo 'kill server'
pid=$(lsof -t -i:9000)
if [ -z "$pid" ]; then
    echo "No process uses 000"
else
    kill -9 $pid
    echo "killed"
fi


sleep 6.1
bash start.sh --server1