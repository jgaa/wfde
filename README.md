
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

2017-10-19: The build of this project as a stand-alone library is currently broken.


## Functional Testing:
--------------------------

In order to run the functional tests, you first run basic_tests.py, and then you start
the FTP server with a configuration-file created by the test script.

Example:
[In a shell session]:
~/src/wfde$ ./tests/functional/basic_tests.py
Creating path: /home/jgaa/src/wfde/test-tmp/ftproot
Creating path: /home/jgaa/src/wfde/test-tmp/client
Creating path: /home/jgaa/src/wfde/test-tmp/ftproot/home/jgaa
Creating path: /home/jgaa/src/wfde/test-tmp/ftproot/upload
Creating path: /home/jgaa/src/wfde/test-tmp/ftproot/pub/sub/sub2
Creating path: /home/jgaa/src/wfde/test-tmp/ftproot/empty
Creating missing test-files for download
This may take a few minutes...
Ready to start tests on ftp-root: /home/jgaa/src/wfde/test-tmp/ftproot
Start the server with config-path to /home/jgaa/src/wfde/test-tmp/wfded.conf
Press ENTER when ready
...

[In another shell session]:
~/src/wfde$ ./dbuild/tests/wfded/wfdedD -c /home/jgaa/src/wfde/test-tmp/wfded.conf
...

Now, press ENTER in the first shell session and wait for the tests to finish.

Press ^C in the second session to kill the FTP server when the tests have finished.

