cd "/c/projects/seec_clang_build"
"C:\\Program Files (x86)\\cmake\\bin\\cmake.exe" -G"MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_EH=true -DLLVM_ENABLE_PIC=true -DLLVM_ENABLE_RTTI=true -DLLVM_INCLUDE_TESTS=off -DCLANG_BUILD_TOOLS=off -DCMAKE_PROGRAM_PATH="C:/projects/deps/bin" -DPYTHON_EXECUTABLE="C:/msys64/mingw64/bin/python2.7.exe" -DENABLE_SHARED=off -DCMAKE_INSTALL_PREFIX="C:\\projects\\seec_clang_install" $APPVEYOR_BUILD_FOLDER
make -j4
make install
cd $APPVEYOR_BUILD_FOLDER
7z a seec_clang_install_$LLVM_VERSION_STRING.zip "C:\\projects\\seec_clang_install\\*"
