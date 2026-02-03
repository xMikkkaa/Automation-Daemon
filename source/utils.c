#include "autd.h"

#ifdef __ANDROID__
system_property_get_fn __system_property_get_ptr = NULL;
#endif

static char last_awake_method[64] = "";

void setup_android_env(void) {
    setenv("PATH", "/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin:/vendor/xbin", 1);
    setenv("ANDROID_ROOT", "/system", 1);
    setenv("ANDROID_DATA", "/data", 1);
    setenv("ANDROID_STORAGE", "/storage", 1);
    setenv("ANDROID_ART_ROOT", "/apex/com.android.art", 1);
    setenv("ANDROID_I18N_ROOT", "/apex/com.android.i18n", 1);
}

void send_notif(const char* msg) {
    char notification_msg[512];
    int n = snprintf(notification_msg, sizeof(notification_msg),
                     "cmd notification post -S bigtext -t 'Automation Daemon' 'autd_status' '%s'",
                     msg ? msg : "");
    if (n < 0 || (size_t)n >= sizeof(notification_msg)) {
        notification_msg[sizeof(notification_msg)-1] = '\0';
    }
    system(notification_msg);
}

void send_notif_tag(const char* tag, const char* msg) {
    char notification_msg[512];
    int n = snprintf(notification_msg, sizeof(notification_msg),
                     "cmd notification post -S bigtext -t 'Automation Daemon' '%s' '%s'",
                     tag ? tag : "autd_status", msg ? msg : "");
    if (n < 0 || (size_t)n >= sizeof(notification_msg)) {
        notification_msg[sizeof(notification_msg)-1] = '\0';
    }
    system(notification_msg);
}

bool read_file_content(const char* path, char* buffer, size_t buffer_size) {
    FILE* f = fopen(path, "r");
    if (!f) {
        return false;
    }
    if (fgets(buffer, buffer_size, f) != NULL) {
        buffer[strcspn(buffer, "\n\r")] = 0; 
        size_t len = strlen(buffer);
        while (len > 0 && isspace((unsigned char)buffer[len-1])) {
            buffer[--len] = '\0';
        }
        if (len < 3 || strcmp(buffer, "0") == 0) {
            fclose(f);
            return false;
        }

        fclose(f);
        return true;
    }
    fclose(f);
    return false;
}

char* run_cmd_capture(const char* cmd, long timeout_ms) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    } else if (pid == 0) {
        close(pipefd[0]); 
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2 stdout");
            _exit(1);
        }
        close(pipefd[1]);

        int devnull_fd = open("/dev/null", O_WRONLY);
        if (devnull_fd == -1) {
            perror("open /dev/null");
            _exit(1);
        }
        if (dup2(devnull_fd, STDERR_FILENO) == -1) {
            perror("dup2 stderr");
            _exit(1);
        }
        close(devnull_fd);

        execlp("/system/bin/sh", "/system/bin/sh", "-c", cmd, (char *)NULL);
        perror("execlp");
        _exit(1);
    } else {
        close(pipefd[1]); 
        char *buffer = NULL;
        size_t buffer_size = 0;
        ssize_t bytes_read;
        int status;

        struct pollfd pfd = { .fd = pipefd[0], .events = POLLIN };
        struct timespec start_time, current_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        bool timed_out = false;

        while (1) {
            int poll_ret = poll(&pfd, 1, 100);
            if (poll_ret == -1) {
                if (errno == EINTR) continue;
                perror("poll");
                break;
            } else if (poll_ret == 0) {
                clock_gettime(CLOCK_MONOTONIC, &current_time);
                long elapsed_ms = (current_time.tv_sec - start_time.tv_sec) * 1000 +
                                  (current_time.tv_nsec - start_time.tv_nsec) / 1000000;
                if (elapsed_ms >= timeout_ms) {
                    timed_out = true;
                    break;
                }
                continue;
            } else {
                buffer_size += 256;
                buffer = realloc(buffer, buffer_size);
                if (!buffer) {
                    perror("realloc");
                    break;
                }
                bytes_read = read(pipefd[0], buffer + buffer_size - 256, 255);
                if (bytes_read == -1) {
                    perror("read");
                    break;
                } else if (bytes_read == 0) {
                    break;
                }
                buffer[buffer_size - 256 + bytes_read] = '\0';
            }
        }

        if (timed_out) {
            fprintf(stderr, "Command \'%s\' timed out after %ld ms\n", cmd, timeout_ms);
            kill(pid, SIGTERM);
        }
        waitpid(pid, &status, 0); 
        close(pipefd[0]);

        if (buffer) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n') {
                buffer[len-1] = '\0';
            }
        }
        return buffer;
    }
}


