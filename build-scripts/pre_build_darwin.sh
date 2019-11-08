
export HOMEBREW_NO_AUTO_UPDATE=1

COUNT=1
DISPLAY=""
DEPS=""

printf "\\n"

printf "Checking xcode-select installation...\\n"
if ! XCODESELECT=$( command -v xcode-select)
then
	printf " - XCode must be installed in order to proceed!\\n\\n"
	exit 1
fi
printf " - XCode installation found @ ${XCODESELECT}\\n"

printf "Checking Home Brew installation...\\n"
if ! BREW=$( command -v brew )
then
    printf "BREW: ${BREW}"
	printf "Homebrew must be installed to compile Fractal Wasm Lib!\\n"
	read -p "Do you wish to install HomeBrew? (y/n)? " ANSWER
	case $ANSWER in
		1 | [Yy]* )
			"${XCODESELECT}" --install 2>/dev/null;
			if ! "${RUBY}" -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"; then
				echo " - Unable to install homebrew at this time."
				exit 1;
			else
				BREW=$( command -v brew )
			fi
		;;
		[Nn]* ) echo "User aborted homebrew installation. Exiting now."; exit 1;;
		* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
	esac
fi
printf " - Home Brew installation found @ ${BREW}\\n"

printf "\\nChecking dependencies...\\n"
var_ifs="${IFS}"
IFS=","
while read -r name tester testee brewname uri; do
	if [ "${tester}" "${testee}" ]; then
		printf " - %s found\\n" "${name}"
		continue
	fi
	# resolve conflict with homebrew glibtool and apple/gnu installs of libtool
	if [ "${testee}" == "/usr/local/bin/glibtool" ]; then
		if [ "${tester}" "/usr/local/bin/libtool" ]; then
			printf " - %s found\\n" "${name}"
			continue
		fi
	fi
	DEPS=$DEPS"${brewname},"
	DISPLAY="${DISPLAY}${COUNT}. ${name}\\n"
	printf " - %s ${bldred}NOT${txtrst} found.\\n" "${name}"
	(( COUNT++ ))
done < "./build-scripts/pre_build_darwin_deps"
IFS="${var_ifs}"

if [ ! -d /usr/local/Frameworks ]; then
	printf "\\n${bldred}/usr/local/Frameworks is necessary to brew install python@3. Run the following commands as sudo and try again:${txtrst}\\n"
	printf "sudo mkdir /usr/local/Frameworks && sudo chown $(whoami):admin /usr/local/Frameworks\\n\\n"
	exit 1;
fi

if [ $COUNT -gt 1 ]; then
	printf "\\nThe following dependencies are required to compile Fractal Wasm Lib:\\n"
	printf "${DISPLAY}\\n\\n"
	read -p "Do you wish to install these packages? (y/n) " ANSWER
	case $ANSWER in
		1 | [Yy]* )
			"${XCODESELECT}" --install 2>/dev/null;
			read -p "Do you wish to update homebrew packages first? (y/n) " ANSWER
			case $ANSWER in
				1 | [Yy]* )
					if ! brew update; then
						printf " - Brew update failed.\\n"
						exit 1;
					else
						printf " - Brew update complete.\\n"
					fi
				;;
				[Nn]* ) echo "Proceeding without update!";;
				* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
			esac

			printf "\\nInstalling Dependencies...\\n"
			# Ignore cmake so we don't install a newer version.
			# Build from source to use local cmake
			# DON'T INSTALL llvm@4 WITH --force!
			OIFS="$IFS"
			IFS=$','
			for DEP in $DEPS; do
				# Eval to support string/arguments with $DEP
				if ! eval $BREW install $DEP; then
					printf " - Homebrew exited with the above errors!\\n"
					exit 1;
				fi
			done
			IFS="$OIFS"
		;;
		[Nn]* ) echo "User aborting installation of required dependencies, Exiting now."; exit;;
		* ) echo "Please type 'y' for yes or 'n' for no."; exit;;
	esac
else
	printf "\\n - No required Home Brew dependencies to install.\\n"
fi


printf "\\n"


export CPATH="$(python-config --includes | awk '{print $1}' | cut -dI -f2):$CPATH" # Boost has trouble finding pyconfig.h
printf "Checking Boost library (${BOOST_VERSION}) installation...\\n"
BOOSTVERSION=$( grep "#define BOOST_VERSION" "$BOOST_ROOT/include/boost/version.hpp" 2>/dev/null | tail -1 | tr -s ' ' | cut -d\  -f3 )
if [ "${BOOSTVERSION}" != "${BOOST_VERSION_MAJOR}0${BOOST_VERSION_MINOR}0${BOOST_VERSION_PATCH}" ]; then
    pushd $OPT_LOCATION &> /dev/null
	printf "Installing Boost library...\\n"
	curl -LO https://dl.bintray.com/boostorg/release/$BOOST_VERSION_MAJOR.$BOOST_VERSION_MINOR.$BOOST_VERSION_PATCH/source/boost_$BOOST_VERSION.tar.bz2 \
	&& tar -xjf boost_$BOOST_VERSION.tar.bz2 \
	&& cd boost_${BOOST_VERSION}\
	&& ./bootstrap.sh --prefix=.\
	&& ./b2 -q -j$(sysctl -in machdep.cpu.core_count) --with-iostreams --with-date_time --with-filesystem \
	                                                  --with-system --with-program_options --with-chrono --with-test install \
	&& cd .. \
	&& rm -f boost_$BOOST_VERSION.tar.bz2 \
	&& ln -fs boost_${BOOST_VERSION} boost\
	|| exit 1
	popd &> /dev/null
	printf " - Boost library successfully installed @ ${BOOST_ROOT}.\\n"
else
	printf " - Boost library found with correct version @ ${BOOST_ROOT}.\\n"
fi
if [ $? -ne 0 ]; then exit -1; fi


printf "\\n"

# We install llvm into /usr/local/opt using brew install llvm@4
printf "Checking LLVM 4 support...\\n"
if [ ! -d $LLVM_ROOT ]; then
	ln -s /usr/local/opt/llvm@4 $LLVM_ROOT \
	|| exit 1
	printf " - LLVM successfully linked from /usr/local/opt/llvm@4 to ${LLVM_ROOT}\\n"
else
	printf " - LLVM found @ ${LLVM_ROOT}.\\n"
fi

printf "\\n"

