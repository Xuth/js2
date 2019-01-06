##js2

This is a refactoring/rewrite of my sliding block puzzle solver (jim'slide) that I started writing over two decades ago.  My original code was extremely fast but difficult to work on or update.  I am really proud of some aspects of the original code, others... not so much.  The original was written in straight C.  This version is written in C++ with a some usage of C++11 library calls (though there are a few places where the code has been changed only minimally)

A description of project and the previous version of the code can be found at http://xuth.net/jimslide/index.html .  While much of the core architecture of the project has changed, the details about the puzzle representation have stayed much the same.

My priorities for this version are

* to have similar functionality
* be easier to work with
* while being built for modern architectures (multithreaded, assume more RAM, more linear memory accesses)
* and being at least as fast as the original codebase in the single thread case.

look at readme.txt for usage instructions.

If you are already familiar with jimslide, the biggest changes to the usage are that bigmem and smallmem have taken on new meanings and that there are several keywords to in the config file about how many threads/cores to use.  (additionally there are some features that aren't yet supported)  These are all covered in the readme.txt file.