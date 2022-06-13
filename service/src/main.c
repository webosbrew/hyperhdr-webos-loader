#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <glib-object.h>
#include <luna-service2/lunaservice.h>
#include <pbnjson.h>
#include "service.h"
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

bool is_elevated()
{
    return (geteuid() == 0);
}

char *daemon_cmdline(char *args)
{
    char *tmp = (char *)calloc(1, FILENAME_MAX);
    snprintf(tmp, FILENAME_MAX, "/bin/bash -c 'LD_LIBRARY_PATH=%s OPENSSL_armcap=%i %s/%s %s'", DAEMON_PATH, 0, DAEMON_PATH, DAEMON_EXECUTABLE, args);
    return tmp;
}

int daemon_spawn(pid_t *pid)
{
    int res = 0;

    char *env_library_path;
    char *env_armcap;
    char *application_executable_path;

    asprintf(&env_library_path, "LD_LIBRARY_PATH=%s", DAEMON_PATH);
    asprintf(&env_armcap, "OPENSSL_armcap=%d", 0);
    asprintf(&application_executable_path, "%s/%s", DAEMON_PATH, DAEMON_EXECUTABLE);

    char *env_vars[] = {env_library_path, env_armcap, "HOME=/home/root", NULL};
    char *argv[] = {application_executable_path, NULL};
    
    res = posix_spawn(pid, application_executable_path, NULL, NULL, argv, env_vars);
    DBG("pid=%d, application_path=%s, env={%s,%s}",
            *pid, application_executable_path, env_library_path, env_armcap);

    free(env_library_path);
    free(env_armcap);
    free(application_executable_path);

    return res;
}

int daemon_terminate(service_t *service)
{
    int res = 0;

    if (service->daemon_pid > 0) {
        res = kill(service->daemon_pid, SIGTERM);
        if (res != 0) {
            ERR("kill failed, res=%d", res);
            return 1;
        }
    } else {
        ERR("PID not set: %d", service->daemon_pid);
        return 2;
    }


    if (service->execution_thread != (pthread_t) NULL) {
        res = pthread_join(service->execution_thread, NULL);
        if (res != 0) {
            ERR("pthread_join failed, res=%d", res);
            return 3;
        }
        service->execution_thread = (pthread_t) NULL;
    }
    else {
        ERR("Execution thread is not up");
        return 4;
    }

    return res;
}

void *execution_task(void *data)
{
    int res = 0;
    int status = 0;
    bool run_loop = true;

    service_t *service = (service_t *)data;

    do {
        if (service->daemon_pid <= 0) {
            WARN("Invalid daemon PID: %d - exiting execution loop");
            break;
        }
        res = waitpid(service->daemon_pid, &status, WNOHANG);

        if (res == service->daemon_pid) {
            if (WIFEXITED(status)) {
                INFO("Child status: exited, status=%d\n", WEXITSTATUS(status));
                run_loop = false;
            } else if (WIFSIGNALED(status)) {
                INFO("Child status: killed by signal %d\n", WTERMSIG(status));
                run_loop = false;
            } else if (WIFSTOPPED(status)) {
                INFO("Child status: stopped by signal %d\n", WSTOPSIG(status));
            } else if (WIFCONTINUED(status)) {
                INFO("Child status: continued\n");
            }
        } else if (res == -1) {
            ERR("waitpid: res=%d", res);
            run_loop = false;
        } else {
            usleep(500 * 1000); // 500ms
        }
    } while (run_loop);

    service->daemon_pid = 0;
    return NULL;
}

char *daemon_version_cmdline()
{
    return daemon_cmdline("--version");
}

bool is_running(pid_t pid)
{
    return (pid > 0);
}

int daemon_start(service_t* service)
{
    int res = 0;
    if (!is_elevated()) {
        ERR("Not elevated");
        return 1;
    } else if (is_running(service->daemon_pid)) {
        ERR("Daemon already running");
        return 2;
    }

    res = daemon_spawn(&service->daemon_pid);
    DBG("daemon_start -> PID=%d", service->daemon_pid);

    if (res != 0) {
        ERR("Failed daemon_start with res=%d", res);
        return 3;
    }

    res = pthread_create(&service->execution_thread, NULL, execution_task, service);
    if (res != 0) {
        ERR("pthread_create failed, res=%d", res);
        return 4;
    }

    return 0;
}

int daemon_stop(service_t* service)
{
    int res = 0;

    if (!is_elevated()) {
        ERR("Not elevated");
        return 1;
    } else if (!is_running(service->daemon_pid)) {
        ERR("Daemon not running");
        return 2;
    }

    res = daemon_terminate(service);
    if (res != 0) {
        ERR("Daemon termination failed, res=%d", res);
        return 3;
    }

    return 0;
}

int daemon_version(service_t* service)
{
    int res = 0;
    // NOTE: --version call is fine even without root privileges
    if (service->daemon_version == NULL) {
        service->daemon_version = (char *)calloc(FILENAME_MAX, 1);
        if (service->daemon_version == NULL) {
            ERR("Failed version buf allocation");
            return 1;
        }

        char *command = daemon_version_cmdline();
        // Spawn process with read-only pipe
        FILE *fp = popen(command, "r");
        if (fp == NULL) {
            ERR("popen failed");
            res = 2;
        } else {
            int bytes_read = fread(service->daemon_version, 1, FILENAME_MAX, fp);
            if (bytes_read == 0) {
                ERR("Reading process' stdout failed");
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

bool service_method_terminate(LSHandle* sh, LSMessage* msg, void* data __attribute__((unused)))
{
    LSError lserror;
    LSErrorInit(&lserror);

    WARN("Terminating");

    jvalue_ref jobj = jobject_create();
    jobject_set(jobj, j_cstr_to_buffer("returnValue"), jboolean_create(true));

    LSMessageReply(sh, msg, jvalue_tostring_simple(jobj), &lserror);

    j_release(&jobj);

    // Stopping mainloop!
    g_main_loop_quit(gmainLoop);

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

int main()
{
    service_t service = {0};
    LSHandle *handle = NULL;
    LSError lserror;

    service.daemon_pid = 0;
    service.daemon_version = NULL;

    log_init();
    log_set_level(Debug);
    LSErrorInit(&lserror);

    // create a GMainLoop
    gmainLoop = g_main_loop_new(NULL, FALSE);

    bool registered = false;

    if (&LSRegisterPubPriv != 0) {
        registered = LSRegisterPubPriv(SERVICE_NAME, &handle, true, &lserror);
    } else {
        registered = LSRegister(SERVICE_NAME, &handle, &lserror);
    }

    if (!registered) {
        ERR("Unable to register on Luna bus: %s", lserror.message);
        LSErrorFree(&lserror);
        return -1;
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
