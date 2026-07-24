#pragma once

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
