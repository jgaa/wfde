WFDE is built using cmake. The development is done with clang++ 3.5,
but I also try to keep it working with g++ 4.9.

I will try to port it to Windows with the preview of Visual Studio 2015 in
January 2015.


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

December 2015, jgaa.

