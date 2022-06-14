#!/bin/sh

SERVICE_NAME="org.webosbrew.hyperhdr.loader.service"
ELEVATION_SCRIPT="/media/developer/apps/usr/palm/services/org.webosbrew.hbchannel.service/elevate-service"

# If elevation script is available, execute it for good measure
if [ -f ${ELEVATION_SCRIPT} ]
then
    ${ELEVATION_SCRIPT} ${SERVICE_NAME}
fi

# Start daemon
luna-send -n 1 "luna://${SERVICE_NAME}/start" '{}' &
