#!/bin/sh
# This one-liner sends the passed argument (assumably a file link) to hydranode shell,
# issuing the command "download $1". Requires "netcat" utility to work.
#
# This can be used to associate file links in browser with hydranode, by passing the urls
# to this script.

echo -e do $1 \\r\\n | nc localhost 9999 -q 1
