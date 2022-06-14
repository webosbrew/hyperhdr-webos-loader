#include "service.h"
#include "daemon.h"
#include "log.h"

GMainLoop *gmainLoop;

void signal_handler(int signum)
{
    INFO("SIGNAL=%d detected, stopping...", signum);
    g_main_loop_quit(gmainLoop);
}

int main()
{
    service_t service = {0};
    LSHandle *handle = NULL;
    LSError lserror;

    service.daemon_pid = 0;
    service.daemon_version = NULL;
    service.power_paused = false;

    log_init(LOG_NAME);
    INFO("Starting up...");

    log_set_level(Debug);
    LSErrorInit(&lserror);

    // Register exit signal handler
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);

    // create a GMainLoop
    gmainLoop = g_main_loop_new(NULL, FALSE);

    bool ret = false;

    if ((ret = service_init(handle, gmainLoop, &service, &lserror)) && !ret ) {
        ERR("Unable to init service: %s", lserror.message);
        goto exit;
    }

    DBG("Going into main loop..");

    // run to check continuously for new events from each of the event sources
    g_main_loop_run(gmainLoop);

    DBG("Main loop quit...");

exit:
    if ((ret = service_destroy(handle, &service, &lserror)) && !ret) {
        WARN("Destroying service properly failed: %s", lserror.message);
    }

    LSErrorFree(&lserror);

    // Decreases the reference count on a GMainLoop object by one
    g_main_loop_unref(gmainLoop);

    DBG("Service main finished");
    return 0;
}
