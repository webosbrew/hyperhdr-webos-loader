#!/bin/sh
# Usage:
# TOOLCHAIN_DIR=<path_to_webos_buildroot_toolchain> ./build_hyperhdr.sh

HYPERHDR_REPO="${HYPERHDR_REPO:-https://github.com/awawa-dev/HyperHDR}"
HYPERHDR_BRANCH="${HYPERHDR_BRANCH:-master}"

# Toolchain params - No changes needed below this line
TOOLCHAIN_DIR=${TOOLCHAIN_DIR:-$HOME/arm-webos-linux-gnueabi_sdk-buildroot}
TOOLCHAIN_ENV_FILE="${TOOLCHAIN_DIR}/environment-setup"
TOOLCHAIN_CMAKE_FILE="${TOOLCHAIN_DIR}/share/buildroot/toolchainfile.cmake"

EXEC_FILE=`readlink -f $0`
EXEC_DIR=`dirname ${EXEC_FILE}`
# Dir to copy hyperhdr build artifacts to
TARGET_DIR="${EXEC_DIR}/hyperhdr"

HYPERHDR_REPO_DIR="${EXEC_DIR}/hyperhdr-repo"
BUILD_DIR="${HYPERHDR_REPO_DIR}/build"

DEPENDENCIES="libpng16.so.16 libjpeg.so.8 libcrypto.so.1.1 libz.so.1 libssl.so.1.1 libQt5Sql.so.5.15.2 libpcre2-16.so.0 libQt5Gui.so.5 libQt5Network.so.5 libQt5Widgets.so.5 libk5crypto.so.3 libatomic.so.1 libQt5Core.so.5 libkrb5support.so.0 libcom_err.so.3 libstdc++.so.6 libkrb5.so.3 libQt5Sql.so.5 libgssapi_krb5.so.2 libQt5SerialPort.so.5 libQt5Sql.so.5.15 libusb-1.0.so.0"

if ! command -v flatc &> /dev/null
then
    echo "flatc (flatbuffer compiler) could not be found"
    exit
fi

if [ ! -d $HYPERHDR_REPO_DIR ]
then
  echo ":: Cloning HyperHDR from repo '$HYPERHDR_REPO', branch: '$HYPERHDR_BRANCH'"
  git clone --recursive --branch $HYPERHDR_BRANCH $HYPERHDR_REPO $HYPERHDR_REPO_DIR || { echo "[-] Cloning git repo failed"; exit 1; }
fi

if [ ! -d ${BUILD_DIR} ]
then
  mkdir ${BUILD_DIR}
fi

pushd ${BUILD_DIR}
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_CMAKE_FILE} \
  -DCMAKE_BUILD_TYPE=Release \
  -DPLATFORM=linux \
  -DENABLE_BOBLIGHT=OFF \
  -DENABLE_SPIDEV=OFF \
  -DENABLE_V4L2=OFF \
  -DENABLE_X11=OFF \
  -DENABLE_PIPEWIRE=OFF \
  -DENABLE_SOUNDCAPLINUX=OFF \
  -DENABLE_CEC=OFF \
  -DENABLE_PROTOBUF=OFF \
  -DENABLE_FRAMEBUFFER=OFF \
  || { echo "[-] Build -CONFIG- failed"; exit 1; }

make -j9 || { echo "[-] Build -MAKE- failed"; exit 1; }
popd

if [ -d $TARGET_DIR ]
then
  echo ":: Empyting target build artifacts dir"
  rm -rf $TARGET_DIR/*
else
  echo ":: Creating build artifacts dir"
  mkdir $TARGET_DIR
fi

echo ":: Copying build artifacts"
cp -ra ${BUILD_DIR}/bin/* ${TARGET_DIR}/

echo ":: Copying dependencies from toolchain sysroot"
for fname in ${DEPENDENCIES}
do
  find ${TOOLCHAIN_DIR}/arm-webos-linux-gnueabi/sysroot/ -name $fname -exec cp {} ${TARGET_DIR}/ \;
done

mkdir -p ${TARGET_DIR}/sqldrivers
mkdir -p ${TARGET_DIR}/imageformats
cp ${TOOLCHAIN_DIR}/arm-webos-linux-gnueabi/sysroot/usr/lib/qt/plugins/sqldrivers/libqsqlite.so ${TARGET_DIR}/sqldrivers/
cp ${TOOLCHAIN_DIR}/arm-webos-linux-gnueabi/sysroot/usr/lib/qt/plugins/imageformats/libqico.so ${TARGET_DIR}/imageformats/
cp ${TOOLCHAIN_DIR}/arm-webos-linux-gnueabi/sysroot/usr/lib/qt/plugins/imageformats/libqjpeg.so ${TARGET_DIR}/imageformats/

echo "[+] Success"
