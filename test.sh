#!/usr/bin/env bash


# sudo apt-get install tcl tk expect

if [ $# -eq 0 ]; then
    echo 'Illegal input, you can choose:'
    echo '  [--t1]'
    echo '  [--t2]'
    echo '  [--t3]'
    exit
fi

chmod u+x testcases/*/*.sh 

arg0=$1

if [ "$arg0" = "--t1" ]; then
    echo 'run t1...'
    ./testcases/testcase1/coordinator.sh > testcases/testcase1/coordinator.output &
        ./testcases/testcase1/server.sh > testcases/testcase1/server.output &
        ./testcases/testcase1/client.sh > testcases/testcase1/client.output
fi

if [ "$arg0" = "--t2" ]; then
    echo 'run t2...'
     ./testcases/testcase2/coordinator.sh > testcases/testcase2/coordinator.output &
        ./testcases/testcase2/server.sh > testcases/testcase2/server.output &
        ./testcases/testcase2/client.sh > testcases/testcase2/client.output
fi

if [ "$arg0" = "--t3" ]; then
    echo 'run t3...'
     ./testcases/testcase3/coordinator.sh > testcases/testcase3/output/coordinator.output &
        ./testcases/testcase3/server1.sh > testcases/testcase3/output/server1.output &
        ./testcases/testcase3/client1.sh > testcases/testcase3/output/client1.output &
        ./testcases/testcase3/server2.sh > testcases/testcase3/output/server2.output &
        ./testcases/testcase3/client2.sh > testcases/testcase3/output/client2.output
fi

if [ "$arg0" = "--kill" ]; then
    pid=$(lsof -t -i:9000)
    kill -9 $pid

    pid=$(lsof -t -i:9001)
    kill -9 $pid

    pid=$(lsof -t -i:3010)
    kill -9 $pid
fi