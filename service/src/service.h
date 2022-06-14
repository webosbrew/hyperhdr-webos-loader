#pragma once

#include <unistd.h>
#include <glib.h>
#include <pthread.h>
#include <luna-service2/lunaservice.h>

#define LOG_NAME "hyperhdr-loader"
#define SERVICE_NAME "org.webosbrew.hyperhdr.loader.service"
#define DAEMON_PATH "/media/developer/apps/usr/palm/services/org.webosbrew.hyperhdr.loader.service/hyperhdr"
#define DAEMON_EXECUTABLE "hyperhdr"
#define DAEMON_NAME "HyperHDR"

// This is a deprecated symbol present in meta-lg-webos-ndk but missing in
// latest buildroot NDK. It is required for proper public service registration
// before webOS 3.5.
//
// SECURITY_COMPATIBILITY flag present in CMakeList disables deprecation notices, see:
// https://github.com/webosose/luna-service2/blob/b74b1859372597fcd6f0f7d9dc3f300acbf6ed6c/include/public/luna-service2/lunaservice.h#L49-L53
bool LSRegisterPubPriv(const char* name, LSHandle** sh,
    bool public_bus,
    LSError* lserror) __attribute__((weak));

// Global from main.c
extern GMainLoop *gmainLoop;

typedef struct {
    char *daemon_version;
    pid_t daemon_pid;
    pthread_t execution_thread;
} service_t;

int service_start(service_t* service);
int service_stop(service_t* service);
bool service_init(LSHandle *handle, GMainLoop* loop, service_t *service, LSError *lserror);
bool service_destroy(LSHandle *handle, service_t *service, LSError *lserror);