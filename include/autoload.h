#pragma once

#include <stddef.h>

/*
 * Start the autoload sequence in a background thread.
 * Returns 0 on success, -1 on failure.
 */
int pldmgr_autoload_start();
void pldmgr_autoload_abort();
void pldmgr_autoload_reset();
int pldmgr_autoload_get_remaining_seconds();
long long pldmgr_autoload_get_remaining_ms();
void pldmgr_autoload_get_status(int *total, int *done, char *current);
void pldmgr_autoload_update_config_entry(const char *old_filename, const char *new_filename);

/* Returns 1 when the manager booted into "picker" mode: autoload has profiles
 * but none is enabled, so the frontend should show a startup selection instead
 * of auto-running anything. Cleared once a profile is run or dismissed. */
int pldmgr_autoload_is_picker();

/* Run a specific profile immediately (no startup countdown), regardless of which
 * profile is enabled. Used when the user picks one from the startup selection.
 * Returns 0 on success, -1 if the profile id is unknown. */
int pldmgr_autoload_run_profile(const char *id);

/* Comma-separated list of payloads for the overlay's current phase. */
void pldmgr_autoload_get_list(char *buf, size_t size);

/* ── Startup payloads (always-run list, stored in autoload.txt) ──────────────
 * Separate from autoload profiles: these payloads launch on every boot
 * regardless of the active/selected profile. */
int pldmgr_startup_is_enabled(const char *filename);
void pldmgr_startup_set(const char *filename, int enabled);
int pldmgr_startup_list_json(char *buf, size_t size);
