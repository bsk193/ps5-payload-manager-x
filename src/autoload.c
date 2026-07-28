#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

static volatile long long countdown_end_time = 0;

long long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)(tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

#include "pldmgr.h"
#include "autoload.h"
#include "payload_mgr.h"
#include "ps5_launcher.h"
#include "profiles.h"
#include "json_helpers.h"

static volatile int abort_flag = 0;
static volatile int remaining_seconds = -1;
static volatile int is_executing = 0;
static pthread_t autoload_thread;

static char autoload_current_name[128] = "";
static int autoload_total_count = 0;
static int autoload_done_count = 0;
static int autoload_triggered = 0; // Starts at 0, becomes 1 when frontend connects

/* Picker mode: booted with profiles present but none enabled. The worker sets
 * this and returns without running; the frontend shows a selection screen. */
static volatile int picker_active = 0;

/* When forced != 0 the worker runs the already-resolved sequence immediately
 * (no startup countdown), bypassing the enabled/picker gating. Set by
 * pldmgr_autoload_run_profile() before starting the worker thread. */
static volatile int forced_run = 0;

/* Comma-separated list of what the overlay should show for the current phase
 * (startup payloads, then the profile sequence). Read by /autoload_status. */
static char autoload_run_list[PROFILE_LIST_MAX] = "";

/* Base-name identities already launched this boot (startup + profile + manual),
 * so the same payload never starts twice even across different versions. */
#define MAX_LAUNCHED 64
static char launched_bases[MAX_LAUNCHED][128];
static int launched_count = 0;

static void launched_reset(void) { launched_count = 0; }
static int launched_has(const char *base) {
    for (int i = 0; i < launched_count; i++)
        if (strcmp(launched_bases[i], base) == 0) return 1;
    return 0;
}
static void launched_add(const char *base) {
    if (launched_count < MAX_LAUNCHED) {
        strncpy(launched_bases[launched_count], base, sizeof(launched_bases[0]) - 1);
        launched_bases[launched_count][sizeof(launched_bases[0]) - 1] = '\0';
        launched_count++;
    }
}

int pldmgr_autoload_is_picker() {
    return picker_active;
}

void pldmgr_autoload_get_list(char *buf, size_t size) {
    if (buf && size) { strncpy(buf, autoload_run_list, size - 1); buf[size - 1] = '\0'; }
}

/* Set autoload_run_list + total_count from a newline sequence file (payloads
 * only, delays excluded). Used so the overlay shows the current phase's list. */
static void set_run_list_from_file(const char *path) {
    autoload_run_list[0] = '\0';
    autoload_total_count = 0;
    autoload_done_count = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    int first = 1;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0' || line[0] == '!') continue;
        if (!first) strncat(autoload_run_list, ",", sizeof(autoload_run_list) - strlen(autoload_run_list) - 1);
        strncat(autoload_run_list, line, sizeof(autoload_run_list) - strlen(autoload_run_list) - 1);
        first = 0;
        autoload_total_count++;
    }
    fclose(f);
}

/* Launch every payload in a newline sequence file, honouring "!ms" delays and
 * skipping any payload whose base identity was already launched this boot. */
static void run_sequence_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0') continue;

        if (line[0] == '!') {
            int delay = atoi(line + 1);
            if (delay > 0) {
                pldmgr_log("[Autoload] Delaying for %d ms...\n", delay);
                usleep(delay * 1000);
            }
            continue;
        }

        char base[128];
        pldmgr_utils_get_payload_folder_name(line, base, sizeof(base));
        if (launched_has(base)) {
            pldmgr_log("[Autoload] Skipping duplicate payload: %s (already launched)\n", line);
            autoload_done_count++;
            continue;
        }

        char full_path[512];
        if (payload_mgr_resolve_path(line, full_path, sizeof(full_path)) == 0) {
            strncpy(autoload_current_name, line, sizeof(autoload_current_name) - 1);
            pldmgr_log("[Autoload] Launching: %s\n", full_path);
            ps5_launch_elf(full_path);
            launched_add(base);
            autoload_done_count++;
            usleep(500000); /* UI visibility */
        } else {
            pldmgr_log("[Autoload] !!! Payload not found: %s\n", line);
            autoload_done_count++;
        }
    }
    fclose(f);
}

/* ── Startup payloads (always-run list, stored in STARTUP_LIST_PATH) ──────── */

