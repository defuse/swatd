/*
 * swatd.c - Run scripts when you are being raided by the police.
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#define DEFAULT_CONFIG "/etc/swatd/swatd.conf"
#define MAX_SCRIPTS 100
#define DEFAULT_CHECK_INTERVAL 30

typedef struct SwatConfig {
    char *execute;
    char **scripts;
    int script_count;
    int failure_count;
} config_t;

typedef struct SensorState {
    char *command;
    int last;
    int failed;
} sensor_t;

void printUsage(void);
void becomeDaemon(void);
void loadConfig(config_t *config, const char *path);
void monitor(config_t *config);
void runCommand(config_t *config);
void logError(const char *msg, ...);
void logInfo(const char *msg, ...);
void strip(char *str);
int startsWith(const char *prefix, const char *s);
void catch_signal(int signal);
void writePID(const char *path);

int use_syslog = 0;
int check_interval = DEFAULT_CHECK_INTERVAL;

int main(int argc, char **argv)
{
    int c = 0;
    int become_daemon = 1;
    int config_loaded = 0;
    config_t config;
    char *pidfile = NULL;

    opterr = 0;
    while ((c = getopt(argc, argv, "sc:p:")) != -1) {
        switch (c) {
            case 's':
                become_daemon = 0;
                break;
            case 'c':
                loadConfig(&config, optarg);
                config_loaded = 1;
                break;
            case 'p':
                pidfile = malloc(strlen(optarg) + 1);
                strcpy(pidfile, optarg);
                break;
            case 'h':
                printUsage();
                exit(0);
                break;
            case '?':
                printUsage();
                exit(EXIT_FAILURE);
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
    
    if (signal(SIGTERM, catch_signal) == SIG_ERR) {
        logError("Error while setting SIGTERM handler.\n");
        exit(EXIT_FAILURE);
    }

    if (become_daemon) {
        becomeDaemon();
    }

    if (pidfile != NULL) {
        writePID(pidfile);
        free(pidfile);
        pidfile = NULL;
    }


    if (!config_loaded) {
        loadConfig(&config, DEFAULT_CONFIG);
    }

    monitor(&config);

    exit(EXIT_SUCCESS);
}

void printUsage(void) 
{
    printf("SWATd - Run scripts when you are being raided by the police.\n");
    printf("  -c CONFIG\t\tUse config file CONFIG.\n");
    printf("  -s\t\t\tDon't fork.\n");
    printf("  -p\t\t\tPID file.\n");
    printf("  -h\t\t\tHelp menu.\n");
}

void becomeDaemon(void)
{
    pid_t pid, sid;
    pid = fork();

    if (pid < 0) {
        /* error */
        logError("Fork failed.\n");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        /* we are the parent */
        exit(EXIT_SUCCESS);
    }

    umask(0);

    openlog("SWATd", LOG_NOWAIT | LOG_PID, LOG_USER);
    use_syslog = 1;
    logInfo("SWATd started.\n");

    sid = setsid();
    if (sid < 0) {
        logError("Could not create process group\n");
        exit(EXIT_FAILURE);
    }

    if (chdir("/") < 0) {
        logError("Could not change working directory to /\n");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void loadConfig(config_t *config, const char *path)
{
    char line[2048];
    FILE *fp;

    /* Don't run if the config file is world writable. */
    struct stat stat_buf;
    if (stat(path, &stat_buf) == -1) {
        logError("Could not stat() %s\n", path);
        exit(EXIT_FAILURE);
    }
    if (stat_buf.st_mode & 2) {
        logError("Config file %s is world writable. This is dangerous.\n", path);
        exit(EXIT_FAILURE);
    }
    
    
    fp = fopen(path, "r");
    if (fp == NULL) {
        logError("Could not open config file %s\n", path);
        exit(EXIT_FAILURE);
    }

    config->scripts = malloc(sizeof(char *) * MAX_SCRIPTS);
    config->script_count = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        strip(line);
        if (startsWith("threshold:", line)) {
            sscanf(line + strlen("threshold:"), "%d", &config->failure_count);
        }
        else if (startsWith("interval:", line)) {
            sscanf(line + strlen("interval:"), "%d", &check_interval);
        }
        else if (startsWith("execute:", line)) {
            char *cmd = line + strlen("execute:");
            strip(cmd);
            config->execute = malloc(strlen(cmd) + 1);
            strcpy(config->execute, cmd);
        } else if (strlen(line) > 0 && config->script_count < MAX_SCRIPTS) {
            config->scripts[config->script_count] = malloc(strlen(line) + 1);
            strcpy(config->scripts[config->script_count], line);
            config->script_count++;
        }
    }

    if (config->script_count == MAX_SCRIPTS) {
        logError("Too many scripts.\n");
    }

    if (ferror(fp)) {
        logError("Error while reading config file %s\n", path);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
}

void monitor(config_t *config)
{
    int i, retval, failed, ran;

    int sensor_count = config->script_count;
    sensor_t *sensors = malloc(sensor_count * sizeof(sensor_t));

    for (i = 0; i < sensor_count; i++) {
        sensors[i].command = config->scripts[i];
        sensors[i].last = -1;
        sensors[i].failed = 0;
    }

    failed = 0;
    ran = 0;

    while (1) {
        sleep(check_interval);

        for (i = 0; i < sensor_count; i++) {
            retval = system(sensors[i].command); 
            if (retval == -1) {
                logError("Could not execute sensor [%s]\n", sensors[i].command);
            } else {
                retval = WEXITSTATUS(retval);
                /* Transition from zero to non-zero (sensor failed). */
                if (sensors[i].last == 0 && retval != 0) {
                    if (retval == 255) {
                        runCommand(config);
                    } else {
                        sensors[i].failed = 1;
                        failed++;
                    }
                /* Transition from non-zero to zero (sensor recovered). */
                } else if (sensors[i].failed && retval == 0) {
                    sensors[i].failed = 0;
                    failed--;
                }
                sensors[i].last = retval;
            }
        }

        /* We don't want to keep executing the command once enough sensors fail.
         * Instead, we only execute it again once the failure count drops below
         * the threshold and crosses it again. */

        if (ran == 0 && failed >= config->failure_count) {
            logInfo("%d sensor(s) failed. Executing the command.\n", failed);
            runCommand(config);
            ran = 1;
        } else if (ran && failed < config->failure_count) {
            logInfo("Some sensors recovered. Allowing re-execution.\n");
            ran = 0;
        }

    }
}

void runCommand(config_t *config)
{
    int retval = system(config->execute);
    if (retval == -1) {
        logError("Could not execute the command [%s]\n", config->execute);
    } else if (retval != 0) {
        logError("Command returned non-zero.\n");
    }
}


void logError(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    if (use_syslog) {
        vsyslog(LOG_ERR, msg, args);
    } else {
        printf("ERROR: ");
        vprintf(msg, args);
    }
    va_end(args);
}

void logInfo(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    if (use_syslog) {
        vsyslog(LOG_NOTICE, msg, args);
    } else {
        printf("NOTICE: ");
        vprintf(msg, args);
    }
    va_end(args);
}

void strip(char *str)
{
    char *to = str, *from = str;

    /* Move to the first non-whitespace character. */
    while (isspace(*from) && *from != '\0') {
        from++;
    }

    /* Copy to the end of the string or start of a comment. */
    while (*from != '\0' && *from != '#') {
        *to = *from;
        to++;
        from++;
    }
    *to = '\0';

    /* Remove spaces from the end of the string.. */
    to--;
    while (isspace(*to)) {
        *to = '\0';
        to--;
    }
}

int startsWith(const char *prefix, const char *s)
{
    if (strlen(s) < strlen(prefix)) {
        return 0;
    }
    
    while (*prefix != '\0') {
        if (*prefix != *s) {
            return 0;
        }
        prefix++;
        s++;
    } 
    return 1;
}

void catch_signal(int signal)
{
    if (signal == SIGTERM) {
        closelog();
        exit(EXIT_SUCCESS);
    }
}

void writePID(const char *path)
{
    pid_t pid = getpid();
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        logError("Error opening PID file.\n");
        return;
    }
    if (fprintf(fp, "%d\n", pid) < 0) {
        logError("Error writing to PID file.\n");
    }
    fclose(fp);
}
