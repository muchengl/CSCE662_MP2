SLEEP 1
15

INTER_RUN 2
./tsc -h localhost -k 9000 -u 1

SLEEP 3
15

SEND 4
list

SEND 5
follow 2
follow 3

SLEEP 6
15

SEND 7
list

SEND 8
timeline
p11
p12

NULL 9

NULL 10

NULL 11

NULL 12

SLEEP 13
10000

KILL