int pldmgr_startup_is_enabled(const char *filename) {
    FILE *f = fopen(STARTUP_LIST_PATH, "r");
    if (!f) return 0;
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strcmp(line, filename) == 0) { found = 1; break; }
    }
    fclose(f);
    return found;
}

void pldmgr_startup_set(const char *filename, int enabled) {
    if (!filename || !filename[0]) return;

    /* Read existing entries (excluding the target). */
    char entries[MAX_LAUNCHED][256];
    int n = 0;
    FILE *f = fopen(STARTUP_LIST_PATH, "r");
    if (f) {
        char line[256];
        while (n < MAX_LAUNCHED && fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] == '\0') continue;
            if (strcmp(line, filename) == 0) continue; /* drop target; re-added below if enabling */
            strncpy(entries[n], line, sizeof(entries[0]) - 1);
            entries[n][sizeof(entries[0]) - 1] = '\0';
            n++;
        }
        fclose(f);
    }

    ensure_dir_recursive(BASE_DATA_DIR);
    f = fopen(STARTUP_LIST_PATH, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) fprintf(f, "%s\n", entries[i]);
    if (enabled) fprintf(f, "%s\n", filename);
    fclose(f);
    pldmgr_log("[Startup] %s %s\n", filename, enabled ? "enabled" : "disabled");
}

int pldmgr_startup_list_json(char *buf, size_t size) {
    JsonListBuilder jb = { buf, size, 0, 1 };
    buf[0] = '\0';
    json_append(&jb, "{\"startup\":[");
    FILE *f = fopen(STARTUP_LIST_PATH, "r");
    if (f) {
        char line[256];
        int first = 1;
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (line[0] == '\0') continue;
            char esc[512];
            pldmgr_json_escape(line, esc, sizeof(esc));
            json_append(&jb, "%s\"%s\"", first ? "" : ",", esc);
            first = 0;
        }
        fclose(f);
    }
    json_append(&jb, "]}");
    return 0;
}

int pldmgr_autoload_get_remaining_seconds() {
    return remaining_seconds;
}

long long pldmgr_autoload_get_remaining_ms() {
    if (countdown_end_time == 0) return (long long)remaining_seconds * 1000;
    long long now = get_current_time_ms();
    if (now >= countdown_end_time) return 0;
    return countdown_end_time - now;
}

void pldmgr_autoload_get_status(int *total, int *done, char *current) {
    autoload_triggered = 1; // Signal that frontend is ready/active
    *total = autoload_total_count;
    *done = autoload_done_count;
    if (current) strcpy(current, autoload_current_name);
}

