#!/bin/sh
case_name="${1:-ok}"
read request
case "$case_name" in
  trailing)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":{},"stdout":""} x'
    ;;
  missing_status)
    printf '%s\n' '{"protocol":1,"returns":{},"stdout":""}'
    ;;
  status_wrong_type)
    printf '%s\n' '{"protocol":1,"status":1,"returns":{},"stdout":""}'
    ;;
  returns_missing)
    printf '%s\n' '{"protocol":1,"status":"ok","stdout":""}'
    ;;
  bad_return_type)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":{},"stdout":"","return":"0"}'
    ;;
  bad_returns)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":[],"stdout":""}'
    ;;
  bad_stdout)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":{}}'
    ;;
  stdout_wrong_type)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":{},"stdout":1}'
    ;;
  protocol_wrong_type)
    printf '%s\n' '{"protocol":"1","status":"ok","returns":{},"stdout":""}'
    ;;
  non_integer)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":{},"stdout":"","return":1.5}'
    ;;
  invalid_literal)
    printf '%s\n' 'tru'
    ;;
  unexpected_char)
    printf '%s\n' 'x'
    ;;
  object_key)
    printf '%s\n' '{1:2}'
    ;;
  missing_colon)
    printf '%s\n' '{"a" 1}'
    ;;
  duplicate_key)
    printf '%s\n' '{"protocol":1,"protocol":2}'
    ;;
  unterminated_object)
    printf '%s\n' '{"protocol":1'
    ;;
  escape)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":{"x":"a\nb"},"stdout":"s\t","return":0}'
    ;;
  unicode)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":{},"stdout":"\u0041","return":0}'
    ;;
  *)
    printf '%s\n' '{"protocol":1,"status":"ok","returns":{},"stdout":"ok","return":0}'
    ;;
esac
