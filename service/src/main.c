#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <stdio.h>
#include <glib-object.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.h>
#include "service.h"

char *hyperiond_commandline()
{
    char *tmp = (char *)calloc(1, FILENAME_MAX);
    snprintf(tmp, FILENAME_MAX, "/bin/bash -c 'LD_LIBRARY_PATH=%s OPENSSL_armcap=%i %s/hyperiond &'", HYPERION_PATH, 0, HYPERION_PATH);
    return tmp;
}

int service_start(service_t* service)
{
    if (service->running) {
        return 1;
    }

    service->running = true;
    // TODO: system() ftw
    char *command = hyperiond_commandline();
    int res = system(command);

    if (res != 0) {
        service->running = false;
        return res;
    }

    return 0;
}

int service_stop(service_t* service)
{
    if (!service->running) {
        return 1;
    }

    // TODO: system() ftw
    system("killall -9 hyperiond");
    service->running = false;

    return 0;
}

bool service_method_start(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    jvalue_ref jobj = jobject_create();
    int res = service_start(service);
#if 1
    char *cmdline = hyperiond_commandline();
    jobject_set(jobj, j_cstr_to_buffer("cmdline"), jstring_create(cmdline));
    free(cmdline);
#endif
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));
    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    return true;
}

bool service_method_stop(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    jvalue_ref jobj = jobject_create();
    int res = service_stop(service);
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));
    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    return true;
}

bool service_method_is_started(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    jvalue_ref jobj = jobject_create();
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(true));
    jobject_set(jobj, j_cstr_to_buffer("started"), jboolean_create(true));
    // TODO: Fetch actual execution status
    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    return true;
}

bool service_method_version(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    jvalue_ref jobj = jobject_create();
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(true));
    jobject_set(jobj, j_cstr_to_buffer("version"), jstring_create("v2.0.12"));
    // TODO: Fetch version from `hyperiond --version`

    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    return true;
}

LSMethod methods[] = {
    {"start", service_method_start},
    {"stop", service_method_stop},
    {"isStarted", service_method_is_started},
    {"version", service_method_version}
};

int main(int argc, char* argv[])
{
    GMainLoop *gmainLoop;
    service_t service;
    LSHandle *handle = NULL;
    LSError lserror;

    service.running = FALSE;

    LSErrorInit(&lserror);

    // create a GMainLoop
    gmainLoop = g_main_loop_new(NULL, FALSE);

    if(!LSRegister(SERVICE_NAME, &handle, &lserror)) {
        LSErrorFree(&lserror);
        return 0;
    }

    LSRegisterCategory(handle, "/", methods, NULL, NULL, &lserror);
    LSCategorySetData(handle, "/", &service, &lserror);

    LSGmainAttach(handle, gmainLoop, &lserror);

    // run to check continuously for new events from each of the event sources
    g_main_loop_run(gmainLoop);
    // Decreases the reference count on a GMainLoop object by one
    g_main_loop_unref(gmainLoop);

    return 0;
}
