INTER_RUN 1
./tsc -h localhost -k 9000 -u 1

SEND 2
list

SEND 3
follow 4

SEND 4
list

SEND 5
timeline
p11
p12
p13

NULL 6

KILL
