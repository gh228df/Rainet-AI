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
