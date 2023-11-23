NULL 1

INTER_RUN 2
./tsc -h localhost -k 9000 -u 2

NULL 3

SEND 4
list

SEND 5
follow 1
follow 3

NULL 6

SEND 7
list

NULL 8

SLEEP 9
15

SEND 10
timeline
p21
p22

NULL 11

NULL 12

KILL
