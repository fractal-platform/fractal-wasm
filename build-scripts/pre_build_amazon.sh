OS_VER=$( grep VERSION_ID /etc/os-release | cut -d'=' -f2 | sed 's/[^0-9\.]//gI' | cut -d'.' -f1 )

if [[ "${OS_NAME}" == "Amazon Linux AMI" ]]; then
	DEP_ARRAY=(
		sudo procps util-linux which gcc72 gcc72-c++ autoconf automake libtool make doxygen graphviz \
		bzip2 bzip2-devel openssl-devel gmp gmp-devel libstdc++72 python27 python27-devel python34 python34-devel \
		libedit-devel ncurses-devel swig wget file libcurl-devel libusb1-devel
	)
else
	DEP_ARRAY=(
		git procps-ng util-linux gcc gcc-c++ autoconf automake libtool make bzip2 \
		bzip2-devel openssl-devel gmp-devel libstdc++ libcurl-devel libusbx-devel \
		python3 python3-devel python-devel libedit-devel doxygen graphviz
	)
fi

COUNT=1
DISPLAY=""
DEP=""

if [[ "${OS_NAME}" == "Amazon Linux AMI" && "${OS_VER}" -lt 2017 ]]; then
	printf "You must be running Amazon Linux 2017.09 or higher to compile Fractal Wasm Lib.\\n"
	printf "exiting now.\\n"
	exit 1
fi

printf "\\nChecking Yum installation.\\n"
if ! YUM=$( command -v yum 2>/dev/null )
then
	printf "\\nYum must be installed to compile Fractal Wasm Lib.\\n"
	printf "\\nExiting now.\\n"
	exit 1
fi
printf "Yum installation found at ${YUM}.\\n"

read -p "Do you wish to update YUM repositories? (y/n) " ANSWER
case $ANSWER in
	1 | [Yy]* )
		if ! sudo $YUM -y update; then
			printf " - YUM update failed.\\n"
			exit 1;
		else
			printf " - YUM update complete.\\n"
		fi
	;;
	[Nn]* ) echo " - Proceeding without update!";;
	* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
esac

printf "Checking RPM for installed dependencies...\\n"
for (( i=0; i<${#DEP_ARRAY[@]}; i++ )); do
	pkg=$( rpm -qi "${DEP_ARRAY[$i]}" 2>/dev/null | grep Name )
	if [[ -z $pkg ]]; then
		DEP=$DEP" ${DEP_ARRAY[$i]} "
		DISPLAY="${DISPLAY}${COUNT}. ${DEP_ARRAY[$i]}\\n"
		printf " - Package %s ${bldred} NOT ${txtrst} found!\\n" "${DEP_ARRAY[$i]}"
		(( COUNT++ ))
	else
		printf " - Package %s found.\\n" "${DEP_ARRAY[$i]}"
		continue
	fi
done
if [ "${COUNT}" -gt 1 ]; then
	printf "\\nThe following dependencies are required to compile Fractal Wasm Lib:\\n"
	printf "${DISPLAY}\\n\\n"
	read -p "Do you wish to install these dependencies? (y/n) " ANSWER
	case $ANSWER in
		1 | [Yy]* )
			if ! sudo $YUM -y install ${DEP}; then
				printf " - YUM dependency installation failed!\\n"
				exit 1;
			else
				printf " - YUM dependencies installed successfully.\\n"
			fi
		;;
		[Nn]* ) echo "User aborting installation of required dependencies, Exiting now."; exit;;
		* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
	esac
else
	printf " - No required YUM dependencies to install.\\n"
fi

printf "Checking CMAKE installation...\\n"
if [ ! -e $CMAKE ]; then
	printf "Installing CMAKE...\\n"
	curl -LO https://cmake.org/files/v$CMAKE_VERSION_MAJOR.$CMAKE_VERSION_MINOR/cmake-$CMAKE_VERSION.tar.gz \
	&& tar -xzf cmake-$CMAKE_VERSION.tar.gz \
	&& cd cmake-$CMAKE_VERSION \
	&& ./bootstrap --prefix=$HOME \
	&& make -j"${CORES}" \
	&& make install \
	&& cd .. \
	&& rm -f cmake-$CMAKE_VERSION.tar.gz \
	|| exit 1
	printf " - CMAKE successfully installed @ ${CMAKE} \\n"
else
	printf " - CMAKE found @ ${CMAKE}.\\n"
fi
if [ $? -ne 0 ]; then exit -1; fi


printf "\\n"


printf "Checking Boost library (${BOOST_VERSION}) installation...\\n"
BOOSTVERSION=$( grep "#define BOOST_VERSION" "$BOOST_ROOT/include/boost/version.hpp" 2>/dev/null | tail -1 | tr -s ' ' | cut -d\  -f3 )
if [ "${BOOSTVERSION}" != "${BOOST_VERSION_MAJOR}0${BOOST_VERSION_MINOR}0${BOOST_VERSION_PATCH}" ]; then
    pushd $OPT_LOCATION &> /dev/null
	printf "Installing Boost library...\\n"
	curl -LO https://dl.bintray.com/boostorg/release/${BOOST_VERSION_MAJOR}.${BOOST_VERSION_MINOR}.${BOOST_VERSION_PATCH}/source/boost_$BOOST_VERSION.tar.bz2 \
	&& tar -xjf boost_$BOOST_VERSION.tar.bz2 \
	&& cd $BOOST_ROOT \
	&& ./bootstrap.sh --prefix=. \
	&& ./b2 cxxflags=-fPIC -q -j"${CORES}" install \
	&& cd .. \
	&& rm -f boost_$BOOST_VERSION.tar.bz2 \
	&& ln -fs boost_${BOOST_VERSION} boost\
	|| exit 1
	popd &> /dev/null
	printf " - Boost library successfully installed @ ${BOOST_ROOT} (Symlinked to ${BOOST_LINK_LOCATION}).\\n"
else
	printf " - Boost library found with correct version @ ${BOOST_ROOT} (Symlinked to ${BOOST_LINK_LOCATION}).\\n"
fi
if [ $? -ne 0 ]; then exit -1; fi


printf "\\n"


printf "Checking LLVM 4 support...\\n"
if [ ! -d $LLVM_ROOT ]; then
    pushd $OPT_LOCATION &> /dev/null
	printf "Installing LLVM 4...\\n"
	git clone --depth 1 --single-branch --branch $LLVM_VERSION https://github.com/llvm-mirror/llvm.git llvm && cd llvm \
	&& mkdir build \
	&& cd build \
	&& $CMAKE -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="${LLVM_ROOT}" -DLLVM_TARGETS_TO_BUILD="host" -DLLVM_BUILD_TOOLS=false -DLLVM_ENABLE_RTTI=1 -DCMAKE_BUILD_TYPE="Release" .. \
	&& make -j"${CORES}" \
	&& make install \
	&& cd ../.. \
	|| exit 1
	popd &> /dev/null
	printf " - LLVM successfully installed @ ${LLVM_ROOT}\\n"
else
	printf " - LLVM found @ ${LLVM_ROOT}.\\n"
fi
if [ $? -ne 0 ]; then exit -1; fi

printf "\\n"
