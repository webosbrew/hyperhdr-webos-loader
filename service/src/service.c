#include <stdbool.h>
#include <pbnjson.h>
#include "service.h"
#include "daemon.h"
#include "log.h"

bool service_method_start(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    jvalue_ref jobj = jobject_create();
    int res = daemon_start(service);

    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));
    switch (res) {
        case 0:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Daemon started successfully"));
            break;
        case 1:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Precondition fail: Not running elevated!"));
            break;
        case 2:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Daemon is already running"));
            break;
        case 3:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Daemon failed to start, posix_spawn failed"));
            break;
        case 4:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Daemon failed to start, pthread_create failed"));
            break;
        default:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Daemon failed to start, reason: Unknown"));
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
    int res = daemon_stop(service);
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));
    switch (res) {
        case 0:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Daemon stopped successfully"));
            break;
        case 1:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Precondition fail: Not running elevated!"));
            break;
        case 2:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Daemon is not running"));
            break;
        default:
            jobject_set(jobj, j_cstr_to_buffer("status"), jstring_create("Daemon failed to stop, reason: Unknown"));
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
    int res = daemon_version(service);
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));
    jobject_set(jobj, j_cstr_to_buffer("returnCode"), jnumber_create_i32(res));
    switch (res) {
        case 0:
            jobject_set(jobj, j_cstr_to_buffer("version"), jstring_create(service->daemon_version));
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
    jobject_set(jobj, j_cstr_to_buffer("running"), jboolean_create(is_running(service->daemon_pid)));
    jobject_set(jobj, j_cstr_to_buffer("elevated"), jboolean_create(is_elevated()));
    jobject_set(jobj, j_cstr_to_buffer("pid"), jnumber_create_i32(service->daemon_pid));
    jobject_set(jobj, j_cstr_to_buffer("daemon"), jstring_create(DAEMON_NAME));

    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    return true;
}

bool service_method_terminate(LSHandle* sh, LSMessage* msg, void* data)
{
    service_t* service = (service_t*)data;
    LSError lserror;
    LSErrorInit(&lserror);

    WARN("Terminating");

    int res = daemon_terminate(service);

    jvalue_ref jobj = jobject_create();
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(res == 0));

    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    if (res == 0) {
        // Stopping mainloop!
        g_main_loop_quit(gmainLoop);
    } else {
        ERR("Failed to terminate self, daemon was not able to stop");
    }

    return true;
}

LSMethod methods[] = {
    {"start", service_method_start, LUNA_METHOD_FLAGS_NONE },
    {"stop", service_method_stop, LUNA_METHOD_FLAGS_NONE },
    {"status", service_method_status, LUNA_METHOD_FLAGS_NONE },
    {"version", service_method_version, LUNA_METHOD_FLAGS_NONE },
    {"terminate", service_method_terminate, LUNA_METHOD_FLAGS_NONE },
    { 0, 0, 0 }
};

bool service_destroy(LSHandle *handle, service_t *service, LSError *lserror)
{
    DBG("Cleaning up service...");
    daemon_terminate(service);

    if (handle) {
        DBG("Unregistering service...");
        LSUnregister(handle, lserror);
    }

    return true;
}

bool service_init(LSHandle *handle, GMainLoop* loop, service_t *service, LSError *lserror)
{
    bool ret = false;

    if (&LSRegisterPubPriv != 0) {
        ret = LSRegisterPubPriv(SERVICE_NAME, &handle, true, lserror);
    } else {
        ret = LSRegister(SERVICE_NAME, &handle, lserror);
    }

    if (!ret) {
        ERR("Unable to register on Luna bus: %s", lserror->message);
        return false;
    }

    if ((ret = LSRegisterCategory(handle, "/", methods, NULL, NULL, lserror)) && !ret ) {
        ERR("Unable to register category on Luna bus: %s", lserror->message);
        return false;
    }

    if ((ret = LSCategorySetData(handle, "/", service, lserror)) && !ret ) {
        ERR("Unable to set category data on Luna bus: %s", lserror->message);
        return false;
    }

    if ((ret = LSGmainAttach(handle, loop, lserror)) && !ret ) {
        ERR("Unable to attach main loop: %s", lserror->message);
        return false;
    }

    return true;
}