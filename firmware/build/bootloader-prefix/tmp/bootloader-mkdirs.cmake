# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/nix/store/xnq0z77rvjvzz1df8mx0bpk7y2igxjd5-esp-idf-v5.5.2/components/bootloader/subproject")
  file(MAKE_DIRECTORY "/nix/store/xnq0z77rvjvzz1df8mx0bpk7y2igxjd5-esp-idf-v5.5.2/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "/home/artverno/Documents/Projects/tloc2/firmware/build/bootloader"
  "/home/artverno/Documents/Projects/tloc2/firmware/build/bootloader-prefix"
  "/home/artverno/Documents/Projects/tloc2/firmware/build/bootloader-prefix/tmp"
  "/home/artverno/Documents/Projects/tloc2/firmware/build/bootloader-prefix/src/bootloader-stamp"
  "/home/artverno/Documents/Projects/tloc2/firmware/build/bootloader-prefix/src"
  "/home/artverno/Documents/Projects/tloc2/firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/artverno/Documents/Projects/tloc2/firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/artverno/Documents/Projects/tloc2/firmware/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
