# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-src")
  file(MAKE_DIRECTORY "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-build"
  "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix"
  "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/tmp"
  "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp"
  "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src"
  "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/pwd12/OneDrive/Documents/GitHub/Haven-Tools/cmake-build-debug/_deps/imguifiledialog-subbuild/imguifiledialog-populate-prefix/src/imguifiledialog-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
