# ADEF

![Build](https://github.com/ukjhsa/adef/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=develop)
[![API Documentation](https://img.shields.io/badge/docs-doxygen-blue.svg)](http://ukjhsa.github.io/adef)
[![Documentation](https://img.shields.io/badge/doc-Markdown-blue.svg)](doc/documentation.md)
[![Releases](https://img.shields.io/github/release/ukjhsa/adef.svg)](https://github.com/ukjhsa/adef/releases)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Issues](https://img.shields.io/github/issues/ukjhsa/adef.svg)](https://github.com/ukjhsa/adef/issues)

## Description
ADEF (A Software Framework for Adaptive Differential Evolution) is a
framework supports a variety of adaptive differential evolution algorithms.

[Differential Evolution](https://en.wikipedia.org/wiki/Differential_evolution) (DE) is one of [Evolutionary algorithm](https://en.wikipedia.org/wiki/Evolutionary_algorithm). DE uses the differences of solutions to create new solutions and keeps each new solution if it is better.

The DE algorithm has parameters F (scaling factor) and CR (crossover rate) and there are many researches on how to adjust. ADEF not only implements some existing mechanism of parameter control, it also presents the extensible architecture.

## Features
ADEF has the following features:
- the procedure of adaptive DE
- an extensible mechanism of the parameter control.
- the usage of configuration by the file rather than recompiling, including parameters and implementation classes.
- the statistics of experimental result, such as the number of successful run, success performance, and etc.

Algorithms
- DE
    - variant *x*/*y*/*z*:
        - *x*: *rand*
        - *y*: positive integer
        - *z*: *bin*
- procedure of adaptive DE
    - **DEPD**, **NSDE**, **SaDE**, **SaNSDE**, **jDE**, **ISADE**, **SDE**

Problems
- [Classical problems](include/problem/basic)

Variable Representations
- real encoding

## Requirements
- [CMake](https://cmake.org/) 3.15 or the newer.
- C++ compiler with C++17 supported.

Tested environments:
- Windows
    - MSVC 2017 (19.16.27024)
- Ubuntu
    - g++ 7.4.0
    - clang++ 7.0.1
- Mac OS X
	- Xcode 10.2 beta 3

## Build
### Use the terminal
1. create the directory for building, suppose it is named `build`.
1. change the current working directory to it.
1. configure and generate generator from the root CMakeLists.txt.
1. compile it.

```sh
mkdir build
cd build
cmake ..
make
```

you can specify the generator such as
- type `cmake -G "MinGW Makefiles" ..` to use Makefiles with MinGW compiler.
- type `cmake -G "CodeBlocks - MinGW Makefiles" ..` to use [Code::Bolcks](http://www.codeblocks.org/) with MinGW compiler.

then compile from the generator.

### Use the gui tool `cmake-gui`
1. open `cmake-gui` and specify "Where is the source code" to the path of source code, suppose `X:/adef`.
1. specify the path of "Where to build the binaries" to build, suppose `X:/adef/build`.
1. click the button "Configure".
1. select the generator. If the default compilers fails to find, specify them.
    - "MinGW Makefiles" for Windows user. The compiler should be mingw32-g++.
    - "Unix Makefiles" for Unix user.
    - "CodeBlocks - MinGW Makefiles" if you want to use [Code::Bolcks](http://www.codeblocks.org/) with MinGW compiler to build.
1. if you change CMake options, click the button "Configure" again.
1. click button "Generate".
1. compile from the generator
    - "MinGW Makefiles"
        1. open the `cmd` and set up environment with MinGW. (use the "MinGW Command Prompt" if TDM-GCC installed.)
        1. change the current working directory to `X:/adef/build`.
        1. type `mingw32-make`.
    - "Unix Makefiles"
        1. open the terminal and change the current working directory to `X:/adef/build`.
        1. type `make`
    - "CodeBlocks - MinGW Makefiles"
        1. open `adef.cbp` by Code::Bolcks and click build.
    - or other specified generator to build.


### CMake Options
#### BUILD_SHARED_LIBS
Build ADEF as the shared library. Default is OFF (i.e., static)
```
-DBUILD_SHARED_LIBS=OFF
```

#### CMAKE_BUILD_TYPE
The build type. Default is Release.
```
-DCMAKE_BUILD_TYPE="Release"
```

#### GENERATE_EXECUTABLE
Generate the executable file. Default is ON. If set to OFF, then it only build the library.
```
-DGENERATE_EXECUTABLE=ON
```

## Usage
### Executable file
If you don't change option of GENERATE_EXECUTABLE (i.e., default) and suppose the executable file named `adef`
#### Default configuration file
```
./adef
```
to use the configuration file `config.json` on the same path of the executable file.

#### Custom configuration file
Specify the configuration file by adding arguments `<-file> <file_name>`.
```
./adef -file example/jDE.json
```
to use configuration file on the path `example/jDE.json`.

### Library file
If you set GENERATE_EXECUTABLE to OFF, you must set the include and library path manually and specify the main code, for example,

```cpp
#include "adef.h"
int main(int argc, char *argv[])
{
    auto system = adef::init_adef(argc, argv);
    system->run();
}
```

See also the [src/main.cpp](src/main.cpp).

## Output

For every single run of DE (e.g. jDE) to solve a certain function (e.g. Sphere), output the following data into five separate files:
- jDE_Sphere_run1_best_fitness.csv: best fitness value
- jDE_Sphere_run1_F.csv: F values used in mutation
- jDE_Sphere_run1_CR.csv: CR values used in crossover
- jDE_Sphere_run1_Parents.csv: fitness values of parents (target vectors)
- jDE_Sphere_run1_Children.csv: fitness values of offspring (trial vectors)

## Documentation

[The documentation](doc/documentation.md)

[The API documentation](http://ukjhsa.github.io/adef/)

## License
ADEF is under the [MIT License](LICENSE)

## third-party library
ADEF uses the following libraryies:
- [JSON for Modern C++](https://github.com/nlohmann/json)
