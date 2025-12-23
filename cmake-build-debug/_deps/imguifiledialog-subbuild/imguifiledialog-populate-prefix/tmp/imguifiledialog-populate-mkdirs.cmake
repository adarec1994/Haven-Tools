# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-src"
  "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-build"
  "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix"
  "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/tmp"
  "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp"
  "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src"
  "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/matthew/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