void* pldmgr_autoload_worker(void* arg) {
    struct stat st;

    int browser_open = 1;
    int auto_delay = 5;

    FILE *ef = fopen(PLDMGR_CONFIG_PATH, "r");
    if (ef) {
        char line[128];
        while (fgets(line, sizeof(line), ef)) {
            if (strncmp(line, "AUTO_BROWSER_OPEN=", 18) == 0) {
                browser_open = atoi(line + 18);
            } else if (strncmp(line, "AUTOLOAD_DELAY=", 15) == 0) {
                auto_delay = atoi(line + 15);
            }
        }
        fclose(ef);
    }

    int forced = forced_run;

    /* ── Manual run (user picked a profile from the startup selection) ──────
     * The chosen profile is already resolved into PROFILE_SCRATCH_PATH. Run it
     * immediately with no countdown. Startup payloads already ran at boot, so
     * launched_bases still holds them and duplicates are skipped. */
    if (forced) {
        if (stat(PROFILE_SCRATCH_PATH, &st) != 0) {
            pldmgr_log("[Autoload] !!! Manual run: resolved sequence missing.\n");
            forced_run = 0;
            return NULL;
        }
        pldmgr_log("[Autoload] Manual run: executing selected profile immediately.\n");
        countdown_end_time = 0;
        remaining_seconds = 0;
        is_executing = 1;
        set_run_list_from_file(PROFILE_SCRATCH_PATH);
        run_sequence_file(PROFILE_SCRATCH_PATH);
        strcpy(autoload_current_name, "DONE");
        remaining_seconds = 0;
        is_executing = 0;
        forced_run = 0;
        return NULL;
    }

    /* ── Boot ─────────────────────────────────────────────────────────────── */
    launched_reset();
    autoload_run_list[0] = '\0';
    autoload_total_count = 0;
    autoload_done_count = 0;
    autoload_current_name[0] = '\0';

    /* Wait briefly for the frontend so the overlay is ready to show progress. */
    if (browser_open) {
        pldmgr_log("[Autoload] Browser Mode: Waiting for frontend connection...\n");
        int wait_timeout = 50; /* 50 * 100ms = 5 seconds */
        while (!autoload_triggered && wait_timeout-- > 0) {
            usleep(100000);
        }
    }

    /* Phase 1: startup payloads — always run, no countdown, no abort. */
    if (stat(STARTUP_LIST_PATH, &st) == 0) {
        set_run_list_from_file(STARTUP_LIST_PATH);
        if (autoload_total_count > 0) {
            pldmgr_log("[Autoload] Running %d startup payload(s)...\n", autoload_total_count);
            is_executing = 1;
            remaining_seconds = 0;
            countdown_end_time = 0;
            run_sequence_file(STARTUP_LIST_PATH);
            is_executing = 0;
        }
    }

    /* Phase 2: resolve the active profile (or fall through to picker/dashboard). */
    {
        ProfileEntry *arr = calloc(MAX_PROFILES, sizeof(ProfileEntry));
        if (!arr) return NULL;
        int count = 0;
        profiles_load(arr, &count);
        int active = profiles_active_index(arr, count);

        if (active < 0) {
            if (count > 0) {
                picker_active = 1;
                pldmgr_log("[Autoload] No enabled profile - showing startup picker (%d profiles).\n", count);
            } else {
                pldmgr_log("[Autoload] No profiles configured - booting to dashboard.\n");
            }
            free(arr);
            /* Startup payloads (if any) already ran; clear the overlay so the
             * picker (or dashboard) takes over. */
            is_executing = 0;
            remaining_seconds = -1;
            countdown_end_time = 0;
            return NULL;
        }

        pldmgr_log("[Autoload] Enabled profile: %s\n", arr[active].name);
        profiles_write_sequence(arr[active].list);
        free(arr);
    }

    if (stat(PROFILE_SCRATCH_PATH, &st) != 0) {
        pldmgr_log("[Autoload] !!! Resolved profile sequence missing.\n");
        is_executing = 0;
        remaining_seconds = -1;
        return NULL;
    }

    /* Show the profile sequence and run its countdown (abort-able). */
    set_run_list_from_file(PROFILE_SCRATCH_PATH);
    remaining_seconds = auto_delay;
    countdown_end_time = get_current_time_ms() + (long long)auto_delay * 1000;

    if (auto_delay > 0) {
        int klog_fd = -1;

        /* Only fall back to on-screen notification + PS button if the browser
         * is NOT auto-opened. */
        if (!browser_open) {
            if (!pldmgr_server_is_active()) {
                char ip[64];
                if (pldmgr_get_local_ip(ip, sizeof(ip)) != 0) strcpy(ip, "0.0.0.0");
                pldmgr_notify(PLDMGR_APP_NAME " Running\nhttp://%s:%d", ip, MENU_PORT);
                pldmgr_notify("Autoloading in %ds\nPress PS Button to Abort", auto_delay);
            }

            klog_fd = open("/dev/klog", O_RDONLY | O_NONBLOCK);
            if (klog_fd >= 0) {
                char flush_buf[4096];
                while (read(klog_fd, flush_buf, sizeof(flush_buf)) > 0);
            }
            pldmgr_log("[Autoload] Fallback Mode: Starting %ds countdown (PS Button active)...\n", auto_delay);
        } else {
            pldmgr_log("[Autoload] Browser Mode: Starting %ds countdown...\n", auto_delay);
        }

        char klog_buf[2048];
        long long remaining_ms;
        while ((remaining_ms = countdown_end_time - get_current_time_ms()) > 0) {
            remaining_seconds = (int)((remaining_ms + 999) / 1000);
            if (abort_flag) {
                if (klog_fd >= 0) close(klog_fd);
                countdown_end_time = 0;
                remaining_seconds = -1;
                is_executing = 0;
                return NULL;
            }

            if (klog_fd >= 0) {
                ssize_t n = read(klog_fd, klog_buf, sizeof(klog_buf) - 1);
                if (n > 0) {
                    klog_buf[n] = 0;
                    if (strstr(klog_buf, "onPSButtonPressed")) {
                        pldmgr_log("[Autoload] ABORTED via PS Button.\n");
                        pldmgr_notify("Autoload Aborted");
                        abort_flag = 1;
                        close(klog_fd);
                        countdown_end_time = 0;
                        remaining_seconds = -1;
                        is_executing = 0;
                        return NULL;
                    }
                }
            }
            usleep(100000); /* 100ms */
        }
        if (klog_fd >= 0) close(klog_fd);
    }
    countdown_end_time = 0;
    remaining_seconds = 0;

    is_executing = 1;
    pldmgr_log("[Autoload] Starting profile sequence...\n");
    run_sequence_file(PROFILE_SCRATCH_PATH);

    pldmgr_log("[Autoload] Sequence complete.\n");
    strcpy(autoload_current_name, "DONE");
    remaining_seconds = 0;
    is_executing = 0;
    return NULL;
}

