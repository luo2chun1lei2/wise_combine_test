#!/bin/sh
set -eu

make build/test_api
./build/test_api
