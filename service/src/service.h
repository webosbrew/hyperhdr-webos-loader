#pragma once

#include <glib.h>

#define SERVICE_NAME "org.webosbrew.hyperion.ng.loader.service"
#define HYPERION_PATH "/media/developer/apps/usr/palm/services/org.webosbrew.hyperion.ng.loader.service/hyperion"

typedef struct {
    bool running;
} service_t;

int service_start(service_t* service);
int service_stop(service_t* service);