NULL 1

INTER_RUN 2
./tsc -h localhost -k 9000 -u 3

NULL 3

SEND 4
list

SEND 5
follow 1
follow 2

NULL 6

SEND 7
list

NULL 8

NULL 9

NULL 10

SLEEP 11
15

SEND 12
timeline
p31
p32

KILL