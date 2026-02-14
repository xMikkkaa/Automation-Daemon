#include "../include/autd.h"

// Add a volatile flag to control the main loop when waiting for screen on
void signal_handler(int sig) {
    (void)sig;
    send_toast("🛑 xBooster Daemon Stopped");
    exit(0);
}

int main() {
    setup_android_env();

    // Register signal handlers at the beginning
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask); // No signals blocked in handler
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    char last_mode[64] = "";
    char user_base[64] = "balance";
    char current_game[256] = "";

    bool low_bat_notif_sent = false;
    bool is_optimize_allowed = false;

    // Wait for boot completed
    while (!get_system_property_boot_completed()) {
        sleep(1);
    }

    sleep(5); // Initial stabilization
    send_toast("Automation Daemon Started");

    int idle_cycles = 0;

    // --- MAIN LOOP ---
    while (1) {

        // ==========================================
        // 1. SMART DEEP SLEEP LOGIC (Battery Saver)
        // ==========================================
        if (!is_awake()) {
            log_active_method("Screen OFF. Entering Deep Sleep Protocol.");

            // Debounce: Wait 7 seconds to see if user turns screen back on
            log_active_method("Screen is OFF. Waiting 7 seconds before terminating.");
            sleep(7);

            if (!is_awake()) {
                // SCREEN STILL OFF -> Terminate
                log_active_method("Screen still OFF. Terminating daemon.");
                if (system("powersave") == -1) {
                    perror("system(powersave)");
                }
                send_toast("Screen is OFF. xBooster Daemon is stopping.");
                exit(0);
            }
            log_active_method("Screen back ON. Resuming normal operation.");
            // If screen turned back on, continue loop to apply correct mode immediately
            continue;
        }

        // ==========================================
        // 2. NORMAL OPERATION (Screen ON)
        // ==========================================

        // Read User Config
        if (!read_file_content(AUTD_BASE_MODE_PATH, user_base, sizeof(user_base))) {
            strcpy(user_base, "balance");
        }

        // Read Optimizer Toggle
        char opt_allow_buffer[2];
        if (read_file_content(AUTD_OPT_ALLOW_PATH, opt_allow_buffer, sizeof(opt_allow_buffer))) {
            is_optimize_allowed = (strcmp(opt_allow_buffer, "1") == 0);
        } else {
            is_optimize_allowed = true; // Default ON
        }

        int bat_level = get_battery_level();
        bool ps_active = is_android_powersave();

        load_filelist_if_changed();

        bool game_found = false;
        char chosen_mode[64] = "performance";
        int game_pid = -1;

        // Check for Games
        if (find_game_process(current_game, sizeof(current_game), chosen_mode, sizeof(chosen_mode), &game_pid)) {
            game_found = true;
        }

        // --- MODE SWITCHING LOGIC ---
        if (game_found) {
            if (strcmp(last_mode, chosen_mode) != 0) {
                if (system(chosen_mode) == -1) perror("system(chosen_mode)");
                
                char msg_buffer[300];
                snprintf(msg_buffer, sizeof(msg_buffer), "Game: %s (Mode: %s)", current_game, chosen_mode);
                send_toast(msg_buffer);
                
                strncpy(last_mode, chosen_mode, sizeof(last_mode) - 1);
                last_mode[sizeof(last_mode) - 1] = '\0';
                idle_cycles = 0;
            }
            
            // Apply Thread Optimization if Allowed
            if (game_pid > 0) {
                if (is_optimize_allowed) {
                    optimize_game_threads(game_pid);
                } else {
                    clear_optimized_set();
                }
            }
        }
        else if (bat_level <= 20 || ps_active) {
            // Low Battery / System Power Saver
            if (strcmp(last_mode, "powersave") != 0) {
                if (system("powersave") == -1) perror("system(powersave)");
                
                send_toast("Mode: Powersave (Battery Low/System Saver)");
                strncpy(last_mode, "powersave", sizeof(last_mode)-1);
                last_mode[sizeof(last_mode)-1] = '\0';
                last_mode[sizeof(last_mode)-1] = '\0';
                idle_cycles = 0;
            }
            clear_optimized_set();
        }
        else {
            // Normal Usage (Balance/User Base)
            if (strcmp(last_mode, user_base) != 0) {
                if (system(user_base) == -1) perror("system(user_base)");
                
                char msg_buffer[300];
                snprintf(msg_buffer, sizeof(msg_buffer), "Mode: %s", user_base);
                send_toast(msg_buffer);
                
                strncpy(last_mode, user_base, sizeof(last_mode) - 1);
                last_mode[sizeof(last_mode) - 1] = '\0';
                idle_cycles = 0;
            }
            clear_optimized_set();
        }

        // Low Battery Notification Logic
        if (bat_level <= 20 && !low_bat_notif_sent) {
            send_toast("Baterai 20%! Sistem beralih ke Powersave.");
            low_bat_notif_sent = true;
        } else if (bat_level > 20) {
            low_bat_notif_sent = false;
        }

        // Write Status File
        FILE *sf = fopen(AUTD_STATUS_PATH, "w");
        if (sf) {
            fprintf(sf, "%s", last_mode);
            fclose(sf);
        }

        // Smart Sleep Interval
        if (!game_found && bat_level > 20 && !ps_active) {
            idle_cycles++;
            if (idle_cycles > 10) {
                sleep(10); // Relaxed checking
            } else {
                sleep(3);
            }
        } else {
            idle_cycles = 0;
            sleep(3); // Aggressive checking for games
        }
    }
    
    // Cleanup on Exit
    free_file_entries();
    perform_cleanup();
    send_toast("Automation Daemon Stopped");
    return 0;
}