#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEFAULT_CONFIG "/etc/swatd/swatd.conf"
#define MAX_SCRIPTS 100
#define DEFAULT_CHECK_INTERVAL 10

typedef struct SwatConfig {
    char *execute;
    char **scripts;
    int script_count;
    int failure_count;
} config_t;

typedef struct SensorState {
    char *command;
    int last;
} sensor_t;

void printUsage(void);
void becomeDaemon(void);
void loadConfig(config_t *config, const char *path);
void monitor(config_t *config);
void logError(const char *msg, ...);
void logInfo(const char *msg, ...);
void strip(char *str);
int startsWith(const char *prefix, const char *s);

/* TODO: close log etc */

int use_syslog = 0;
int check_interval = DEFAULT_CHECK_INTERVAL;

int main(int argc, char **argv)
{
    int c = 0;
    int become_daemon = 1;
    int config_loaded = 0;
    config_t config;

    opterr = 0;
    while ((c = getopt(argc, argv, "sc:")) != -1) {
        switch (c) {
            case 's':
                become_daemon = 0;
                break;
            case 'c':
                loadConfig(&config, optarg);
                config_loaded = 1;
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
    

    if (become_daemon) {
        becomeDaemon();
    }

    if (!config_loaded) {
        loadConfig(&config, DEFAULT_CONFIG);
    }

    monitor(&config);

    return 0;
}

void printUsage(void) 
{
    printf("SWATd - Run scripts when you are being raided by the police.\n");
    printf("  -c CONFIG\t\tUse config file CONFIG.\n");
    printf("  -s\t\t\tDon't fork.\n");
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
            config->execute = malloc(strlen(cmd));
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
                /* Transition from zero to non-zero (sensor failed). */
                if (sensors[i].last == 0 && retval != 0) {
                    failed++;
                /* Transition from non-zero to zero (sensor recovered). */
                } else if (sensors[i].last > 0 && retval == 0) {
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
            retval = system(config->execute);
            if (retval == -1) {
                logError("Could not execute the command [%s]\n", config->execute);
            } else if (retval != 0) {
                logError("Command returned non-zero.\n");
            }
            ran = 1;
        } else if (ran && failed < config->failure_count) {
            logInfo("Some sensors recovered. Allowing re-execution.\n");
            ran = 0;
        }

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
