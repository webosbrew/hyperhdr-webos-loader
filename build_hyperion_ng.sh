#!/bin/sh
# Usage:
# TOOLCHAIN_DIR=<path_to_webos_buildroot_toolchain> ./build_hyperion_ng.sh

PATCH_LIBRT="yes"

HYPERION_NG_REPO="${HYPERION_NG_REPO:-https://github.com/hyperion-project/hyperion.ng}"
HYPERION_NG_BRANCH="${HYPERION_NG_BRANCH:-master}"

# Toolchain params - No changes needed below this line
TOOLCHAIN_DIR=${TOOLCHAIN_DIR:-$HOME/arm-webos-linux-gnueabi_sdk-buildroot}
TOOLCHAIN_ENV_FILE="${TOOLCHAIN_DIR}/environment-setup"
TOOLCHAIN_CMAKE_FILE="${TOOLCHAIN_DIR}/share/buildroot/toolchainfile.cmake"

EXEC_FILE=`readlink -f $0`
EXEC_DIR=`dirname ${EXEC_FILE}`
# Dir to copy hyperion.ng build artifacts to
TARGET_DIR="${EXEC_DIR}/hyperion"

HYPERION_NG_DIR="${EXEC_DIR}/hyperion.ng"
BUILD_DIR_NATIVE="${HYPERION_NG_DIR}/build-x86x64"
BUILD_DIR_CROSS="${HYPERION_NG_DIR}/build-cross"

DEPENDENCIES="libpng16.so.16 libjpeg.so.8 libcrypto.so.1.1 libz.so.1 libssl.so.1.1 libQt5Sql.so.5.15.2 libpcre2-16.so.0 libQt5Gui.so.5 libQt5Network.so.5 libQt5Widgets.so.5 libk5crypto.so.3 libatomic.so.1 libQt5Core.so.5 libkrb5support.so.0 libcom_err.so.3 libstdc++.so.6 libkrb5.so.3 libQt5Sql.so.5 libgssapi_krb5.so.2 libQt5SerialPort.so.5 libQt5Sql.so.5.15 libusb-1.0.so.0"

if [ ! -d $HYPERION_NG_DIR ]
then
  echo ":: Cloning hyperion.ng from repo '$HYPERION_NG_REPO', branch: '$HYPERION_NG_BRANCH'"
  git clone --recursive --branch $HYPERION_NG_BRANCH $HYPERION_NG_REPO $HYPERION_NG_DIR || { echo "[-] Cloning git repo failed"; exit 1; }
  
  if [ ! -z $PATCH_LIBRT ]
  then
    echo "* Patching to force-link librt"
    sed -i -e 's/hidapi-libusb)$/rt hidapi-libusb)/g w /dev/stdout' hyperion.ng/libsrc/leddevice/CMakeLists.txt
  fi
fi

# Native build to have flatc compiler
if [ ! -d ${BUILD_DIR_NATIVE} ]
then
  mkdir ${BUILD_DIR_NATIVE}
fi

pushd ${BUILD_DIR_NATIVE}
cmake  .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_FLATBUF_SERVER=OFF \
  -DENABLE_DISPMANX=OFF \
  -DENABLE_FB=OFF \
  -DENABLE_V4L2=OFF \
  -DENABLE_X11=OFF \
  -DENABLE_XCB=OFF \
  -DENABLE_BOBLIGHT_SERVER=OFF \
  -DENABLE_CEC=OFF \
  -DENABLE_DEV_NETWORK=OFF \
  -DENABLE_DEV_SERIAL=OFF \
  -DENABLE_DEV_TINKERFORGE=OFF \
  -DENABLE_DEV_USB_HID=OFF \
  -DENABLE_EFFECTENGINE=OFF \
  -DENABLE_REMOTE_CTL=OFF \
  -DENABLE_QT=OFF \
  -DENABLE_FORWARDER=OFF \
  -DENABLE_DEV_SPI=OFF \
  -DENABLE_MDNS=OFF \
  -DENABLE_FLATBUF_CONNECT=ON \
  -DENABLE_PROTOBUF_SERVER=OFF \
  -Wno-dev || { echo "[-] Native build -CONFIG- failed"; exit 1; }

make -j9 || { echo "[-] Native build -MAKE- failed"; exit 1; }
popd

# Target cross build
# source $WEBOS_ENV_FILE

if [ ! -d ${BUILD_DIR_CROSS} ]
then
  mkdir ${BUILD_DIR_CROSS}
fi

pushd ${BUILD_DIR_CROSS}
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_CMAKE_FILE} \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPLATFORM=rpi \
  -DHYPERION_LIGHT=ON \
  -DENABLE_QT=OFF \
  -DENABLE_EFFECTENGINE=OFF \
  -DENABLE_JSONCHECKS=OFF \
  -DENABLE_DEV_SERIAL=ON \
  -DENABLE_DEV_USB_HID=ON \
  -DENABLE_DEV_WS281XPWM=OFF \
  -DENABLE_DEV_TINKERFORGE=ON \
  -DENABLE_MDNS=ON \
  -DENABLE_DEPLOY_DEPENDENCIES=OFF \
  -DENABLE_BOBLIGHT_SERVER=ON \
  -DENABLE_FLATBUF_SERVER=ON \
  -DENABLE_PROTOBUF_SERVER=OFF \
  -DENABLE_FORWARDER=ON \
  -DENABLE_FLATBUF_CONNECT=ON \
  -DIMPORT_FLATC=${BUILD_DIR_NATIVE}/flatc_export.cmake || { echo "[-] Cross build -CONFIG- failed"; exit 1; }

make -j9 || { echo "[-] Native build -MAKE- failed"; exit 1; }
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
cp -ra ${BUILD_DIR_CROSS}/bin/* ${TARGET_DIR}/

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
