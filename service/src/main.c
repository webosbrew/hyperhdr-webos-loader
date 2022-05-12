#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.h>
#include "service.h"

GMainLoop *gmainLoop;

bool is_elevated()
{
    return (geteuid() == 0);
}

char *hyperhdr_cmdline(char *args)
{
    char *tmp = (char *)calloc(1, FILENAME_MAX);
    snprintf(tmp, FILENAME_MAX, "/bin/bash -c 'LD_LIBRARY_PATH=%s OPENSSL_armcap=%i %s/hyperhdr %s'", HYPERHDR_PATH, 0, HYPERHDR_PATH, args);
    return tmp;
}

char *hyperhdr_start_cmdline()
{
    // Run hyperhdr in background
    return hyperhdr_cmdline("&");
}

char *hyperhdr_version_cmdline()
{
    return hyperhdr_cmdline("--version");
}

int hyperhdr_start(service_t* service)
{
    if (!is_elevated()) {
        return 1;
    } else if (service->running) {
        return 2;
    }

    service->running = true;
    // TODO: system() ftw
    char *command = hyperhdr_start_cmdline();
    int res = system(command);

    if (res != 0) {
        service->running = false;
        return 3;
    }

    return 0;
}

int hyperhdr_stop(service_t* service)
{
    if (!is_elevated()) {
        return 1;
    } else if (!service->running) {
        return 2;
    }

    // TODO: system() ftw
    system("killall -9 hyperhdr");
    service->running = false;

    return 0;
}

int hyperhdr_version(service_t* service)
{
    int res = 0;
    // NOTE: --version call is fine even without root privileges
    if (service->hyperhdr_version == NULL) {
        service->hyperhdr_version = (char *)calloc(FILENAME_MAX, 1);
        if (service->hyperhdr_version == NULL) {
            // Buffer allocation failed
            return 1;
        }

        char *command = hyperhdr_version_cmdline();
        // Spawn process with read-only pipe
        FILE *fp = popen(command, "r");
        if (fp == NULL) {
            // Opening process failed
            res = 2;
        } else {
            int bytes_read = fread(service->hyperhdr_version, 1, FILENAME_MAX, fp);
            if (bytes_read == 0) {
                // Reading process' stdout failed
                res = 3;
            }

            pclose(fp);
        }
    }

    return res;
}

bool service_method_start(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    jvalue_ref jobj = jobject_create();
    int res = hyperhdr_start(service);

    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));
    switch (res) {
        case 0:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("HyperHDR started successfully"));
            break;
        case 1:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Precondition fail: Not running elevated!"));
            break;
        case 2:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("HyperHDR was already running"));
            break;
        case 3:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("HyperHDR failed to start, reason: Unknown"));
            break;
    }
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
    int res = hyperhdr_stop(service);
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));
    switch (res) {
        case 0:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("HyperHDR stopped successfully"));
            break;
        case 1:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Precondition fail: Not running elevated!"));
            break;
        case 2:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("HyperHDR was not running"));
            break;
    }
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
    int res = hyperhdr_version(service);
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));
    jobject_set(jobj, j_cstr_to_buffer("returnCode"), jnumber_create_i32(res));
    switch (res) {
        case 0:
            jobject_set(jobj, j_cstr_to_buffer("version"), jstring_create(service->hyperhdr_version));
            break;
        default:
            jobject_set(jobj, j_cstr_to_buffer("version"), jstring_create("Error while fetching"));
            break;
    }
    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    return true;
}

bool service_method_status(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    jvalue_ref jobj = jobject_create();
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(true));
    jobject_set(jobj, j_cstr_to_buffer("running"), jboolean_create(service->running));
    jobject_set(jobj, j_cstr_to_buffer("elevated"), jboolean_create(is_elevated()));

    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    return true;
}

bool service_method_terminate(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    jvalue_ref jobj = jobject_create();
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(true));

    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    // Stopping mainloop!
    g_main_loop_quit(gmainLoop);

    return true;
}

LSMethod methods[] = {
    {"start", service_method_start},
    {"stop", service_method_stop},
    {"status", service_method_status},
    {"version", service_method_version},
    {"terminate", service_method_terminate},
};

int main(int argc, char* argv[])
{
    service_t service;
    LSHandle *handle = NULL;
    LSError lserror;

    service.running = FALSE;
    service.hyperhdr_version = NULL;

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
