WFDE is built using cmake. 

Supported compilers:
  clang++ 3.5
  g++ 4.9
  Visual Studio 2015 CTP 5 [The latest code is not checked with Windows compilers at the moment]


This project depends on warlib (for logging, error-handling and the thread-pool).

The cmake-file assumes that the directory "../warlib' exists and that it's
libraries are compiled.

Personally, I use Kdevelop with both projects, "warlib" and "wfde", opened.

Warlib is available here:

  The warlib project is here: http://sourceforge.net/projects/warlib/

To clone it:
  git clone git://git.code.sf.net/p/warlib/warlib warlib


Please use the project-page at Sourceforge for bug-reports, questions and
suggestions: http://sourceforge.net/projects/wfde/


Functional Testing:
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

