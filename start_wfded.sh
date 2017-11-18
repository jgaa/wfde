#!/bin/sh

# Start wfdedD (debug build) on port 2121 for testing/debugging
# Logging is verbose

./build/src/wfded/wfded -c src/wfded/conf/simple-localhost2121.conf -L TRACE4 -C DEBUG

