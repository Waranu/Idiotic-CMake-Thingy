## What is it?

A minimal, hacky CMake template that builds libraries from source so I don't have to suffer over and over again. 

> **Note:** This is extremely hacky/inefficient garbage that should only be used for quick prototyping. 

## What You Should Know

* **Debug by default**: No build type is specified in the CMake file, so it defaults to Debug.
* **Static linking**: All libraries are linked statically.
* **Demo code**: `src/main.c` includes a simple SDL2 + Nuklear example for quick testing.
* **Library Organization**:

  * `vendor/` holds prebuilt libraries that are a pain to build from source. (Update these occasionally.)
  * `deps/` is for libraries that can be built from source easily.
- **Switching from C to C++**: 

    Change the following line in the CMakelists.txt file:
    
    From this: 
    ```txt
    file(GLOB_RECURSE SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
    ```
    To This:
    ```txt
    file(GLOB_RECURSE SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    ```


## Prerequisites

* [Python](https://www.python.org/downloads/)
* [CMake](https://cmake.org/download/)
* A C and C++ compiler (e.g., GCC, Clang, or MSVC. I usually just use [this](https://github.com/brechtsanders/winlibs_mingw/releases/download/15.1.0posix-12.0.0-msvcrt-r1/winlibs-x86_64-posix-seh-gcc-15.1.0-mingw-w64msvcrt-12.0.0-r1.zip))
## Getting Started

1. **Clone the repository**:

   ```bash
   git clone https://github.com/Waranu/Idiotic-CMake-Thingy.git
   cd Idiotic-CMake-Thingy
   ```

2. **Install Python dependencies**:

   ```bash
   pip install -r requirements.txt
   ```

3. **Download libraries**:
   Create a deps/ directory in this repo's cloned directory. 

   The `download_libs.py` script reads a JSON file containing supported libraries. To download one or more libraries, run:

   ```bash
   python download_libs.py [LIB_NAME_1] [LIB_NAME_2 ...] [--shallow_submodules]
   ```

   * `[LIB_NAME_X]`: Exact name of the library as specified in the JSON.

   * `--shallow_submodules` (optional): Perform shallow clones for each submodule.
   * Example: 
    ```bash
    python download_libs.py SDL2 NUKLEAR GLAD --shallow_submodules
    ```

   > **Note:** Running this script will clear the `deps/` directory each time.

4. **Configure the build**:

   1. Check your available CMake generators:

      ```bash
      cmake -G
      ```
   2. Create a `build/` folder, and create a shell (or .ps1/.cmd) script named `build.sh` (or `.ps1`/`.cmd`) in it, with the following content:

      ```bash
      cmake .. 
        -G "[YOUR_GENERATOR_NAME]" -D[LIB_NAME]=ON -D[ANOTHER_LIB]=ON -DCMAKE_C_COMPILER=[your_c_compiler]-DCMAKE_CXX_COMPILER=[your_cxx_compiler]
      ```

      > **Note:** Lib names in json and cmake are exactly the same. 

      For example, using MinGW on Windows:

      ```bash
      cmake .. -G "MinGW Makefiles" -DSDL2=ON -DNUKLEAR=ON -DGLAD=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
      ```

5. **Build**:
   
   For windows, 

   ```bash
   mingw32-make
   ```
   For linux, it is usually: 
   ```bash
    make
   ```

6. **Run**:

   After building, run the executable (usually `./game` or `game.exe`) from the `build/` directory.

---

Chass chao 👍 (means to have fun in the present moment, in my native language) . 
