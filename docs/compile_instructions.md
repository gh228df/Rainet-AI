# Compilation

The rnab library is designed to be freestanding and therefore doesn't rely on libc or anything else. The supported platforms are:
- Windows x86-64, x86
- Linux x86-64, x86, ARM64, ARM32
- Android ARM64, ARM32

It should be compatible with any hardware, though for limited or very old systems you would either need to provide helper functions to handle 64-bit operations or rely on external dependencies.

**Please prefer clang over gcc if possible.**

# Flags

- `RNAB_DEBUG` - print verbose debug information (little to no runtime cost)
- `ENABLE_STRIP` - enable -s flag
- `BUILD_LIBRARY` - build librnab only
- `-DBRANCH_DEBUG` - track the cutoffs for each of the minimax entries (up to 2 times slower when enabled, only enable if you're willing to explore & improve the move order)
- `TABLE_BITS` - sets the transposition table size in bits (22 - 128MB, 23 - 256MB, 24 - 512MB, 25 - 1GB...), 23 is the default value. Higher values give the engine more 'memory'.

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
(rnab_export.h contains exported functions)

# Android (librnab.so)

ARM64
```
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_HOME%/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
ninja
```
ARM32
```
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK_HOME%/build/cmake/android.toolchain.cmake -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-21 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
ninja
```
(rnab_export.h contains exported functions)

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
docker run -it --rm -v $(pwd):/src -w /src amd64/alpine:edge sh -c '
  apk add --no-cache clang cmake ninja lld &&
  cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON &&
  cmake --build build
'
```

### Linux x86

```
docker run -it --rm -v $(pwd):/src -w /src i386/alpine:edge sh -c '
  apk add --no-cache clang cmake ninja lld &&
  cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON &&
  cmake --build build
'
```

### Linux ARM64 crosscompile on x86-64

```
sudo apt update
sudo apt install -y qemu-user-static binfmt-support # ensure we got qemu installed

docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

sudo docker run --rm --platform linux/arm64 -v $(pwd):/src -w /src arm64v8/alpine:edge sh -c '
  apk add --no-cache clang cmake ninja lld &&
  cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON &&
  cmake --build build
'
```

### Linux ARM32 crosscompile on x86-64

```
sudo apt update
sudo apt install -y qemu-user-static binfmt-support # ensure we got qemu installed

docker run --rm --privileged multiarch/qemu-user-static --reset -p yes

sudo docker run --rm --platform linux/arm/v7 -v $(pwd):/src -w /src arm32v7/alpine:edge sh -c '
  apk add --no-cache clang cmake ninja lld &&
  cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY=ON &&
  cmake --build build
'
```

(rnab_export.h contains exported functions)
