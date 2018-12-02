#!/bin/bash
# ===========================================================================
#
# This script uses macdeployqt to add the required libs to spectrecoin package.
# - Fixes non @executable openssl references.
# - Replaces openssl 1.0.0 references with 1.1
#
# ===========================================================================

# Store path from where script was called, determine own location
# and source helper content from there
callDir=$(pwd)
ownLocation="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd ${ownLocation}
. ./include/helpers_console.sh

# Go to Spectrecoin repository root directory
cd ..

if [ -z "${QT_PATH}" ] ; then
    QT_PATH=~/Qt/5.9.6/clang_64
    warning "QT_PATH not set, using '${QT_PATH}'"
else
    info "QT_PATH: ${QT_PATH}"
fi
if [ -z "${OPENSSL_PATH}" ] ; then
    OPENSSL_PATH=/usr/local/Cellar/openssl@1.1/1.1.1
    warning "OPENSSL_PATH not set, using '${OPENSSL_PATH}'"
else
    info "OPENSSL_PATH: ${OPENSSL_PATH}"
fi

info "Cleanup previous build artifacts"
if [ -e Spectrecoin.dmg ] ; then
    rm -f Spectrecoin.dmg
fi
if [ -e src/bin/spectrecoin.dmg ] ; then
    rm -f src/bin/spectrecoin.dmg
fi

info "Call macdeployqt:"
${QT_PATH}/bin/macdeployqt src/bin/Spectrecoin.app

info "Remove openssl 1.0.0 libs:"
rm -v src/bin/spectrecoin.app/Contents/Frameworks/libssl.1.0.0.dylib
rm -v src/bin/spectrecoin.app/Contents/Frameworks/libcrypto.1.0.0.dylib

info "Replace openssl 1.0.0 lib references with 1.1:"
for f in src/bin/spectrecoin.app/Contents/Frameworks/*.dylib ; do
    install_name_tool -change @executable_path/../Frameworks/libssl.1.0.0.dylib @executable_path/../Frameworks/libssl.1.1.dylib ${f};
    install_name_tool -change @executable_path/../Frameworks/libcrypto.1.0.0.dylib @executable_path/../Frameworks/libcrypto.1.1.dylib ${f};
done


info "install_name_tool -change $OPENSSL_PATH/lib/libcrypto.1.1.dylib @executable_path/../Frameworks/libcrypto.1.1.dylib src/bin/spectrecoin.app/Contents/Frameworks/libssl.1.1.dylib ..."
install_name_tool -change ${OPENSSL_PATH}/lib/libcrypto.1.1.dylib @executable_path/../Frameworks/libcrypto.1.1.dylib src/bin/spectrecoin.app/Contents/Frameworks/libssl.1.1.dylib
otool -l src/bin/spectrecoin.app/Contents/Frameworks/libssl.1.1.dylib | grep dylib


info "Please check for non included lib references:"
for f in src/bin/spectrecoin.app/Contents/Frameworks/*.dylib ; do
    otool -l ${f} | grep dylib | grep -v @
done


info "Create dmg package:"
${QT_PATH}/bin/macdeployqt src/bin/Spectrecoin.app -dmg
mv src/bin/Spectrecoin.dmg Spectrecoin.dmg
