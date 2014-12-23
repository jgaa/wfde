#!/bin/sh

# Start wfdedD (debug build) on port 2121 for testing/debugging
# Logging is verbose

./dbuild/tests/wfded/wfdedD -c tests/wfded/conf/simple-localhost2121.conf -L TRACE4 -C DEBUG

