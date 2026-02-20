# Windows

## Default build with simple GUI
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
## Build librnab.dll library only 
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON
cmake --build build
```
(rnab_export.hpp contains exported functions)

# Android (librnab.so)

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
(rnab_export.hpp contains exported functions)

# Linux

## Default build with simple GUI
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
## Build librnab.so library only 

The easiest and most reliable way to get a maximally compatible binary is to build inside an **Alpine Linux** container.

### Linux x86-64

```
docker run -it --rm -v $(pwd):/src -w /src alpine:latest sh
apk add --no-cache clang lld cmake ninja musl-dev git python3 linux-headers
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON
cmake --build build
```

### Linux x86

```
docker run -it --rm -v $(pwd):/src -w /src i386/alpine:latest sh
apk add --no-cache clang lld cmake ninja musl-dev git python3 linux-headers
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON
cmake --build build
```

### Linux ARM64 crosscompile on x86-64

```
sudo apt update
sudo apt install -y qemu-user-static binfmt-support # ensure we got qemu installed

docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

sudo docker run --rm --platform linux/arm64 \
  -v $(pwd):/src -w /src \
  arm64v8/alpine:latest sh -c '
    apk add --no-cache clang lld cmake ninja musl-dev git python3 linux-headers &&
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON &&
    cmake --build build
  '
```

(rnab_export.hpp contains exported functions)
