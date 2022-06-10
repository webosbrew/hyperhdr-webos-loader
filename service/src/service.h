#pragma once

#include <unistd.h>
#include <glib.h>
#include <pthread.h>

#define SERVICE_NAME "org.webosbrew.hyperhdr.loader.service"
#define HYPERHDR_PATH "/media/developer/apps/usr/palm/services/org.webosbrew.hyperhdr.loader.service/hyperhdr"

typedef struct {
    char *hyperhdr_version;
    pid_t daemon_pid;
    pthread_t execution_thread;
} service_t;

int service_start(service_t* service);
int service_stop(service_t* service);