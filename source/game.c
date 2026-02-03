#include "autd.h"

static TidNode *optimized_set = NULL;
static FileEntry *file_entries = NULL;
static size_t num_file_entries = 0;
static time_t filelist_mtime = 0;

bool tid_exists(int tid) {
    TidNode *cur = optimized_set;
    while (cur) {
        if (cur->tid == tid) return true;
        cur = cur->next;
    }
    return false;
}

void add_tid(int tid) {
    if (tid_exists(tid)) return;
    TidNode *node = malloc(sizeof(TidNode));
    if (!node) return;
    node->tid = tid;
    node->next = optimized_set;
    optimized_set = node;
}

void clear_optimized_set(void) {
    TidNode *cur = optimized_set;
    while (cur) {
        TidNode *next = cur->next;
        free(cur);
        cur = next;
    }
    optimized_set = NULL;
}


void optimize_game_threads(int pid) {
    char task_path[64];
    snprintf(task_path, sizeof(task_path), "/proc/%d/task/", pid);
    
    DIR *task_dir = opendir(task_path);
    if (!task_dir) return;

    struct dirent *task_entry;
    while ((task_entry = readdir(task_dir)) != NULL) {
        if (!isdigit((unsigned char)task_entry->d_name[0])) continue;
        
        int tid = atoi(task_entry->d_name);
        if (tid_exists(tid)) continue;

        cpu_set_t mask;
        CPU_ZERO(&mask);
        // Enable all cores (up to 32)
        for (int i = 0; i < 32; i++) CPU_SET(i, &mask);

        if (sched_setaffinity(tid, sizeof(mask), &mask) == 0) {
            add_tid(tid);
        }
    }
    closedir(task_dir);
}

void free_file_entries(void) {
    if (file_entries) {
        free(file_entries);
        file_entries = NULL;
    }
    num_file_entries = 0;
    filelist_mtime = 0;
}

bool check_pid_name(int pid, char *out_base, size_t out_base_len, char *out_mode, size_t out_mode_len) {
    char cmdline_path[64];
    snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);
    
    FILE *cmd_f = fopen(cmdline_path, "r");
    if (!cmd_f) return false;

    char cmdline[256];
    size_t r = fread(cmdline, 1, sizeof(cmdline) - 1, cmd_f);
    fclose(cmd_f);
    
    if (r == 0) return false;
    cmdline[r] = '\0';

    char *name = cmdline;
    
    if (name[0] == '\0') return false;

    char *slash = strrchr(name, '/');
    if (slash && slash[1] != '\0') name = slash + 1;

    for (size_t fe = 0; fe < num_file_entries; fe++) {
        if (strcmp(name, file_entries[fe].base) == 0) {
            if (out_base && out_base_len) {
                strncpy(out_base, file_entries[fe].base, out_base_len - 1);
                out_base[out_base_len - 1] = '\0';
            }
            if (out_mode && out_mode_len) {
                strncpy(out_mode, file_entries[fe].chosen_mode, out_mode_len - 1);
                out_mode[out_mode_len - 1] = '\0';
            }
            return true;
        }
    }
    return false;
}

bool find_game_process(char *out_base, size_t out_base_len, char *out_mode, size_t out_mode_len, int *out_pid) {
    if (num_file_entries == 0) return false;
    const char* cpuset_paths[] = {
        "/dev/cpuset/top-app/cgroup.procs",
        "/dev/stune/top-app/cgroup.procs",
        NULL
    };
    for (int i = 0; cpuset_paths[i] != NULL; i++) {
        FILE *f = fopen(cpuset_paths[i], "r");
        if (f) {
            int pid;
            while (fscanf(f, "%d", &pid) == 1) {
                if (check_pid_name(pid, out_base, out_base_len, out_mode, out_mode_len)) {
                    *out_pid = pid;
                    fclose(f);
                    return true;
                }
            }
            fclose(f);
            return false; 
        }
    }
    DIR *proc = opendir("/proc");
    if (!proc) return false;
    
    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (!isdigit((unsigned char)entry->d_name[0])) continue;
        int pid = atoi(entry->d_name);
        if (check_pid_name(pid, out_base, out_base_len, out_mode, out_mode_len)) {
            *out_pid = pid;
            closedir(proc);
            return true;
        }
    }
    closedir(proc);
    return false;
}

void load_filelist_if_changed(void) {
    struct stat st;
    if (stat(FILELIST_PATH, &st) == 0) {
        if (filelist_mtime == st.st_mtime && file_entries != NULL) {
            return;
        }
        filelist_mtime = st.st_mtime;
    } else {
        free_file_entries();
        filelist_mtime = 0;
        return;
    }

    FILE *fl = fopen(FILELIST_PATH, "r");
    if (!fl) {
        free_file_entries();
        return;
    }

    size_t cap = 16;
    FileEntry *arr = malloc(cap * sizeof(FileEntry));
    if (!arr) {
        fclose(fl);
        return;
    }
    size_t n = 0;
    char line[256];
    while (fgets(line, sizeof(line), fl)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0') continue;

        char base[256];
        strncpy(base, line, sizeof(base)-1);
        base[sizeof(base)-1] = '\0';

        char chosen_mode[64] = "performance";
        size_t len = strlen(base);
        if (len > 3 && strcmp(base + len - 3, "_g2") == 0) {
            base[len - 3] = '\0';
            strncpy(chosen_mode, "gaming2", sizeof(chosen_mode)-1);
            chosen_mode[sizeof(chosen_mode)-1] = '\0';
        } else if (len > 2 && strcmp(base + len - 2, "_g") == 0) {
            base[len - 2] = '\0';
            strncpy(chosen_mode, "gaming", sizeof(chosen_mode)-1);
            chosen_mode[sizeof(chosen_mode)-1] = '\0';
        } else if (len > 2 && strcmp(base + len - 2, "_p") == 0) {
            base[len - 2] = '\0';
            strncpy(chosen_mode, "performance", sizeof(chosen_mode)-1);
            chosen_mode[sizeof(chosen_mode)-1] = '\0';
        } else {
            strncpy(chosen_mode, "performance", sizeof(chosen_mode)-1);
            chosen_mode[sizeof(chosen_mode)-1] = '\0';
        }

        if (n >= cap) {
            size_t ncap = cap * 2;
            FileEntry *tmp = realloc(arr, ncap * sizeof(FileEntry));
            if (!tmp) break;
            arr = tmp;
            cap = ncap;
        }
        strncpy(arr[n].base, base, sizeof(arr[n].base)-1);
        arr[n].base[sizeof(arr[n].base)-1] = '\0';
        strncpy(arr[n].chosen_mode, chosen_mode, sizeof(arr[n].chosen_mode)-1);
        arr[n].chosen_mode[sizeof(arr[n].chosen_mode)-1] = '\0';
        n++;
    }
    fclose(fl);

    free_file_entries();
    file_entries = arr;
    num_file_entries = n;
}