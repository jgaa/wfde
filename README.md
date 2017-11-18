
# War FTP Daemon Engine

This is a implementation of the FTP (and HTTP) server protocols, using modern C++.
The primary purpose is to use the library in an upcoming version of the War FTP Daemon,
targeting Unix, Windows and MacOS.

The goal is to have a modern re-implementation of the world famous "War FTP Daemon",
providing hassle-free sharing over FTP, HTTP and SFTP (the ssh file transfer protocol),
using user permissions managed by the daemon (or plugins for common authentication providers),
but totally detached from the operating system. In other words, no Linux or Windows
user account will be required for the persons using the service.

This library will provide basic implementations of the protocols in a generic way,
so the basic building blocks can be shared by other projects as well.

The library includes a simple ftp server to allow functional testing, and to demonstrate how
to use the library.

This is Work in progress.


## Functional Testing:
--------------------------

In order to run the functional tests, you first run basic_tests.py, and then you start
the FTP server with a configuration-file created by the test script.

The first time the tests are run they will create some large files for testing of large file transfers.

Shell session 1
```sh
~/src/wfde$ ./tests/functional/basic_tests.py
Creating missing test-files for download
Ready to start tests on ftp-root: /home/jgaa/src/wfde/test-tmp/ftproot
Start the server with config-path to /home/jgaa/src/wfde/test-tmp/wfded.conf
Press ENTER when ready
```

Shell session 2
```sh
~/src/wfde$ ./build/src/wfded/wfded -c /home/jgaa/src/wfde/test-tmp/wfded.conf
2017-11-18 10:28.098 139793673900288 INFO: wfded 0.21 starting up
2017-11-18 10:28.098 139793673900288 NOTICE: Reading configuration-file: "/home/jgaa/src/wfde/test-tmp/wfded.conf"
2017-11-18 10:28.098 139793673900288 NOTICE: Starting threadpool with 7 threads.
2017-11-18 10:28.101 139793673900288 NOTICE: Adding {Host "Default"} to {Server "Server"}
2017-11-18 10:28.102 139793673900288 NOTICE: Adding {Protocol "FTP"} to {Host "Default"}
2017-11-18 10:28.103 139793673900288 INFO: Starting all services
2017-11-18 10:28.103 139793673900288 NOTICE: Starting the services for {Server "Server"}
2017-11-18 10:28.103 139793673900288 NOTICE: Starting {Host "Default"}
2017-11-18 10:28.103 139793673900288 NOTICE: Starting {Protocol "FTP"}
2017-11-18 10:28.103 139793673900288 NOTICE: Starting {Interface "tcp-local"}, listening on 127.0.0.1:2121
2017-11-18 10:28.103 139793673900288 NOTICE: Done starting the services for {Server "Server"}
```
Now, press ENTER in the first shell session and wait for the tests to finish.

Press ^C in the second session to kill the FTP server when the tests have finished.

When the project is a bit more mature, I will refactor these tests to be easily used cross platform from Continuous Integration systems like Jenkins.
