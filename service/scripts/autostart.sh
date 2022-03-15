#!/bin/sh

# Elevate service to run as root
# * needs to be done before calling into the service
/media/developer/apps/usr/palm/services/org.webosbrew.hbchannel.service/elevate-service org.webosbrew.hyperion.ng.loader.service

# Start hyperiond
luna-send -n 1 'luna://org.webosbrew.hyperion.ng.loader.service/start' '{}'
