#!/usr/bin/env bash
if [ $# -eq 0 ]; then
    echo 'Illegal input, you can choose:'
    echo '  [--coordinator] launch a coordinator'
    echo '  [--cluster1]'
    echo '  [--cluster2]'
    echo '  [--cluster3]'
    echo '  [--synchronizer]'
    echo '  [--client1] launch a client with username 1'
    echo '  [--client2] launch a client with username 2'
    echo '  [--client3] launch a client with username 3'
    echo '  [--client5] launch a client with username 5'
    exit
fi

arg0=$1

if [ "$arg0" = "--coordinator" ]; then
    ./coordinator -p 9000
fi

if [ "$arg0" = "--cluster1" ]; then
    ./tsd -c 1 -s 1 -h localhost -k 9000 -p 10000 &
    ./tsd -c 1 -s 2 -h localhost -k 9000 -p 10001
fi

if [ "$arg0" = "--cluster2" ]; then
    ./tsd -c 2 -s 1 -h localhost -k 9000 -p 20000 &
    ./tsd -c 2 -s 2 -h localhost -k 9000 -p 20001
fi

if [ "$arg0" = "--cluster3" ]; then
    ./tsd -c 3 -s 1 -h localhost -k 9000 -p 30000 &
    ./tsd -c 3 -s 2 -h localhost -k 9000 -p 30001
fi

if [ "$arg0" = "--synchronizer" ]; then
    ./synchronizer -h localhost -k 9000 -p 9001 -i 1 &
    ./synchronizer -h localhost -k 9000 -p 9002 -i 2 &
    ./synchronizer -h localhost -k 9000 -p 9003 -i 3
fi


if [ "$arg0" = "--client1" ]; then
    ./tsc -h localhost -k 9000 -u 1
fi

if [ "$arg0" = "--client2" ]; then
    ./tsc -h localhost -k 9000 -u 2
fi

if [ "$arg0" = "--client3" ]; then
    ./tsc -h localhost -k 9000 -u 3
fi

if [ "$arg0" = "--client5" ]; then
    ./tsc -h localhost -k 9000 -u 5
fi