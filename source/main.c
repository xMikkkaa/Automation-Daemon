#include "autd.h"

volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

int main() {
    setup_android_env();
    char last_mode[64] = "";
    char user_base[64] = "balance";
    char current_game[256] = "";
    bool low_bat_notif_sent = false;
    struct sigaction sa;
    signal(SIGCHLD, SIG_IGN);
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Wait for boot completed
    while (running && !get_system_property_boot_completed()) {
        sleep(1);
    }
    if (!running) {
        send_notif_tag("autd_startup", "Automation Daemon Stopped (during boot wait)");
        return 0;
    }
    
    sleep(5); // Sleep 5 seconds
    send_notif_tag("autd_startup", "Automation Daemon Started");

    int idle_cycles = 0;
    while (running) {
        // --- SMART SCREEN OFF LOGIC ---
        if (!is_awake()) {
            sleep(7); // Debounce
            if (!is_awake()) {
                if (strcmp(last_mode, "powersave") != 0) {
                    if (system("powersave") == -1) perror("system(powersave)");
                    strncpy(last_mode, "powersave", sizeof(last_mode)-1);
                    last_mode[sizeof(last_mode)-1] = '\0';
                    
                    FILE *sf = fopen(AUTD_STATUS_PATH, "w");
                    if (sf) { fprintf(sf, "powersave (Screen Off)"); fclose(sf); }
                }
                
                // Deep Sleep Loop
                while (running && !is_awake()) {
                    sleep(30);
                }
            }
            continue; 
        }

        if (read_file_content(AUTD_BASE_MODE_PATH, user_base, sizeof(user_base))) {
        } else {
            strcpy(user_base, "balance");
        }

        int bat_level = get_battery_level();
        bool ps_active = is_android_powersave();

        load_filelist_if_changed();

        bool game_found = false;
        char chosen_mode[64] = "performance";
        int game_pid = -1;
        if (find_game_process(current_game, sizeof(current_game), chosen_mode, sizeof(chosen_mode), &game_pid)) {
            game_found = true;
        }

        if (game_found) {
            if (strcmp(last_mode, chosen_mode) != 0) {
                if (system(chosen_mode) == -1) {
                    perror("system(chosen_mode)");
                }
                char msg[300];
                if (strcmp(chosen_mode, "gaming2") == 0) {
                    snprintf(msg, sizeof(msg), "Game Aktif: %s (Auto Gaming 2)", current_game);
                } else if (strcmp(chosen_mode, "gaming") == 0) {
                    snprintf(msg, sizeof(msg), "Game Aktif: %s (Auto Gaming)", current_game);
                } else {
                    snprintf(msg, sizeof(msg), "Game Aktif: %s (Auto Performance)", current_game);
                }
                send_notif(msg);
                strncpy(last_mode, chosen_mode, sizeof(last_mode)-1);
                last_mode[sizeof(last_mode)-1] = '\0';
                idle_cycles = 0;
            }
            if (game_pid > 0) optimize_game_threads(game_pid);
        }
        else if (bat_level <= 20 || ps_active) {
            if (strcmp(last_mode, "powersave") != 0) {
                if (system("powersave") == -1) {
                    perror("system(powersave)");
                }
                send_notif("Mode: Powersave (Battery Low/System Saver)");
                strncpy(last_mode, "powersave", sizeof(last_mode)-1);
                last_mode[sizeof(last_mode)-1] = '\0';
                idle_cycles = 0;
            }
            clear_optimized_set();
        }
        else {
            if (strcmp(last_mode, user_base) != 0) {
                if (system(user_base) == -1) {
                    perror("system(user_base)");
                }
                char msg[300];
                snprintf(msg, sizeof(msg), "Mode: %s", user_base);
                send_notif(msg);
                strncpy(last_mode, user_base, sizeof(last_mode)-1);
                last_mode[sizeof(last_mode)-1] = '\0';
                idle_cycles = 0;
            }
            clear_optimized_set();
        }

        if (bat_level <= 20 && !low_bat_notif_sent) {
        send_notif("Baterai 20%! Sistem beralih ke Powersave.");
            low_bat_notif_sent = true;
        } else if (bat_level > 20) {
            low_bat_notif_sent = false;
        }

        FILE *sf = fopen(AUTD_STATUS_PATH, "w");
        if (sf) {
            fprintf(sf, "%s", last_mode);
            fclose(sf);
        }

        if (!game_found && bat_level > 20 && !ps_active) {
            idle_cycles++;
            if (idle_cycles > 10) {
                sleep(10);
            } else {
                sleep(3);
            }
        } else {
            idle_cycles = 0;
            sleep(3);
        }
    }
    free_file_entries();
    perform_cleanup();
    send_notif_tag("autd_startup", "Automation Daemon Stopped");
    return 0;
}