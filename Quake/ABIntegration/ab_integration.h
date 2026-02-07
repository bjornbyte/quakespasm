/*
 * AccelByte SDK Integration for QuakeSpasm
 * C wrapper for AccelByte C++ SDK
 */

#ifndef AB_INTEGRATION_H
#define AB_INTEGRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Login status enum */
typedef enum {
    AB_LOGIN_IDLE,          /* Not started */
    AB_LOGIN_IN_PROGRESS,   /* Login request sent, waiting for response */
    AB_LOGIN_QUEUED,        /* In login queue, waiting for turn */
    AB_LOGIN_SUCCESS,       /* Successfully logged in */
    AB_LOGIN_FAILED         /* Login failed */
} ab_login_status_t;

/* Matchmaking status enum */
typedef enum {
    AB_MM_IDLE,             /* Not matchmaking */
    AB_MM_SEARCHING,        /* Searching for a match */
    AB_MM_FOUND,            /* Match found */
    AB_MM_CANCELLED,        /* Matchmaking cancelled */
    AB_MM_ERROR             /* Matchmaking error */
} ab_matchmake_status_t;

/*
 * Initialize AccelByte SDK
 * Reads configuration from cvars: ab_server_url, ab_client_id, ab_client_secret
 * Call once during Host_Init()
 */
void AB_Init(void);

/*
 * Shutdown AccelByte SDK
 * Call during Host_Shutdown()
 */
void AB_Shutdown(void);

/*
 * Initiate login with device ID
 * This is async - check status with AB_GetLoginStatus()
 */
void AB_LoginWithDeviceId(void);

/*
 * Process async callbacks
 * Call each frame in _Host_Frame()
 */
void AB_Update(void);

/*
 * Get current login status
 */
ab_login_status_t AB_GetLoginStatus(void);

/*
 * Get user ID after successful login
 * Returns NULL if not logged in
 */
const char* AB_GetUserId(void);

/*
 * Get display name after successful login
 * Returns NULL if not logged in
 */
const char* AB_GetDisplayName(void);

/*
 * Get error message if login failed
 * Returns NULL if no error
 */
const char* AB_GetErrorMessage(void);

/*
 * Update a user stat item value.
 * Calls AccelByte's update_user_stat_item_value_v2.
 * stat_code: the stat code identifier (e.g., "kills", "headshots")
 * value: the value to apply
 * strategy: 0 = OVERRIDE, 1 = INCREMENT, 2 = MAX, 3 = MIN
 */
void AB_UpdateUserStatItemValue(const char* stat_code, float value, int strategy);

/*
 * Check if SDK is initialized
 */
int AB_IsInitialized(void);

/*
 * Create a matchmaking ticket
 * This is async - check status with AB_GetMatchmakingStatus()
 */
void AB_CreateMatchTicket(void);

/*
 * Cancel the active matchmaking ticket
 */
void AB_CancelMatchTicket(void);

/*
 * Get current matchmaking status
 */
ab_matchmake_status_t AB_GetMatchmakingStatus(void);

/*
 * Get the match ticket ID
 * Returns NULL if no active ticket
 */
const char* AB_GetMatchTicketId(void);

void* get_current_user(void);

#ifdef __cplusplus
}
#endif

#endif /* AB_INTEGRATION_H */
