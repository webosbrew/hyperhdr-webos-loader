#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "daemon.h"
#include "log.h"

bool is_elevated()
{
    return (geteuid() == 0);
}

bool is_running(pid_t pid)
{
    return (pid > 0);
}

char *daemon_cmdline(char *args)
{
    char *tmp = (char *)calloc(1, FILENAME_MAX);
    snprintf(tmp, FILENAME_MAX, "/bin/bash -c 'LD_LIBRARY_PATH=%s OPENSSL_armcap=%i %s/%s %s'", DAEMON_PATH, 0, DAEMON_PATH, DAEMON_EXECUTABLE, args);
    return tmp;
}

char *daemon_version_cmdline()
{
    return daemon_cmdline("--version");
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

    if (is_running(service->daemon_pid)) {
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