@echo off

cd %APPVEYOR_BUILD_FOLDER%

echo Compiler: %COMPILER%
echo Platform: %PLATFORM%
echo MSYS2 directory: %MSYS2_DIR%
echo MSYS2 system: %MSYSTEM%

REM Create a writeable TMPDIR
mkdir %APPVEYOR_BUILD_FOLDER%\tmp
set TMPDIR=%APPVEYOR_BUILD_FOLDER%\tmp

IF %COMPILER%==msys2 (
  @echo on
  SET "PATH=C:\%MSYS2_DIR%\%MSYSTEM%\bin;C:\%MSYS2_DIR%\usr\bin;%PATH%"

  pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-python2

  REM download and extract llvm build artifact
  mkdir c:\projects\deps
  cd c:\projects\deps
  appveyor DownloadFile https://ci.appveyor.com/api/projects/mheinsen/llvm-with-seec-clang/artifacts/llvm_install_%LLVM_VERSION_STRING%.zip?branch=%LLVM_BRANCH%
  7z x llvm_install_%LLVM_VERSION_STRING%.zip -y
  dir c:\projects\deps

  REM causes problems linking libclang:
  del C:\msys64\usr\lib\libdl.a
)
