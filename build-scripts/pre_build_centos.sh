printf "Checking Yum installation...\\n"
if ! YUM=$( command -v yum 2>/dev/null ); then
		printf "!! Yum must be installed to compile Fractal Wasm Lib!!\\n"
		printf "Exiting now.\\n"
		exit 1;
fi
printf " - Yum installation found at %s.\\n" "${YUM}"

read -p "Do you wish to update YUM repositories? (y/n) " ANSWER;
case $ANSWER in
	1 | [Yy]* )
		if ! "${YUM}" -y update; then
			printf " - YUM update failed.\\n"
			exit 1;
		else
			printf " - YUM update complete.\\n"
		fi
	;;
	[Nn]* ) echo " - Proceeding without update!";;
	* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
esac

printf "Checking installation of Centos Software Collections Repository...\\n"
SCL=$( rpm -qa | grep -E 'centos-release-scl-[0-9].*' )
if [ -z "${SCL}" ]; then
	if [ $ANSWER != 1 ]; then read -p "Do you wish to install and enable this repository? (y/n)? " ANSWER; fi
	case $ANSWER in
		1 | [Yy]* )
			printf "Installing SCL...\\n"
			if ! "${YUM}" -y --enablerepo=extras install centos-release-scl 2>/dev/null; then
				printf "!! Centos Software Collections Repository installation failed !!\\n"
				printf "Exiting now.\\n\\n"
				exit 1;
			else
				printf "Centos Software Collections Repository installed successfully.\\n"
			fi
		;;
		[Nn]* ) echo "User aborting installation of required Centos Software Collections Repository, Exiting now."; exit;;
	* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
	esac
else
	printf " - ${SCL} found.\\n"
fi

printf "Checking installation of devtoolset-7...\\n"
DEVTOOLSET=$( rpm -qa | grep -E 'devtoolset-7-[0-9].*' )
if [ -z "${DEVTOOLSET}" ]; then
	if [ $ANSWER != 1 ]; then read -p "Do you wish to install devtoolset-7? (y/n)? " ANSWER; fi
	case $ANSWER in
		1 | [Yy]* )
			printf "Installing devtoolset-7...\\n"
			if ! "${YUM}" install -y devtoolset-7; then
					printf "!! Centos devtoolset-7 installation failed !!\\n"
					printf "Exiting now.\\n"
					exit 1;
			else
					printf " - Centos devtoolset installed successfully!\\n"
			fi
		;;
		[Nn]* ) echo "User aborting installation of devtoolset-7. Exiting now."; exit;;
		* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
	esac
else
	printf " - ${DEVTOOLSET} found.\\n"
fi
if [ -d /opt/rh/devtoolset-7 ]; then
	printf "Enabling Centos devtoolset-7 so we can use GCC 7...\\n"
	source /opt/rh/devtoolset-7/enable || exit 1
	printf " - Centos devtoolset-7 successfully enabled!\\n"
fi

printf "\\n"

DEP_ARRAY=( 
	git autoconf automake libtool make bzip2 doxygen graphviz \
	bzip2-devel openssl-devel gmp-devel \
	ocaml libicu-devel python python-devel python33 \
	gettext-devel file sudo libusbx-devel libcurl-devel
 )
COUNT=1
DISPLAY=""
DEP=""
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
	read -p "Do you wish to install these dependencies? (y/n) " ANSWER;
	case $ANSWER in
		1 | [Yy]* )
			if ! "${YUM}" -y install ${DEP}; then
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

if [ -d /opt/rh/python33 ]; then
	printf "Enabling python33...\\n"
	source /opt/rh/python33/enable || exit 1
	printf " - Python33 successfully enabled!\\n"
fi

printf "\\n"

printf "Checking CMAKE installation...\\n"
if [ ! -e $CMAKE ]; then
    pushd $OPT_LOCATION &> /dev/null
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
	popd &> /dev/null
	printf " - CMAKE successfully installed @ ${CMAKE} \\n"
else
	printf " - CMAKE found @ ${CMAKE}.\\n"
fi
if [ $? -ne 0 ]; then exit -1; fi

printf "\\n"

export CPATH="$CPATH:/opt/rh/python33/root/usr/include/python3.3m" # m on the end causes problems with boost finding python3
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
	#git clone --depth 1 --single-branch --branch $LLVM_VERSION https://github.com/llvm-mirror/llvm.git llvm && cd llvm \
	cd llvm_git \
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


