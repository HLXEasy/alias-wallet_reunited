#!/bin/bash
# ===========================================================================
#
# Created: 2019-10-22 HLXEasy
#
# This script can be used to build Spectrecoin for Android using CMake
#
# ===========================================================================

# ===========================================================================
# Store path from where script was called, determine own location
# and source helper content from there
callDir=$(pwd)
ownLocation="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "${ownLocation}" || die 1 "Unable to cd into own location ${ownLocation}"
. ./include/helpers_console.sh
_init
. ./include/handle_buildconfig.sh

##### ### # Global definitions # ### ########################################
##### ### # Android # ### ###################################################
ANDROID_NDK_ARCHIVE_LOCATION=${ARCHIVES_ROOT_DIR}/Android
ANDROID_NDK_ROOT=${ANDROID_NDK_ARCHIVE_LOCATION}/android-ndk-${ANDROID_NDK_VERSION}
ANDROID_TOOLCHAIN_CMAKE=${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake
ANDROID_ARCH=arm64
ANDROID_ABI=arm64-v8a
ANDROID_API=23

##### ### # Boost # ### #####################################################
# Location of Boost will be resolved by trying to find required Boost libs
BOOST_ARCHIVE_LOCATION=${ARCHIVES_ROOT_DIR}/Boost
BOOST_ROOT=${BOOST_ARCHIVE_LOCATION}/boost_${BOOST_VERSION//./_}_android_${ANDROID_ARCH}
BOOST_INCLUDEDIR=${BOOST_ROOT}/include
BOOST_LIBRARYDIR=${BOOST_ROOT}/lib
BOOST_REQUIRED_LIBS='chrono filesystem iostreams program_options system thread regex date_time atomic'
# regex date_time atomic

##### ### # BerkeleyDB # ### ################################################
# Location of archive will be resolved like this:
# ${BERKELEYDB_ARCHIVE_LOCATION}/db-${BERKELEYDB_BUILD_VERSION}.tar.gz
BERKELEYDB_ARCHIVE_LOCATION=${ARCHIVES_ROOT_DIR}/BerkeleyDB

##### ### # OpenSSL # ### ###################################################
# Location of archive will be resolved like this:
# ${OPENSSL_ARCHIVE_LOCATION}/openssl-${OPENSSL_BUILD_VERSION}.tar.gz
#OPENSSL_ARCHIVE_LOCATION=https://mirror.viaduck.org/openssl
OPENSSL_ARCHIVE_LOCATION=${ARCHIVES_ROOT_DIR}/OpenSSL

##### ### # EventLib # ### ##################################################
# Location of archive will be resolved like this:
# ${EVENTLIB_ARCHIVE_LOCATION}/libevent-${EVENTLIB_BUILD_VERSION}-stable.tar.gz
EVENTLIB_ARCHIVE_LOCATION=${ARCHIVES_ROOT_DIR}/EventLib

##### ### # ZLib # ### ######################################################
# Location of archive will be resolved like this:
# ${ZLIB_ARCHIVE_LOCATION}/v${ZLIB_BUILD_VERSION}.tar.gz
ZLIB_ARCHIVE_LOCATION=${ARCHIVES_ROOT_DIR}/ZLib

##### ### # XZLib # ### #####################################################
# Location of archive will be resolved like this:
# ${XZLIB_ARCHIVE_LOCATION}/xz-${XZLIB_BUILD_VERSION}.tar.gz
XZLIB_ARCHIVE_LOCATION=${ARCHIVES_ROOT_DIR}/XZLib

##### ### # Tor # ### #######################################################
# Location of archive will be resolved like this:
# ${XZLIB_ARCHIVE_LOCATION}/tor-${XZLIB_BUILD_VERSION}.tar.gz
TOR_ARCHIVE_LOCATION=${ARCHIVES_ROOT_DIR}/Tor

BUILD_DIR=cmake-build-android-cmdline_${ANDROID_ARCH}

helpMe() {
    echo "

    Helper script to build Spectrecoin wallet and daemon using CMake.
    Assumptions:
    - The BerkeleyDB archive to use must be located on ${BERKELEYDB_ARCHIVE_LOCATION}
    - Naming must be default like 'db-4.8.30.tar.gz'
    - Android toolchain already installed

    Usage:
    ${0} [options]

    Optional parameters:
    -c <cores-to-use>
        The amount of cores to use for build. If not using this option
        the script determines the available cores on this machine.
        Not used for build steps of external libraries like OpenSSL or
        BerkeleyDB.
    -f  Perform fullbuild by cleanup all generated data from previous
        build runs.
    -t  Build with included Tor
    -h  Show this help

    "
}

checkOpenSSLArchive(){
    info ""
    if [[ -e "${OPENSSL_ARCHIVE_LOCATION}/openssl-${OPENSSL_BUILD_VERSION}.tar.gz" ]] ; then
        info "Using OpenSSL archive ${OPENSSL_ARCHIVE_LOCATION}/openssl-${OPENSSL_BUILD_VERSION}.tar.gz"
    else
        OPENSSL_ARCHIVE_URL=https://mirror.viaduck.org/openssl/openssl-${OPENSSL_BUILD_VERSION}.tar.gz
        info "Downloading OpenSSL archive ${OPENSSL_ARCHIVE_URL}"
        if [[ ! -e ${OPENSSL_ARCHIVE_LOCATION} ]] ; then
            mkdir -p ${OPENSSL_ARCHIVE_LOCATION}
        fi
        cd ${OPENSSL_ARCHIVE_LOCATION}
        wget ${OPENSSL_ARCHIVE_URL}
        cd - >/dev/null
    fi
}

checkBerkeleyDBArchive(){
    info ""
    if [[ -e "${BERKELEYDB_ARCHIVE_LOCATION}/db-${BERKELEYDB_BUILD_VERSION}.tar.gz" ]] ; then
        info "Using BerkeleyDB archive ${BERKELEYDB_ARCHIVE_LOCATION}/db-${BERKELEYDB_BUILD_VERSION}.tar.gz"
    else
        BERKELEYDB_ARCHIVE_URL=https://download.oracle.com/berkeley-db/db-${BERKELEYDB_BUILD_VERSION}.tar.gz
        info "Downloading BerkeleyDB archive ${BERKELEYDB_ARCHIVE_URL}"
        if [[ ! -e ${BERKELEYDB_ARCHIVE_LOCATION} ]] ; then
            mkdir -p ${BERKELEYDB_ARCHIVE_LOCATION}
        fi
        cd ${BERKELEYDB_ARCHIVE_LOCATION}
        wget ${BERKELEYDB_ARCHIVE_URL}
        cd - >/dev/null
    fi
}

checkBoost(){
    info ""
    info "Searching required static Boost libs"
    buildBoost=false
    if [[ -d ${BOOST_LIBRARYDIR} ]] ; then
        for currentBoostDependency in ${BOOST_REQUIRED_LIBS} ; do
            if [[ -n $(find ${BOOST_LIBRARYDIR}/ -name "libboost_${currentBoostDependency}*.a") ]] ; then
                info "${currentBoostDependency}: OK"
            else
                warning "${currentBoostDependency}: Not found!"
                buildBoost=true
            fi
        done
    else
        warning "Boost library directory ${BOOST_LIBRARYDIR} not found!"
        buildBoost=true
    fi
    if ${buildBoost} ; then
        local currentDir=$(pwd)
        if [[ ! -e ${BOOST_ARCHIVE_LOCATION} ]] ; then
            mkdir -p ${BOOST_ARCHIVE_LOCATION}
        fi
        cd ${BOOST_ARCHIVE_LOCATION}
        if [[ ! -e "boost_${BOOST_VERSION//./_}.tar.gz" ]] ; then
            info "Downloading Boost archive"
            wget https://dl.bintray.com/boostorg/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION//./_}.tar.gz
        else
            info "Using existing Boost archive"
        fi
        info "Cleanup before extraction"
        rm -rf boost_${BOOST_VERSION//./_}_android_${ANDROID_ARCH}
        mkdir boost_${BOOST_VERSION//./_}_android_${ANDROID_ARCH}
        cd boost_${BOOST_VERSION//./_}_android_${ANDROID_ARCH}
        info "Extracting Boost archive"
        tar xzf ../boost_${BOOST_VERSION//./_}.tar.gz
        cd boost_${BOOST_VERSION//./_}
        mv * ../
        cd - >/dev/null
        rm -rf boost_${BOOST_VERSION//./_}
        info "Building Boost"
        ${ownLocation}/build-boost-for-android.sh -v ${BOOST_VERSION} -a ${ANDROID_ARCH} -n ${ANDROID_NDK_ROOT} -l ${BOOST_REQUIRED_LIBS// /,}
        cd "${currentDir}"
    fi
}

checkEventLibArchive(){
    info ""
    if [[ -e "${EVENTLIB_ARCHIVE_LOCATION}/libevent-${EVENTLIB_BUILD_VERSION}-stable.tar.gz" ]] ; then
        info "Using EventLib archive ${EVENTLIB_ARCHIVE_LOCATION}/libevent-${EVENTLIB_BUILD_VERSION}-stable.tar.gz"
    else
        EVENTLIB_ARCHIVE_URL=https://github.com/libevent/libevent/releases/download/release-${EVENTLIB_BUILD_VERSION}-stable/libevent-${EVENTLIB_BUILD_VERSION}-stable.tar.gz
        info "Downloading EventLib archive ${EVENTLIB_ARCHIVE_URL}"
        if [[ ! -e ${EVENTLIB_ARCHIVE_LOCATION} ]] ; then
            mkdir -p ${EVENTLIB_ARCHIVE_LOCATION}
        fi
        cd ${EVENTLIB_ARCHIVE_LOCATION}
        wget ${EVENTLIB_ARCHIVE_URL}
        cd - >/dev/null
    fi
}

checkZLibArchive(){
    info ""
    if [[ -e "${ZLIB_ARCHIVE_LOCATION}/v${ZLIB_BUILD_VERSION}.tar.gz" ]] ; then
        info "Using ZLib archive ${ZLIB_ARCHIVE_LOCATION}/v${ZLIB_BUILD_VERSION}.tar.gz"
    else
        ZLIB_ARCHIVE_URL=https://github.com/madler/zlib/archive/v${ZLIB_BUILD_VERSION}.tar.gz
        info "Downloading ZLib archive ${ZLIB_ARCHIVE_URL}"
        if [[ ! -e ${ZLIB_ARCHIVE_LOCATION} ]] ; then
            mkdir -p ${ZLIB_ARCHIVE_LOCATION}
        fi
        cd ${ZLIB_ARCHIVE_LOCATION}
        wget ${ZLIB_ARCHIVE_URL}
        cd - >/dev/null
    fi
}

checkXZLibArchive(){
    info ""
    if [[ -e "${XZLIB_ARCHIVE_LOCATION}/xz-${XZLIB_BUILD_VERSION}.tar.gz" ]] ; then
        info "Using XZLib archive ${XZLIB_ARCHIVE_LOCATION}/xz-${XZLIB_BUILD_VERSION}.tar.gz"
    else
        XZLIB_ARCHIVE_URL=https://tukaani.org/xz/xz-${XZLIB_BUILD_VERSION}.tar.gz
        info "Downloading XZLib archive ${ZLIB_ARCHIVE_URL}"
        if [[ ! -e ${XZLIB_ARCHIVE_LOCATION} ]] ; then
            mkdir -p ${XZLIB_ARCHIVE_LOCATION}
        fi
        cd ${XZLIB_ARCHIVE_LOCATION}
        wget ${XZLIB_ARCHIVE_URL}
        cd - >/dev/null
    fi
}

checkTorArchive(){
    info ""
    if [[ -e "${TOR_ARCHIVE_LOCATION}/tor-${TOR_BUILD_VERSION}.tar.gz" ]] ; then
        info "Using Tor archive ${TOR_ARCHIVE_LOCATION}/tor-${TOR_BUILD_VERSION}.tar.gz"
    else
        TOR_ARCHIVE_URL=https://github.com/torproject/tor/archive/tor-${TOR_BUILD_VERSION}.tar.gz
        info "Downloading Tor archive ${ZLIB_ARCHIVE_URL}"
        if [[ ! -e ${TOR_ARCHIVE_LOCATION} ]] ; then
            mkdir -p ${TOR_ARCHIVE_LOCATION}
        fi
        cd ${TOR_ARCHIVE_LOCATION}
        wget ${TOR_ARCHIVE_URL}
        cd - >/dev/null
    fi
}

checkNDKArchive(){
    info ""
    info "Searching Android toolchain file ${ANDROID_TOOLCHAIN_CMAKE}"
    if [[ -e ${ANDROID_TOOLCHAIN_CMAKE} ]] ; then
        info "Found it! :-)"
    else
        warning "Android toolchain file ${ANDROID_TOOLCHAIN_CMAKE} not found!"
        local currentDir=$(pwd)
        if [[ ! -e ${ANDROID_NDK_ARCHIVE_LOCATION} ]] ; then
            mkdir -p ${ANDROID_NDK_ARCHIVE_LOCATION}
        fi
        cd ${ANDROID_NDK_ARCHIVE_LOCATION}
        if [[ -e "${ANDROID_NDK_ARCHIVE}" ]] ; then
            info "Using existing NDK archive"
        else
            ANDROID_NDK_ARCHIVE_URL=https://dl.google.com/android/repository/${ANDROID_NDK_ARCHIVE}
            info "Downloading NDK archive ${ANDROID_NDK_ARCHIVE_URL}"
            wget ${ANDROID_NDK_ARCHIVE_URL}
        fi
        info "Cleanup before extraction"
        rm -rf android-ndk-${ANDROID_NDK_VERSION}
        info "Extracting NDK archive"
        unzip ${ANDROID_NDK_ARCHIVE}
        cd - >/dev/null
    fi
}

# Determine system
# Determine amount of cores:
if [[ "$OSTYPE" == "linux-gnu" ]]; then
    CORES_TO_USE=$(grep -c ^processor /proc/cpuinfo)
    ANDROID_NDK_ARCHIVE=android-ndk-${ANDROID_NDK_VERSION}-linux-x86_64.zip
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    CORES_TO_USE=$(system_profiler SPHardwareDataType | grep "Total Number of Cores" | tr -s " " | cut -d " " -f 6)
    ANDROID_NDK_ARCHIVE=android-ndk-${ANDROID_NDK_VERSION}-darwin-x86_64.zip
#elif [[ "$OSTYPE" == "cygwin" ]]; then
#    # POSIX compatibility layer and Linux environment emulation for Windows
#elif [[ "$OSTYPE" == "msys" ]]; then
#    # Lightweight shell and GNU utilities compiled for Windows (part of MinGW)
#elif [[ "$OSTYPE" == "win32" ]]; then
#    # I'm not sure this can happen.
#elif [[ "$OSTYPE" == "freebsd"* ]]; then
#    CORES_TO_USE=1
else
    CORES_TO_USE=1
fi

FULLBUILD=false
WITH_TOR=false

while getopts a:c:fth? option; do
    case ${option} in
        a) ANDROID_TOOLCHAIN_CMAKE="${OPTARG}";;
        c) CORES_TO_USE="${OPTARG}";;
        f) FULLBUILD=true;;
        t) WITH_TOR=true;;
        h|?) helpMe && exit 0;;
        *) die 90 "invalid option \"${OPTARG}\"";;
    esac
done

# Go to Spectrecoin repository root directory
cd ..

if [[ ! -d ${BUILD_DIR} ]] ; then
    info ""
    info "Creating build directory ${BUILD_DIR}"
    mkdir ${BUILD_DIR}
fi

cd ${BUILD_DIR} || die 1 "Unable to cd into ${BUILD_DIR}"

if ${FULLBUILD} ; then
    info ""
    info "Cleanup leftovers from previous build run"
    rm -rf ./*
fi

checkNDKArchive
checkBoost
checkBerkeleyDBArchive
checkOpenSSLArchive
if ${WITH_TOR} ; then
    checkEventLibArchive
    checkXZLibArchive
    checkZLibArchive
    checkTorArchive
fi

info ""
info "Generating build configuration"
read -r -d '' cmd << EOM
cmake \
    -DANDROID=1 \
    -DCMAKE_ANDROID_API=${ANDROID_API} \
    -DANDROID_PLATFORM=${ANDROID_API} \
    -DCMAKE_ANDROID_ARCH_ABI=${ANDROID_ARCH}-v8a \
    -DCMAKE_TOOLCHAIN_FILE=${ANDROID_TOOLCHAIN_CMAKE} \
    -DANDROID_NDK_ROOT=${ANDROID_NDK_ROOT} \
    -DANDROID_ABI=${ANDROID_ABI} \
    \
    -DBERKELEYDB_ARCHIVE_LOCATION=${BERKELEYDB_ARCHIVE_LOCATION} \
    -DBERKELEYDB_BUILD_VERSION=${BERKELEYDB_BUILD_VERSION} \
    -DBERKELEYDB_BUILD_VERSION_SHORT=${BERKELEYDB_BUILD_VERSION%.*} \
    \
    -DBOOST_ROOT=${BOOST_ROOT} \
    -DBOOST_INCLUDEDIR=${BOOST_INCLUDEDIR} \
    -DBOOST_LIBRARYDIR=${BOOST_LIBRARYDIR} \
    -DBoost_INCLUDE_DIR=${BOOST_INCLUDEDIR} \
    -DBoost_LIBRARY_DIR=${BOOST_LIBRARYDIR} \
    \
    -DBUILD_OPENSSL=ON \
    -DOPENSSL_ARCHIVE_LOCATION=${OPENSSL_ARCHIVE_LOCATION} \
    -DOPENSSL_BUILD_VERSION=${OPENSSL_BUILD_VERSION} \
    -DOPENSSL_API_COMPAT=0x00908000L
EOM
if ${WITH_TOR} ; then
    read -r -d '' cmd << EOM
${cmd} \
    \
    -DEVENTLIB_ARCHIVE_LOCATION=${EVENTLIB_ARCHIVE_LOCATION} \
    -DEVENTLIB_BUILD_VERSION=${EVENTLIB_BUILD_VERSION} \
    -DEVENTLIB_BUILD_VERSION_SHORT=${EVENTLIB_BUILD_VERSION%.*} \
    \
    -DZLIB_ARCHIVE_LOCATION=${ZLIB_ARCHIVE_LOCATION} \
    -DZLIB_BUILD_VERSION=${ZLIB_BUILD_VERSION} \
    -DZLIB_BUILD_VERSION_SHORT=${ZLIB_BUILD_VERSION%.*} \
    \
    -DXZLIB_ARCHIVE_LOCATION=${XZLIB_ARCHIVE_LOCATION} \
    -DXZLIB_BUILD_VERSION=${XZLIB_BUILD_VERSION} \
    -DXZLIB_BUILD_VERSION_SHORT=${XZLIB_BUILD_VERSION%.*} \
    \
    -DTOR_ARCHIVE_LOCATION=${TOR_ARCHIVE_LOCATION} \
    -DTOR_BUILD_VERSION=${TOR_BUILD_VERSION} \
    -DTOR_BUILD_VERSION_SHORT=${TOR_BUILD_VERSION%.*} \
    \
    -DWITH_TOR=ON \
    ..
EOM
else
    read -r -d '' cmd << EOM
${cmd} \
    ..
EOM
fi

echo "=============================================================================="
echo "Executing the following CMake cmd:"
echo "${cmd}"
echo "=============================================================================="
#read a
${cmd}
#read a

info ""
info "Building with ${CORES_TO_USE} cores:"
cmake \
    --build . \
    -- \
    -j "${CORES_TO_USE}"

rtc=$?
info ""
if [[ ${rtc} = 0 ]] ; then
    info "Finished"
else
    error "Finished with return code ${rtc}"
fi
cd "${callDir}" || die 1 "Unable to cd back to where we came from (${callDir})"
