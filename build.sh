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

echo ":: Hyperion.NG ::"
mkdir -p ${EXEC_DIR}/dist/service/hyperion
cp -r ${EXEC_DIR}/hyperion/* ${EXEC_DIR}/dist/service/hyperion/ || exit 1

echo ":: Ensure executable bit set ::"
for file in autostart.sh loader_service start_hyperiond
do
  FILE="${EXEC_DIR}/dist/service/${file}"
  echo "=> ${FILE}"
  chmod +x ${FILE}
done

for file in hyperiond hyperion-remote flatc flathash
do
  FILE="${EXEC_DIR}/dist/service/hyperion/${file}"
  echo "=> ${FILE}"
  chmod +x ${FILE}
done

echo ":: Package into IPK ::"
npm run package || exit 1