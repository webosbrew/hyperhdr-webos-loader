#pragma once

#include <glib.h>

#define SERVICE_NAME "org.webosbrew.hyperhdr.loader.service"
#define HYPERHDR_PATH "/media/developer/apps/usr/palm/services/org.webosbrew.hyperhdr.loader.service/hyperhdr"

typedef struct {
    bool running;
    char *hyperhdr_version;
} service_t;

int service_start(service_t* service);
int service_stop(service_t* service);