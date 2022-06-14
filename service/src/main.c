#include "service.h"
#include "daemon.h"
#include "log.h"

GMainLoop *gmainLoop;

// This is a deprecated symbol present in meta-lg-webos-ndk but missing in
// latest buildroot NDK. It is required for proper public service registration
// before webOS 3.5.
//
// SECURITY_COMPATIBILITY flag present in CMakeList disables deprecation notices, see:
// https://github.com/webosose/luna-service2/blob/b74b1859372597fcd6f0f7d9dc3f300acbf6ed6c/include/public/luna-service2/lunaservice.h#L49-L53
bool LSRegisterPubPriv(const char* name, LSHandle** sh,
    bool public_bus,
    LSError* lserror) __attribute__((weak));

int main()
{
    service_t service = {0};
    LSHandle *handle = NULL;
    LSError lserror;

    service.daemon_pid = 0;
    service.daemon_version = NULL;

    log_init(LOG_NAME);
    INFO("Starting up...");

    log_set_level(Debug);
    LSErrorInit(&lserror);

    // create a GMainLoop
    gmainLoop = g_main_loop_new(NULL, FALSE);

    bool ret = false;

    if (&LSRegisterPubPriv != 0) {
        ret = LSRegisterPubPriv(SERVICE_NAME, &handle, true, &lserror);
    } else {
        ret = LSRegister(SERVICE_NAME, &handle, &lserror);
    }

    if (!ret) {
        ERR("Unable to register on Luna bus: %s", lserror.message);
        goto exit;
    }

    if ((ret = service_init(handle, &service, &lserror)) && !ret ) {
        ERR("Unable to init service: %s", lserror.message);
        goto exit;
    }

    if ((ret = LSGmainAttach(handle, gmainLoop, &lserror)) && !ret ) {
        ERR("Unable to attach main loop: %s", lserror.message);
        goto exit;
    }

    DBG("Going into main loop..");

    // run to check continuously for new events from each of the event sources
    g_main_loop_run(gmainLoop);

    DBG("Main loop quit...");

    DBG("Cleaning up service...");
    daemon_terminate(&service);

exit:
    if (handle) {
        DBG("Unregistering service...");
        LSUnregister(handle, &lserror);
    }

    LSErrorFree(&lserror);

    // Decreases the reference count on a GMainLoop object by one
    g_main_loop_unref(gmainLoop);

    DBG("Service main finished");
    return 0;
}
