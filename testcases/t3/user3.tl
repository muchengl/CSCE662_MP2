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

KILL 13

NULL 14

INTER_RUN 15
./tsc -h localhost -k 9000 -u 3

NULL 16

NULL 17

SEND 18
follow 5

NULL 19

SEND 20
list

NULL 21

NULL 22

SEND 23
timeline
p33
p34

NULL 24

NULL 25

NULL 26

NULL 27

NULL 28

NULL 29

NULL 30

NULL 31

NULL 32