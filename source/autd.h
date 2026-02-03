#ifndef AUTD_H
#define AUTD_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <dlfcn.h>
#include <errno.h>
#include <sched.h>

#ifdef __ANDROID__
#include <sys/system_properties.h>
typedef int (*system_property_get_fn)(const char *name, char *value);
extern system_property_get_fn __system_property_get_ptr;
#endif

#define AUTD_BASE_MODE_PATH "/data/adb/autd_base_mode"
#define FILELIST_PATH "/sdcard/Android/filelist.txt"
#define AUTD_STATUS_PATH "/dev/autd_status"
#define AUTD_AWAKE_DEBUG_LOG "/sdcard/autd_awake_method.info"

typedef struct TidNode {
    int tid;
    struct TidNode *next;
} TidNode;

typedef struct {
    char base[256];
    char chosen_mode[64];
} FileEntry;


void setup_android_env(void);
void send_notif(const char* msg);
void send_notif_tag(const char* tag, const char* msg);
bool read_file_content(const char* path, char* buffer, size_t buffer_size);
char* run_cmd_capture(const char* cmd, long timeout_ms);
void log_active_method(const char* method);
bool is_awake(void);
int get_battery_level(void);
bool is_android_powersave(void);
bool get_system_property_boot_completed(void);
void perform_cleanup(void);


bool tid_exists(int tid);
void add_tid(int tid);
void clear_optimized_set(void);
void optimize_game_threads(int pid);
void load_filelist_if_changed(void);
void free_file_entries(void);
bool check_pid_name(int pid, char *out_base, size_t out_base_len, char *out_mode, size_t out_mode_len);
bool find_game_process(char *out_base, size_t out_base_len, char *out_mode, size_t out_mode_len, int *out_pid);

extern volatile sig_atomic_t running;
void handle_signal(int sig);

#endif