void log_active_method(const char* method) {
    if (strcmp(last_awake_method, method) != 0) {
        FILE *log_file = fopen(AUTD_AWAKE_DEBUG_LOG, "w");
        if (log_file) {
            fprintf(log_file, "Active Method: %s\n", method);
            fclose(log_file);
        }
        strncpy(last_awake_method, method, sizeof(last_awake_method) - 1);
        last_awake_method[sizeof(last_awake_method) - 1] = '\0';
    }
}

bool is_awake(void) {
    DIR *backlight_base_dir = opendir("/sys/class/backlight/");
    if (backlight_base_dir) {
        struct dirent *backlight_entry;
        bool backlight_found = false;
        char backlight_dir_path[256];

        while ((backlight_entry = readdir(backlight_base_dir)) != NULL) {
            if (strcmp(backlight_entry->d_name, ".") == 0 || strcmp(backlight_entry->d_name, "..") == 0) continue;
            
            snprintf(backlight_dir_path, sizeof(backlight_dir_path), "/sys/class/backlight/%s/brightness", backlight_entry->d_name);
            
            FILE *brightness_file = fopen(backlight_dir_path, "r");
            if (brightness_file) {
                int brightness = 0;
                if (fscanf(brightness_file, "%d", &brightness) == 1) {
                    fclose(brightness_file);
                    closedir(backlight_base_dir);
                    if (brightness > 0) {
                        log_active_method("SysFS Backlight");
                        return true;
                    } else {
                        log_active_method("SysFS Backlight");
                        return false;
                    }
                }
                fclose(brightness_file);
            }
        }
        closedir(backlight_base_dir);
    }

    DIR *drm_dir = opendir("/sys/class/drm/");
    if (drm_dir) {
        struct dirent *entry;
        while ((entry = readdir(drm_dir)) != NULL) {
            if (strncmp(entry->d_name, "card0-", 6) == 0 && strstr(entry->d_name, "virtual") == NULL) {
                char enabled_path[256];
                snprintf(enabled_path, sizeof(enabled_path), "/sys/class/drm/%s/enabled", entry->d_name);
                FILE *f_enabled = fopen(enabled_path, "r");
                if (f_enabled) {
                    char status_buf[16];
                    if (fgets(status_buf, sizeof(status_buf), f_enabled)) {
                        fclose(f_enabled);
                        closedir(drm_dir);
                        status_buf[strcspn(status_buf, "\n\r")] = 0;
                        if (strcmp(status_buf, "enabled") == 0) {
                            log_active_method("SysFS DRM (Enabled Check)");
                            return true;
                        } else if (strcmp(status_buf, "disabled") == 0) {
                            log_active_method("SysFS DRM (Enabled Check)");
                            return false;
                        }
                    }
                    fclose(f_enabled);
                }
            }
        }
        closedir(drm_dir);
    }

    char* dumpsys_output = run_cmd_capture("dumpsys power", 1000);
    bool awake = false;
    if (dumpsys_output) {
        if (strstr(dumpsys_output, "mWakefulness=Awake")) {
            awake = true;
            log_active_method("Fallback Dumpsys");
        } else {
            log_active_method("Fallback Dumpsys");
        }
        free(dumpsys_output);
    }
    return awake;
}

int get_battery_level(void) {
    FILE *f = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (!f) return 100;
    int level = 100;
    if (fscanf(f, "%d", &level) != 1) level = 100;
    fclose(f);
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    return level;
}

bool is_android_powersave(void) {
    char* output = run_cmd_capture("settings get global low_power", 1000);
    bool active = false;
    if (output) {
        active = (output[0] == '1');
        free(output);
    }
    return active;
}

bool get_system_property_boot_completed(void) {
#ifdef __ANDROID__
    if (!__system_property_get_ptr) {
        void* libc_handle = dlopen("libc.so", RTLD_NOW);
        if (libc_handle) {
            __system_property_get_ptr = (system_property_get_fn)dlsym(libc_handle, "__system_property_get");
        }
    }

    if (__system_property_get_ptr) {
        char value[PROP_VALUE_MAX];
        if (__system_property_get_ptr("sys.boot_completed", value) > 0) {
            return strcmp(value, "1") == 0;
        }
    }
#endif
    char* output = run_cmd_capture("getprop sys.boot_completed", 1000);
    bool boot_completed = false;
    if (output) {
        boot_completed = (strcmp(output, "1") == 0);
        free(output);
    }
    return boot_completed;
}

void perform_cleanup(void) {
    remove(AUTD_STATUS_PATH); 
    remove(AUTD_AWAKE_DEBUG_LOG);
    remove(AUTD_BASE_MODE_PATH); 
    sync();
}