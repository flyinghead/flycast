NoWide
======

|  Linux/GCC  |  Windows/MSVC  |  Binaries  |
| :---------: | :------------: | :--------: |
|  [Travis CI](https://travis-ci.org/Nephatrine/nowide-standalone)  |  [Appveyor CI](https://ci.appveyor.com/project/Nephatrine/nowide-standalone)  |  [MSVC 2013 32-Bit/64-Bit](https://ci.appveyor.com/project/Nephatrine/nowide-standalone/build/artifacts)  |
|  ![Build Status](https://travis-ci.org/Nephatrine/nowide-standalone.svg?branch=master)  |  ![Build Status](https://ci.appveyor.com/api/projects/status/github/nephatrine/nowide-standalone?branch=master)  |  ---  |

NoWide is a library originally implemented by Artyom Beilis that makes
cross-platform, Unicode-aware programming easier.

The library provides an implementation of standard C and C++ library functions,
such that their inputs are UTF-8--aware on Windows without requiring to use Wide
API.

------------------------------------------------------

Rationale
---------

### The Problem ###

Consider a simple application that splits a big file into chunks, such that
they can be sent by email. It requires doing a few very simple tasks:

  * Access Command-Line Arguments
  * Open Input File
  * Open Output Files
  * Possibly Remove Output Files During Rollback
  * Print Progress Report In Console

Unfortunately, it is **impossible** to implement this task in simple, standard
C++. **Why?** Well, what happens when the filename being used in those
operations contains non-ASCII characters?

On modern POSIX systems (Linux, Mac OSX, Solaris, BSD), filenames are
internally encoded in UTF-8. On such systems, the program reads the UTF-8
filenames from ```argv[]``` and simply pass them verbatim to the needed classes
and functions (```std::fstream```, ```std::remove```, ```std::cout```, etc.).

Windows, though, is not so simple. Windows uses UTF-16 internally. UTF-16
cannot fit into a simple ```char```. This means a Unicode filename simply
cannot be passed via the normal ```argv[]``` and such files cannot be
opened or manipulated via the standard C and C++ APIs. Instead, the
Microsoft-specific APIs and extensions would need to be used to handle such a
program.

Normally, you'd need to write any code dealing with filenames twice: once for
Windows and then again for all other platforms. This makes writing portable
code a challenge even for such simple programs.
  
### The Solution ###

NoWide implements drop-in replacement functions for various C and C++ standard
library functions in the ```nowide``` namespace rather than ```std```. On
Windows, these functions will translate between UTF-8 and UTF-16 where needed
and present a solely UTF-8 interface for you to program against that will work
anywhere. On other platforms, the functions are simply aliases to the
corresponding standard library function.

The library provides:

  * Easy to use functions for converting between UTF-8 and UTF-16.
  * A helper class to access UTF-8 ```argc```, ```argc``` and ```env```.
  * UTF-8--Aware Implementations:
    * ```<cstdio> Functions:```
      * ```fopen```
      * ```freopen```
      * ```remove```
      * ```rename```
    * ```<cstdlib> Functions:```
      * ```system```
      * ```getenv```
      * ```setenv```
      * ```unsetenv```
      * ```putenv```
    * ```<fstream> Functions:```
      * ```filebuf```
      * ```fstream```
	  * ```ofstream```
	  * ```ifstream```
    * ```<iostream> Functions:```
      * ```cout```
      * ```cerr```
      * ```clog```
      * ```cin```

### Why not use a wide API everywhere? ###

The trouble is ```wchar_t``` **isn't portable**. It could be 1, 2, or 4 bytes
and there is no specific encoding it should be in. Additionally, the standard
library only provides narrow functions when dealing with the OS (e.g. there is
no ```fopen(wchar_t)``` in the standard). We determined it would be better to
try and stick closely to the C and C++ standards rather than implement wide
function variants everywhere as Microsoft does.

For further reading, see [UTF-8 Everywhere](http://www.utf8everywhere.org/).

------------------------------------------------------

Usage
-----

**IMPORTANT:** If you are using MSVC and a dynamic/shared build of NoWide, you
will need to define the ```NOWIDE_DLL``` symbol prior to including the NoWide
headers so the functions are decorated with ```__declspec(dllimport)``` as
needed. This is not required if using a static library or MinGW/GCC.

To use the library, you need to do to include the ```<nowide/*>``` headers
instead of the standard ones and then call the functions using the ```nowide```
namespace instead of ```std```.

For example, this is a naïve file line counter that cannot handle Unicode:

```cpp
#include <fstream>
#include <iostream>

int main(int argc,char **argv)
{
    if(argc!=2) {
        std::cerr << "Usage: file_name" << std::endl;
        return 1;
    }

    std::ifstream f(argv[1]);
    if(!f) {
        std::cerr << "Can't open a file " << argv[1] << std::endl;
        return 1;
    }
    int total_lines = 0;
    while(f) {
        if(f.get() == '\n')
            total_lines++;
    }
    f.close();
    std::cout << "File " << argv[1] << " has " << total_lines << " lines"
	        << std::endl;
    return 0;
}
```

To make this program handle Unicode properly we make the following changes:

```cpp
#include <nowide/args.hpp>
#include <nowide/fstream.hpp>
#include <nowide/iostream.hpp>

int main(int argc,char **argv)
{
    nowide::args a(argc,argv); // UTF-8
    if(argc!=2) {
        nowide::cerr << "Usage: file_name" << std::endl; // UTF-8
        return 1;
    }

    nowide::ifstream f(argv[1]); // UTF-8
    if(!f) {
        nowide::cerr << "Can't open a file " << argv[1] << std::endl; // UTF-8
        return 1;
    }
    int total_lines = 0;
    while(f) {
        if(f.get() == '\n')
            total_lines++;
    }
    f.close();
    nowide::cout << "File " << argv[1] << " has " << total_lines << " lines"
	        << std::endl; // UTF-8
    return 0;
}
```

This simple and straightforward approach helps writing Unicode-aware programs.

### Interacting With Wide APIs ###

Of course, the above cannot cover every use-case. There may be a Wide API that
you need to work with at some point -- either a Microsoft API or a custom
external one. When dealing with such APIs, use the ```nowide::widen``` and ```nowide::narrow```
functions to convert to/from UTF-8 at the point of use.

For Example:

```cpp
CopyFileW( nowide::widen(existing_file).c_str(),
           nowide::widen(new_file).c_str(),
           TRUE);
```

These functions allocate normal ```std::string```s, but you may want to
allocate the string on the stack for particularly short strings. To do this,
the ```nowide::basic_stackstring``` class can be used.

```cpp
nowide::basic_stackstring<wchar_t,char,64> wexisting_file, wnew_file;
if(!wexisting_file.convert(existing_file) || !wnew_file.convert(new_file))
    return -1;     // invalid UTF-8
CopyFileW(wexisting_file.c_str(), wnew_file.c_str(), TRUE);
```

The following ```typedef```s are also provided for convenience:

  * ```stackstring```: narrows ```wchar_t``` to ```char```; holds 256 characters.
  * ```wstackstring```: widens ```char_t``` to ```wchar```; holds 256 characters.
  * ```short_stackstring```: narrows ```wchar_t``` to ```char```; holds 16 characters.
  * ```wshort_stackstring```: widens ```char_t``` to ```wchar```; holds 16 characters.

These types will fall back to heap-based allocation if the string does not fit
into the specified stack space.

### <windows.h> ###

The library does not include ```<windows.h>``` in order to prevent namespace
pollution. The library rather defines the prototypes to the needed Win32 API
functions.

You may request to use the actual ```<windows.h>``` anyways by setting defining
the ```NOWIDE_USE_WINDOWS_H``` symbol before including any NoWide headers.

------------------------------------------------------

Building Source
---------------

You will need a standard build environment for your platform (i.e. GCC,
Xcode/Clang, MinGW, MSVC, etc.) as well as the following tools:

* CMake 2.8+
* Doxygen (*Optional*; For Documentation)
  * GraphViz/Dot (Class Diagrams)
  * HTML Help Workshop (CHM Documentation)
  * PDFLaTeX (PDF Documentation)
  
Compilation steps are bog-standard for a CMake project:

	mkdir build
	cd build
	cmake ..
	make && make test

Optionally, to install:
	
	make install
