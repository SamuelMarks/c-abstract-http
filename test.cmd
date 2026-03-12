call "C:\Program Files (x86)\Microsoft Visual Studio 8\Common7\Tools\vsvars32.bat"
cmake -S . -B build_msvc2005_libevent -G "NMake Makefiles" -DC_ABSTRACT_HTTP_USE_LIBEVENT=ON
cmake --build build_msvc2005_libevent
cd build_msvc2005_libevent
ctest -C Debug
