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

KILL 13

NULL 14

INTER_RUN 15
./tsc -h localhost -k 9000 -u 2

NULL 16

NULL 17

SEND 18
follow 5

NULL 19

SEND 20
list

NULL 21

NULL 22

NULL 23

NULL 24

SEND 25
timeline
p23
p24

NULL 26

NULL 27

NULL 28

NULL 29

NULL 30

NULL 31

NULL 32