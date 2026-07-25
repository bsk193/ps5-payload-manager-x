#pragma once

/* ── Build identity ────────────────────────────────────────────
 * Define PLDMGRX (e.g. `make ... PLDMGRX=1`, which passes -DPLDMGRX) to build
 * "Payload Manager X": a second instance that can be installed and run next to
 * a stock Payload Manager without colliding. It uses a different HTTP port,
 * home-screen app id/name, and data directory (/data/pldmgrx), so the two never
 * share config, profiles, or the autoload scratch file.
 * Without the flag, the build is identical to stock. */
#ifdef PLDMGRX
  /* Port is chosen at build time (make ... PLDMGRX_PORT=8084|8184). 8084 makes a
   * drop-in replacement for the official manager; 8184 runs alongside it. */
  #ifdef MENU_PORT_OVERRIDE
    #define MENU_PORT MENU_PORT_OVERRIDE
  #else
    #define MENU_PORT 8084
  #endif
  #define PLDMGR_DATA_ROOT "/data/pldmgrx"
  /* PS5 title IDs must be 4 uppercase letters + 5 digits (like the official
   * PLDM00001). A malformed id (e.g. PLDMGRX01) is rejected and no app tile is
   * created. PLDM00002 keeps the proven prefix but a distinct number. */
  #define PLDMGR_TITLE_ID  "PLDM00002"
  #define PLDMGR_APP_NAME  "Payload Manager X"
#else
  #define MENU_PORT        8084
  #define PLDMGR_DATA_ROOT "/data/pldmgr"
  #define PLDMGR_TITLE_ID  "PLDM00001"
  #define PLDMGR_APP_NAME  "Payload Manager"
#endif

/* Network Settings */
#define ELFLDR_PORT 9021

/* Routes */
#define ROUTE_INDEX "/"
#define ROUTE_INDEX_HTML "/index.html"
#define ROUTE_LIST_PAYLOADS "/list_payloads"
#define ROUTE_UPLOAD "/manage:upload"
#define ROUTE_CHECK "/manage:check"
#define ROUTE_DELETE "/manage:delete"
#define ROUTE_LOAD_PAYLOAD "/loadpayload:"
#define ROUTE_SHUTDOWN "/shutdown"
#define ROUTE_LOG "/log"
#define ROUTE_VERSION "/version"
#define ROUTE_GETIP "/getip"
#define ROUTE_GET_CONFIG "/get_config"
#define ROUTE_SET_CONFIG "/set_config"
#define ROUTE_ABORT "/abort"
#define ROUTE_AUTOLOAD_STATUS "/autoload_status"
#define ROUTE_AUTOLOAD_CLEAR "/autoload_clear"
#define ROUTE_PROFILES_GET "/profiles_get"
#define ROUTE_PROFILES_SET "/profiles_set"
#define ROUTE_PROFILE_RUN "/profile_run"
#define ROUTE_REPO_LIST "/repository_payloads"
#define ROUTE_REPO_REFRESH "/repository_refresh"
#define ROUTE_REPO_INSTALL "/repository_install"
#define ROUTE_REPO_PUSH "/repository_push"
#define ROUTE_REPO_INSTALL_PUSH "/repository_install_push"
#define ROUTE_SOURCES_LIST "/sources_list"
#define ROUTE_SOURCES_SET "/sources_set"
#define ROUTE_SOURCES_ADD "/sources_add"
#define ROUTE_SOURCES_REMOVE "/sources_remove"
#define ROUTE_USB_MOVE_CHECK "/usb_move_check"
#define ROUTE_USB_MOVE_PERFORM "/usb_move_perform"
#define ROUTE_CACHE_MANIFEST "/cache.appcache"

#define ROUTE_PROCESSES_LIST "/processes_list"
#define ROUTE_PROCESS_KILL "/process_kill"
#define ROUTE_HISTORY_LIST "/history_list"

#define MENU_VERSION "0.5.0-x"
#define AUTOLOAD_CONFIG_PATH PLDMGR_DATA_ROOT "/autoload.txt"
#define PROFILES_CONFIG_PATH PLDMGR_DATA_ROOT "/profiles.json"
#define PLDMGR_CONFIG_PATH PLDMGR_DATA_ROOT "/pldmgr_config.txt"
#define REPOSITORY_CACHE_PATH PLDMGR_DATA_ROOT "/repository_cache.json"
#define PAYLOADS_STORAGE_DIR PLDMGR_DATA_ROOT "/payloads"
#define REPOSITORY_SOURCE_URL                                                  \
  "https://bsk193.github.io/ps5-payloads-mirror/payloads.json"
#define REPOSITORY_REFRESH_INTERVAL_SEC 86400

/* Logging (implementation in log_server.c) */
void pldmgr_log(const char *fmt, ...);
int pldmgr_server_is_active();

#include "autoload.h"
#include "notification.h"
#include "utils.h"

/* Paths */
#define BASE_DATA_DIR PLDMGR_DATA_ROOT
#define SOURCES_CONFIG_PATH PLDMGR_DATA_ROOT "/sources.json"
#define MAX_SOURCES 50

/* Scan Locations (Internal + 8 USB ports).
 * The internal dir follows the build's data root; USB payload dirs are shared. */
static const char *SCAN_DIRS[] = {
    PLDMGR_DATA_ROOT,   "/mnt/usb0/pldmgr", "/mnt/usb1/pldmgr",
    "/mnt/usb2/pldmgr", "/mnt/usb3/pldmgr", "/mnt/usb4/pldmgr",
    "/mnt/usb5/pldmgr", "/mnt/usb6/pldmgr", "/mnt/usb7/pldmgr"};
#define SCAN_DIRS_COUNT 9

/* Messages */
#define MSG_OK "OK"
