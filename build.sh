#!/bin/sh

# Toolchain params
TOOLCHAIN_DIR=${TOOLCHAIN_DIR:-$HOME/arm-webos-linux-gnueabi_sdk-buildroot}
TOOLCHAIN_ENV_FILE=${TOOLCHAIN_DIR}/environment-setup

EXEC_FILE=`readlink -f $0`
EXEC_DIR=`dirname ${EXEC_FILE}`
SERVICE_DIR=${EXEC_DIR}/service

echo "* Using toolchain dir: ${TOOLCHAIN_DIR}"

echo "* Activating toolchain env"
source ${TOOLCHAIN_ENV_FILE} || exit 1

npm run clean || exit 1

echo ":: Frontend ::"
npm run build || exit 1

echo ":: Service ::"
npm run build-service || exit 1

echo ":: HyperHDR ::"
mkdir -p ${EXEC_DIR}/dist/service/hyperhdr
cp -r ${EXEC_DIR}/hyperhdr/* ${EXEC_DIR}/dist/service/hyperhdr/ || exit 1

echo ":: Ensure executable bit set ::"
for file in autostart.sh loader_service start_hyperhdr
do
  FILE="${EXEC_DIR}/dist/service/${file}"
  echo "=> ${FILE}"
  chmod +x ${FILE}
done

for file in hyperhdr flatc flathash
do
  FILE="${EXEC_DIR}/dist/service/hyperhdr/${file}"
  echo "=> ${FILE}"
  chmod +x ${FILE}
done

echo ":: Copy HDR LUT"
unxz -dc ${EXEC_DIR}/resources/flat_lut_lin_tables.3d.xz > ${EXEC_DIR}/dist/service/hyperhdr/flat_lut_lin_tables.3d

echo ":: Package into IPK ::"
npm run package || exit 1
