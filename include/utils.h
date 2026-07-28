#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Gets the local IP address. Returns 0 on success, -1 on failure. */
int pldmgr_get_local_ip(char *ip_buf, size_t buf_size);
void pldmgr_utils_get_payload_folder_name(const char *filename, char *out_buf, size_t out_size);
void pldmgr_json_escape(const char *src, char *dst, size_t dst_size);

/* Best-effort read of the console firmware version as a "MAJOR.MINOR" string
 * (e.g. "7.61"). Returns 0 on success, -1 if it could not be determined (in
 * which case out is set to ""). Callers must degrade gracefully. */
int pldmgr_get_system_fw(char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
