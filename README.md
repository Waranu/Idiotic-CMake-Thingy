# Note 
Build type is not specified in cmake. So it will build everything in debug mode.
You can set it to release mode using -DCMAKE_BUILD_TYPE=Release flag. 

Every library is linked statically by default. 
# Flags to give to cmake
- -DHAVE_SDL=ON
  - [Repo Link](https://github.com/libsdl-org/SDL.git)
  - It downloads the stable release, which is 2.30.9 at time of writing this.
  - HAPTIC is disabled by default. 
- -DHAVE_PREFIXED_RAYLIB=ON (for the inclusion of prefixed raylib)
    - [Repo Link](https://github.com/CrackedPixel/raylib.git)
    - The current vendored version is for mingw64
- -DHAVE_BOX2D=ON
  - [Repo Link](https://github.com/erincatto/box2d.git)