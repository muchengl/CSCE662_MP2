#!/usr/bin/expect 
set timeout 60

sleep 7
spawn ./tsc -h localhost -k 3010 -u 2

# waiting for kill server2
# sleep 1

# 1
sleep 1
expect "Cmd>"
send "list\r"

expect "Cmd>"
send "timeline\r"


# 2
sleep 5
expect "Cmd>"
send "list\r"

expect "Cmd>"
send "timeline\r"


# 3
sleep 5
expect "Cmd>"
send "list\n"
sleep 0.1

expect "Cmd>"
send "timeline\n"
sleep 0.1

send "p1\n"
sleep 0.1
send "p2\n"
sleep 0.1
send "p3\n"

interact