#!/bin/sh
pidfile="${1:-/tmp/wise_adapter_child.pid}"
read request
sleep 30 &
echo $! > "$pidfile"
sleep 30