int pldmgr_autoload_start() {
    abort_flag = 0;
    is_executing = 0;
    forced_run = 0;
    if (pthread_create(&autoload_thread, NULL, pldmgr_autoload_worker, NULL) != 0) {
        pldmgr_log("[Autoload] !!! Failed to create background thread\n");
        return -1;
    }
    pthread_detach(autoload_thread);
    return 0;
}

int pldmgr_autoload_run_profile(const char *id) {
    if (!id || !id[0]) return -1;

    ProfileEntry *arr = calloc(MAX_PROFILES, sizeof(ProfileEntry));
    if (!arr) return -1;
    int count = 0;
    profiles_load(arr, &count);
    int idx = profiles_find_by_id(arr, count, id);
    if (idx < 0) {
        free(arr);
        pldmgr_log("[Autoload] !!! Manual run: unknown profile id '%s'\n", id);
        return -1;
    }

    pldmgr_log("[Autoload] Manual run requested: %s\n", arr[idx].name);
    profiles_write_sequence(arr[idx].list);
    free(arr);

    /* Leaving the picker and starting a forced (no-countdown) run. Prime the
     * status fields up front so the first poll sees an active run (remaining >= 0)
     * rather than a stale idle value, avoiding a dashboard flash. */
    picker_active = 0;
    abort_flag = 0;
    is_executing = 0;
    autoload_total_count = 0;
    autoload_done_count = 0;
    remaining_seconds = 0;
    countdown_end_time = 0;
    strcpy(autoload_current_name, "");
    forced_run = 1;

    if (pthread_create(&autoload_thread, NULL, pldmgr_autoload_worker, NULL) != 0) {
        pldmgr_log("[Autoload] !!! Failed to create background thread\n");
        forced_run = 0;
        return -1;
    }
    pthread_detach(autoload_thread);
    return 0;
}

void pldmgr_autoload_abort() {
    if (!is_executing) {
        abort_flag = 1;
    }
}

void pldmgr_autoload_reset() {
    /* If already counting down, don't reset to avoid jumps.
     * The browser launch often triggers a system 'resume' which would snap time back.
     * We only reset if the countdown has already finished or hasn't started. */
    if (countdown_end_time > 0 && remaining_seconds > 0) {
        pldmgr_log("[PLDMGR] Autoload reset ignored (timer active)\n");
        return;
    }

    pldmgr_log("[PLDMGR] Autoload reset triggered\n");
    remaining_seconds = -1;
    is_executing = 0;
    picker_active = 0;
    forced_run = 0;
    autoload_total_count = 0;
    autoload_done_count = 0;
    autoload_triggered = 0;
    strcpy(autoload_current_name, "");
}

void pldmgr_autoload_update_config_entry(const char *old_filename, const char *new_filename) {
    /* A payload rename/removal must be reflected in every profile's sequence
     * AND in the startup list (autoload.txt). */
    profiles_propagate_payload_change(old_filename, new_filename);

    /* Update the startup list: rewrite it, replacing or dropping the entry. */
    FILE *f = fopen(STARTUP_LIST_PATH, "r");
    if (!f) return;
    char lines[MAX_LAUNCHED][256];
    int n = 0, modified = 0;
    char line[256];
    while (n < MAX_LAUNCHED && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '\0') continue;
        if (strcmp(line, old_filename) == 0) {
            modified = 1;
            if (new_filename) { strncpy(lines[n], new_filename, sizeof(lines[0]) - 1); lines[n][sizeof(lines[0]) - 1] = '\0'; n++; }
            /* else: drop the entry (delete) */
        } else {
            strncpy(lines[n], line, sizeof(lines[0]) - 1);
            lines[n][sizeof(lines[0]) - 1] = '\0';
            n++;
        }
    }
    fclose(f);
    if (modified) {
        f = fopen(STARTUP_LIST_PATH, "w");
        if (f) {
            for (int i = 0; i < n; i++) fprintf(f, "%s\n", lines[i]);
            fclose(f);
        }
    }
}
