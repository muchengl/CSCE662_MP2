#!/usr/bin/expect 
#!/usr/bin/bash
sleep 1 

sleep 1
sleep 5
spawn bash start.sh --client1
expect eof 

sleep 5
spawn bash start.sh --client1
expect eof 

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