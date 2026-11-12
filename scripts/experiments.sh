#!/bin/bash
# Robustness experiments: each runs WITHOUT (--no-robust) and WITH protection.
# usage: ./scripts/experiments.sh        experiments 1-3 (the ones used in the report)
cd "$(dirname "$0")/.."
mkdir -p build/logs
# random base port: a port used by a previous run may still be in TIME_WAIT for ~60 s
BASE=$((20000 + RANDOM % 20000))
run_server() {
  ./build/server "$@" 2>>build/logs/experiments.log & SPID=$!; sleep 0.4
  kill -0 $SPID 2>/dev/null || { echo "SETUP ERROR: server did not start"; exit 1; }
}

echo "===== Experiment 1: zombie processes (SIGCHLD handler) ====="
P=$((BASE+10))
for mode in "--no-robust" ""; do
  P=$((P+1))           # separate port per mode so runs do not affect each other
  echo "--- server mode: ${mode:-robust} (port $P)"
  run_server $P $mode
  for i in 1 2 3; do ./build/client 127.0.0.1 $P ECHO hi; done
  sleep 0.5
  echo "children state (Z = zombie):"; ps -o pid,stat,cmd --ppid $SPID || true
  echo "zombie count: $(ps -o stat= --ppid $SPID | grep -c Z)"
  kill $SPID; wait $SPID 2>/dev/null
done

echo; echo "===== Experiment 2: client vanishes mid-reply (SIGPIPE) ====="
P=$((BASE+20))
for mode in "--no-robust" ""; do
  P=$((P+1))
  echo "--- server mode: ${mode:-robust} (port $P)"
  : > build/logs/experiments.log
  run_server $P $mode
  # send FLOOD then close immediately, without reading the replies
  python3 - <<PY
import socket
s=socket.create_connection(("127.0.0.1",$P)); s.sendall(b"FLOOD\n"); s.close()
PY
  sleep 1.5
  echo "server log:"; cat build/logs/experiments.log
  if grep -q "child finished normally" build/logs/experiments.log; then
    echo "child result: exited normally, error was logged"
  else
    echo "child result: killed by SIGPIPE (no log line, no normal exit)"
  fi
  kill -0 $SPID 2>/dev/null && echo "parent server still alive: yes"
  kill $SPID; wait $SPID 2>/dev/null
done

echo; echo "===== Experiment 3: quick restart (SO_REUSEADDR) ====="
PORT3=$((BASE+30))
for mode in "--no-robust" ""; do
  PORT3=$((PORT3+1))   # separate port per mode so runs do not affect each other
  echo "--- server mode: ${mode:-robust} (port $PORT3)"
  run_server $PORT3 $mode
  ./build/client 127.0.0.1 $PORT3 ECHO hi >/dev/null
  kill $SPID; wait $SPID 2>/dev/null
  ./build/server $PORT3 $mode 2>&1 & S2=$!; sleep 0.5
  if kill -0 $S2 2>/dev/null; then echo "restart result: OK, server running"
  else echo "restart result: FAILED (see message above)"; fi
  kill $S2 2>/dev/null; wait $S2 2>/dev/null
done

exit 0
