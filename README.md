# TCP-Sockets
Client and server on UNIX streaming sockets TCP.

Fixing the current state (still in an unfinished form)...

=========================================================

cd /home/...

gcc client.c common.c -o client
gcc server.c common.c -o server

./server &
./client 127.0.0.1 12345

ps -ef | grep server
kill -SIGTERM 1923

// ps aux | grep server
// sudo netstat -tunpl

ipcs -s
ipcrm -s ID

ipcs -m
ipcrm -m ID
