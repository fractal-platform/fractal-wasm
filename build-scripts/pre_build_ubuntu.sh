# llvm-4.0 is installed into /usr/lib/llvm-4.0
# clang is necessary for building on ubuntu
DEP_ARRAY=(
	git llvm-4.0 clang-4.0 libclang-4.0-dev make automake libbz2-dev libssl-dev doxygen graphviz \
	libgmp3-dev autotools-dev build-essential libicu-dev python2.7 python2.7-dev python3 python3-dev \
	autoconf libtool curl zlib1g-dev sudo ruby libusb-1.0-0-dev libcurl4-gnutls-dev pkg-config
)
COUNT=1
DISPLAY=""
DEP=""

read -p "Do you wish to update repositories with apt-get update? (y/n) " ANSWER
case $ANSWER in
	1 | [Yy]* )
		if ! sudo apt-get update; then
			printf " - APT update failed.\\n"
			exit 1;
		else
			printf " - APT update complete.\\n"
		fi
	;;
	[Nn]* ) echo "Proceeding without update!";;
	* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
esac

printf "\\nChecking for installed dependencies...\\n"
for (( i=0; i<${#DEP_ARRAY[@]}; i++ )); do
	pkg=$( dpkg -s "${DEP_ARRAY[$i]}" 2>/dev/null | grep Status | tr -s ' ' | cut -d\  -f4 )
	if [ -z "$pkg" ]; then
		DEP=$DEP" ${DEP_ARRAY[$i]} "
		DISPLAY="${DISPLAY}${COUNT}. ${DEP_ARRAY[$i]}\\n"
		printf " - Package %s${bldred} NOT${txtrst} found!\\n" "${DEP_ARRAY[$i]}"
		(( COUNT++ ))
	else
		printf " - Package %s found.\\n" "${DEP_ARRAY[$i]}"
		continue
	fi
done
if [ "${COUNT}" -gt 1 ]; then
	printf "\\nThe following dependencies are required to install Fractal Wasm Lib:\\n"
	printf "${DISPLAY}\\n\\n"
	read -p "Do you wish to install these packages? (y/n) " ANSWER
	case $ANSWER in
		1 | [Yy]* )
			if ! sudo apt-get -y install ${DEP}; then
				printf " - APT dependency failed.\\n"
				exit 1
			else
				printf " - APT dependencies installed successfully.\\n"
			fi
		;;
		[Nn]* ) echo "User aborting installation of required dependencies, Exiting now."; exit;;
		* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
	esac
else
	printf " - No required APT dependencies to install."
fi


printf "\\n"


printf "Checking CMAKE installation...\\n"
if [ ! -e $CMAKE ]; then
	printf "Installing CMAKE...\\n"
	curl -LO https://cmake.org/files/v$CMAKE_VERSION_MAJOR.$CMAKE_VERSION_MINOR/cmake-$CMAKE_VERSION.tar.gz \
	&& tar -xzf cmake-$CMAKE_VERSION.tar.gz \
	&& cd cmake-$CMAKE_VERSION \
	&& ./bootstrap --prefix=$HOME \
	&& make -j"${JOBS}" \
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
	&& cd boost_${BOOST_VERSION}\
	&& ./bootstrap.sh --prefix=.\
	&& ./b2 cxxflags=-fPIC -q -j"${CORES}" --with-iostreams --with-date_time --with-filesystem \
						--with-system --with-program_options --with-chrono --with-test install \
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
	ln -s /usr/lib/llvm-4.0 $LLVM_ROOT \
	|| exit 1
	printf " - LLVM successfully linked from /usr/lib/llvm-4.0 to ${LLVM_ROOT}\\n"
else
	printf " - LLVM found @ ${LLVM_ROOT}.\\n"
fi
if [ $? -ne 0 ]; then exit -1; fi

printf "\\n"


