#pragma once

#include <unistd.h>
#include <glib.h>
#include <pthread.h>

#define SERVICE_NAME "org.webosbrew.hyperhdr.loader.service"
#define DAEMON_PATH "/media/developer/apps/usr/palm/services/org.webosbrew.hyperhdr.loader.service/hyperhdr"
#define DAEMON_EXECUTABLE "hyperhdr"
#define DAEMON_NAME "HyperHDR"

typedef struct {
    char *daemon_version;
    pid_t daemon_pid;
    pthread_t execution_thread;
} service_t;

int service_start(service_t* service);
int service_stop(service_t* service);