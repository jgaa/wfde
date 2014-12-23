#!/usr/bin/env python3

# The Abor tests initially triggered several race-conditions and one
# client-side hang. So we need to run this test intensivly to prevent
# hard to reproduce regressions
#
# This test asserts that basic_tests.py have already been run and that the
# server is running with basic_tests.py's generated configuration.


import threading
import basic_tests
from test_file import File

class AsyncAbor(threading.Thread):
    def __init__(self, tc, num):
        threading.Thread.__init__(self)
        self.tc = tc
        self.num = num

    def run(self):
        for ix in range(self.num):
            self.tc.TestAbors()


testcase = basic_tests.FtpTest(basic_tests.config)
testcase.disable_print = True

num_iterations = 1000
num_threads = 8

print('Running ' + str(num_iterations) + \
    ' iterations of ABOR test in each of ' + str(num_threads) + ' threads.')

tasks = []
for t in range(num_threads):
    tasks.append(AsyncAbor(testcase, num_iterations))

for t in tasks:
    t.start()

print('Please wait for the tests to complete...')

for t in tasks:
    t.join()

testcase.ReportResult()

