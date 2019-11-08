#!/bin/bash

CMAKE_BUILD_TYPE=Release

txtbld=$(tput bold)
bldred=${txtbld}$(tput setaf 1)
txtrst=$(tput sgr0)

CORES=`getconf _NPROCESSORS_ONLN`

export OPT_LOCATION=$(pwd)/build/opt
export CMAKE_VERSION_MAJOR=3
export CMAKE_VERSION_MINOR=13
export CMAKE_VERSION_PATCH=2
export CMAKE_VERSION=${CMAKE_VERSION_MAJOR}.${CMAKE_VERSION_MINOR}.${CMAKE_VERSION_PATCH}
export BOOST_VERSION_MAJOR=1
export BOOST_VERSION_MINOR=67
export BOOST_VERSION_PATCH=0
export BOOST_VERSION=${BOOST_VERSION_MAJOR}_${BOOST_VERSION_MINOR}_${BOOST_VERSION_PATCH}
export BOOST_ROOT=${OPT_LOCATION}/boost_${BOOST_VERSION}
export BOOST_LINK_LOCATION=${OPT_LOCATION}/boost
export Boost_NO_BOOST_CMAKE=1
export LLVM_VERSION=release_40
export LLVM_ROOT=${OPT_LOCATION}/llvm
export LLVM_DIR=${LLVM_ROOT}/lib/cmake/llvm

# Setup directories
mkdir -p $OPT_LOCATION

# Find and use existing CMAKE
export CMAKE=$(command -v cmake 2>/dev/null)

ARCH=$( uname )
printf "\\nARCHITECTURE: %s\\n" "${ARCH}"

if [ "$ARCH" == "Linux" ]; then
   # Check if cmake is already installed or not and use source install location
   if [ -z $CMAKE ]; then export CMAKE=$HOME/bin/cmake; fi
   export OS_NAME=$( cat /etc/os-release | grep ^NAME | cut -d'=' -f2 | sed 's/\"//gI' )
   if [ ! -e /etc/os-release ]; then
      printf "\\nFractal currently supports Centos, Mint & Ubuntu Linux only.\\n"
      printf "Please install on the latest version of one of these Linux distributions.\\n"
      printf "https://www.centos.org/\\n"
      printf "https://linuxmint.com/\\n"
      printf "https://www.ubuntu.com/\\n"
      printf "Exiting now.\\n"
      exit 1
   fi
   case "$OS_NAME" in
      "Amazon Linux")
         FILE="./build-scripts/pre_build_amazon.sh"
         CXX_COMPILER=g++
         C_COMPILER=gcc
      ;;
      "CentOS Linux")
         FILE="./build-scripts/pre_build_centos.sh"
         export CMAKE=/root/opt/cmake/bin/cmake
         CXX_COMPILER=g++
         C_COMPILER=gcc
      ;;
      "elementary OS")
         FILE="./build-scripts/pre_build_ubuntu.sh"
         CXX_COMPILER=clang++-4.0
         C_COMPILER=clang-4.0
      ;;
      "Linux Mint")
         FILE="./build-scripts/pre_build_ubuntu.sh"
         CXX_COMPILER=clang++-4.0
         C_COMPILER=clang-4.0
      ;;
      "Ubuntu")
         FILE="./build-scripts/pre_build_ubuntu.sh"
         CXX_COMPILER=clang++-4.0
         C_COMPILER=clang-4.0
         #CXX_COMPILER=g++
         #C_COMPILER=gcc
      ;;
      "Debian GNU/Linux")
         FILE="./build-scripts/pre_build_ubuntu.sh"
         CXX_COMPILER=clang++-4.0
         C_COMPILER=clang-4.0
      ;;
      *)
         printf "\\nUnsupported Linux Distribution($OS_NAME). Exiting now.\\n\\n"
         exit 1
   esac
fi

if [ "$ARCH" == "Darwin" ]; then
   # Check if cmake is already installed or not and use source install location
   if [ -z $CMAKE ]; then export CMAKE=/usr/local/bin/cmake; fi
   export OS_NAME=MacOSX
   LOCAL_CMAKE_FLAGS="-DCMAKE_PREFIX_PATH=/usr/local/opt/gettext;$HOME/lib/cmake ${LOCAL_CMAKE_FLAGS}"
   FILE="./build-scripts/pre_build_darwin.sh"
   CXX_COMPILER=clang++
   C_COMPILER=clang
fi

. "$FILE"

mkdir -p build/libraries
cd build/libraries

$CMAKE -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
   -DCMAKE_C_COMPILER="${C_COMPILER}" \
   -DBoost_NO_BOOST_CMAKE=1 ../../libraries
if [ $? -ne 0 ]; then exit -1; fi
make -j"${CORES}"


