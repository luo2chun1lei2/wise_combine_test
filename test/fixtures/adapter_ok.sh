#!/bin/sh
read request
func=$(printf '%s' "$request" | sed -n 's/.*"function":"\([^"]*\)".*/\1/p')
case "$func" in
  init)
    printf '%s\n' '{"protocol":1,"status":"ok","return":0,"returns":{"handle":"42"},"stdout":"init ok","stderr":""}'
    ;;
  start)
    printf '%s\n' '{"protocol":1,"status":"ok","return":0,"returns":{"status":"ok"},"stdout":"start ok","stderr":""}'
    ;;
  *)
    printf '%s\n' '{"protocol":1,"status":"ok","return":0,"returns":{},"stdout":"ok","stderr":""}'
    ;;
esac
