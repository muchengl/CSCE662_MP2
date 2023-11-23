INTER_RUN 1
./tsc -h localhost -k 9000 -u 4

SEND 2
list

SEND 3
follow 1

SEND 4
list

SEND 5
timeline
p41
p42

SLEEP 6
100

KILL
