#!/bin/bash

[ $# -lt 1 ]&&echo "Specify, please, some arguments"&&exit 1
echo "Hello World !"
echo "$*"
echo "$@"
f=$(sed -n ' p' ./example.sh)
echo "$f"
