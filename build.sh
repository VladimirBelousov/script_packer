#!/bin/bash
# Copyright 2013-2014, 2021 Vladimir Belousov (vlad.belos@gmail.com)
# https://github.com/VladimirBelousov/fancy_scripts
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

type sed;[ $? != 0 ]&&echo "Error: a stream editor SED is needed to build from sources"
type gcc;[ $? != 0 ]&&echo "Error: GCC is needed to compile from sources"

# Remove comments and empty strings, escape backslashes and double quotes
# At the beginning of each line insert two spaces and a double quote
# At the end of each line insert a double quote and a comma
wrap=$(sed ' /^[ \t]*\/\//d;/^[ \t]*\/\*/{:a;N;s/^[ \t]*\/\*.*\*\///;Ta}' ./wrap.c | sed ' /^[ \t]*$/d;s/\\/\\\\/g;s/"/\\"/g;s/^/  "/g;s/$/",/g;')
placeholder3=$(sed ' /^[ \t]*\/\//d;/^[ \t]*\/\*/{:a;N;s/^[ \t]*\/\*.*\*\///;Ta}' ./script.c | sed ' /^[ \t]*$/d;s/\\/\\\\/g;s/"/\\"/g;s/^/  "/g;s/$/",/g;')

placeholder1=$(echo -n "$wrap" | sed -n ' /^[ \t\x22]*#placeholder 1 start/{:a;N;s/^[ \t\x22]*#placeholder 1 start.*#placeholder 1 end/&/;Ta;h};${x;p}' | sed ' 1d;$d')
placeholder2=$(echo -n "$wrap" | sed -n ' /^[ \t\x22]*#placeholder 2 start/{:a;N;s/^[ \t\x22]*#placeholder 2 start.*#placeholder 2 end/&/;Ta;h};${x;p}' | sed ' 1d;$d')
placeholder4=$(echo -n "$wrap" | sed -n ' /^[ \t\x22]*#placeholder 4 start/{:a;N;s/^[ \t\x22]*#placeholder 4 start.*#placeholder 4 end/&/;Ta;h};${x;p}' | sed ' 1d;$d')

sed -n ' 1{:a;N;s/#placeholder 1 content/&/;Ta;h};${x;p}' ./frame.c | sed ' $d' > ./src_built/srcpck.c
echo -n "$placeholder1" >> ./src_built/srcpck.c
sed -n ' /#placeholder 1 content/{:a;N;s/#placeholder 2 content/&/;Ta;h};${x;p}' ./frame.c | sed ' 1d;$d' >> ./src_built/srcpck.c
echo -n "$placeholder2" >> ./src_built/srcpck.c
sed -n ' /#placeholder 2 content/{:a;N;s/#placeholder 3 content/&/;Ta;h};${x;p}' ./frame.c | sed ' 1d;$d' >> ./src_built/srcpck.c
echo -n "$placeholder3" >> ./src_built/srcpck.c
sed -n ' /#placeholder 3 content/{:a;N;s/#placeholder 4 content/&/;Ta;h};${x;p}' ./frame.c | sed ' 1d;$d' >> ./src_built/srcpck.c
echo -n "$placeholder4" >> ./src_built/srcpck.c
sed -n ' /#placeholder 4 content/,$p' ./frame.c | sed ' 1d' >> ./src_built/srcpck.c

gcc ./src_built/srcpck.c -o ./dist/srcpck.exe
