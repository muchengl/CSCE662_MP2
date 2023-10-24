#!/usr/bin/expect
sleep 0.1

sleep 5
sleep 1
spawn ./start.sh --client1

sleep 5
spawn ./start.sh --client1

sleep 5
spawn ./start.sh --client1

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