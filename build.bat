@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
nvcc -O3 -std=c++17 -arch=sm_120 -Xcompiler "/O2 /EHsc /wd4819" ^
  -o RCKangaroo.exe ^
  Ec.cpp RCKangaroo.cpp GpuKang.cpp RCGpuCore.cu utils.cpp ^
  -lcuda
exit /b %errorlevel%
