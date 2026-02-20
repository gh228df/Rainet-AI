## Default build with simple GUI
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
## Build librnab library only 
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON
cmake --build build
```
(rnab_export.hpp contains exported functions)

## Android (librnab.so)

ARM64
```
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_HOME%/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
ninja
```
ARM V7
```
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_HOME%/build/cmake/android.toolchain.cmake -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-21 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
ninja
```
