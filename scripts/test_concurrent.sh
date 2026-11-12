#!/bin/bash
# Start the server, open 12 simultaneous connections, show 12 child processes alive.
cd "$(dirname "$0")/.."
PORT=${PORT:-9000}
mkdir -p build/logs
./build/server $PORT 2>build/logs/server.log &
SPID=$!
sleep 0.5
for i in $(seq 12); do ./build/client 127.0.0.1 $PORT SLEEP >/dev/null & done
sleep 1.5
echo "children of server (pid $SPID) while clients are connected:"
CH=$(pgrep -P $SPID | wc -l)
pgrep -P $SPID -a
echo "concurrent children = $CH"
wait $(jobs -p | grep -v $SPID) 2>/dev/null
sleep 6
echo "zombies after finish: $(ps -o stat= --ppid $SPID | grep -c Z)"
kill $SPID
[ "$CH" -ge 10 ] && echo PASS || echo FAIL
