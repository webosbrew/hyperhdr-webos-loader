#pragma once

#include "service.h"

bool is_elevated();
bool is_running(pid_t pid);

char *daemon_cmdline(char *args);
char *daemon_version_cmdline();

int daemon_spawn(pid_t *pid);
int daemon_terminate(service_t *service);
void *execution_task(void *data);

int daemon_start(service_t* service);
int daemon_stop(service_t* service);
int daemon_version(service_t* service);