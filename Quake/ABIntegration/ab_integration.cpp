/*
 * AccelByte SDK Integration for QuakeSpasm
 * C++ implementation
 */

#include "ab_integration.h"

// AccelByte SDK headers - Match2 first to avoid parse issues
#include <accelbyte/match2/MatchTickets.h>
#include <accelbyte/match2/match_tickets/CreateMatchTicket.h>
#include <accelbyte/match2/match_tickets/DeleteMatchTicket.h>
#include <accelbyte/match2/models/MatchTicketRequest.h>
#include <accelbyte/match2/models/MatchTicket.h>

// Other AccelByte SDK headers
#include <accelbyte/common/String.h>
#include <accelbyte/user/UserLogin.h>
#include <accelbyte/user/User.h>
#include <accelbyte/user/parameters/LoginWithDeviceId.h>
#include <accelbyte/settings/InMemorySettings.h>
#include <accelbyte/settings/global_settings.h>
#include <accelbyte/common/Error.h>
#include <accelbyte/iam/models/LoginQueueTicket.h>
#include <accelbyte/curl_http_executor/CurlRequestExecutorFactory.h>
#include <accelbyte/http/RequestExecutorFactory.h>
#include <accelbyte/social/UserStatistic.h>
#include <accelbyte/social/user_statistic/UpdateUserStatItemValueV2.h>
#include <accelbyte/social/models/UpdateStatItem.h>
#include <accelbyte/lobby/Lobby.h>
#include <accelbyte/lobby/LobbyConnection.h>
#include <accelbyte/lobby/TypedMessageHandler.h>
#include <accelbyte/lobby/notifications/OnMatchFound.h>
#include <accelbyte/memory/memory.h>
#include <accelbyte/web_socket/WebSocketFactory.h>
#include <accelbyte/cpp_web_socket/CppWebSocketFactory.h>

#include "ab_task_runner.h"

// Standard library
#include <string>
#include <mutex>
#include <future>

// Quake headers (C linkage)
extern "C" {
#include "quakedef.h"
#include "console.h"
}

// Forward declarations for Quake C functions
extern "C" {
    extern cvar_t* Cvar_FindVar(const char* var_name);
    extern void Cvar_RegisterVariable(cvar_t* variable);
    extern double Sys_DoubleTime(void);
}

//------------------------------------------------------------------------------
// CVars for AccelByte configuration
//------------------------------------------------------------------------------
static cvar_t ab_server_url = {"ab_server_url", "", CVAR_ARCHIVE, 0.0f, NULL, NULL, NULL};
static cvar_t ab_client_id = {"ab_client_id", "", CVAR_ARCHIVE, 0.0f, NULL, NULL, NULL};
static cvar_t ab_client_secret = {"ab_client_secret", "", CVAR_ARCHIVE, 0.0f, NULL, NULL, NULL};
static cvar_t ab_match_pool = {"ab_match_pool", "", CVAR_ARCHIVE, 0.0f, NULL, NULL, NULL};

//------------------------------------------------------------------------------
// Internal state
//------------------------------------------------------------------------------
static std::mutex g_mutex;
static ab_login_status_t g_login_status = AB_LOGIN_IDLE;
static std::string g_user_id;
static std::string g_display_name;
static std::string g_error_message;
static std::string g_device_id;
static bool g_initialized = false;

// Login queue handling
static accelbyte::memory::SharedPtr<accelbyte::iam::model::LoginQueueTicket> g_queue_ticket;
static double g_last_queue_poll = 0.0;

// User credential storage
static accelbyte::memory::SharedPtr<accelbyte::user::User> g_current_user;

// Settings instance
static accelbyte::settings::InMemorySettings g_settings;

// Matchmaking state
static ab_matchmake_status_t g_matchmake_status = AB_MM_IDLE;
static std::string g_match_ticket_id;
static std::string g_matchmake_error;
static std::string g_match_id;

// Lobby connection
static accelbyte::memory::SharedPtr<accelbyte::lobby::LobbyConnection> g_lobby_connection;
static accelbyte::memory::SharedPtr<accelbyte::lobby::Lobby> g_lobby;

static ABTaskRunner runner;
//------------------------------------------------------------------------------
// Device ID generation (Windows)
//------------------------------------------------------------------------------
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <accelbyte/crypto/md5.h>

static std::string GenerateDeviceId()
{
    std::string combined;

    // Get machine GUID from registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS)
    {
        char guid[256] = {0};
        DWORD size = sizeof(guid);
        if (RegQueryValueExA(hKey, "MachineGuid", NULL, NULL, (LPBYTE)guid, &size) == ERROR_SUCCESS)
        {
            combined += guid;
        }
        RegCloseKey(hKey);
    }

    // Get volume serial number of C:
    DWORD serial = 0;
    if (GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0))
    {
        combined += std::to_string(serial);
    }

    // If we couldn't get any system info, use a fallback
    if (combined.empty())
    {
        combined = "quakespasm-default-device";
    }

    // Hash with MD5 for a consistent format
    accelbyte::String ab_combined(combined.c_str());
    return accelbyte::crypto::md5(ab_combined).c_str();
}
#else
// Unix/Linux implementation
#include <fstream>
#include <accelbyte/crypto/md5.h>

static std::string GenerateDeviceId()
{
    std::string machine_id;

    // Try to read /etc/machine-id (systemd)
    std::ifstream file("/etc/machine-id");
    if (file.is_open())
    {
        std::getline(file, machine_id);
        file.close();
    }

    // Fallback to /var/lib/dbus/machine-id
    if (machine_id.empty())
    {
        std::ifstream dbus_file("/var/lib/dbus/machine-id");
        if (dbus_file.is_open())
        {
            std::getline(dbus_file, machine_id);
            dbus_file.close();
        }
    }

    // If we couldn't get any system info, use a fallback
    if (machine_id.empty())
    {
        machine_id = "quakespasm-default-device";
    }

    // Hash with MD5 for a consistent format
    accelbyte::String ab_machine_id(machine_id.c_str());
    return accelbyte::crypto::md5(ab_machine_id).c_str();
}
#endif

//------------------------------------------------------------------------------
// Lobby message handlers
//------------------------------------------------------------------------------
class MatchFoundHandler : public accelbyte::lobby::TypedMessageHandler<accelbyte::lobby::notifications::OnMatchFound>
{
public:
    void handle(const accelbyte::lobby::notifications::OnMatchFound& message) override
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        // Update matchmaking status and store match details
        g_matchmake_status = AB_MM_FOUND;
        g_match_id = message.match_id.c_str();

        // Queue console message on main thread
        runner.queue_task([match_id = std::string(message.match_id.c_str())](
                          const accelbyte::String& dummy1,
                          const accelbyte::String& dummy2,
                          const accelbyte::String& dummy3){
            Con_Printf("AccelByte: Match found! Match ID: %s\n", match_id.c_str());
        }, accelbyte::String(""), accelbyte::String(""), accelbyte::String(""));
    }
};

static accelbyte::memory::SharedPtr<MatchFoundHandler> g_match_found_handler;

//------------------------------------------------------------------------------
// Callback handlers
//------------------------------------------------------------------------------
static void OnLoginSuccess(const accelbyte::memory::SharedPtr<accelbyte::user::User> user)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    g_current_user = user;
    g_user_id = user->user_id().c_str();
    g_display_name = user->display_name().c_str();
    g_login_status = AB_LOGIN_SUCCESS;
    g_queue_ticket = nullptr;

    // Create lobby and connect for match notifications
    g_lobby = accelbyte::memory::make_shared_ptr<accelbyte::lobby::Lobby>();
    if (!g_lobby)
    {
        Con_Printf("AccelByte: Failed to create Lobby instance\n");
    }
    else
    {
        Con_Printf("AccelByte: Lobby instance created\n");
        g_lobby_connection = g_lobby->create_connection(*user);
        if (!g_lobby_connection)
        {
            Con_Printf("AccelByte: Failed to create LobbyConnection (lobby_url may not be configured)\n");
            Con_Printf("AccelByte: Check that ab_server_url is set correctly\n");
        }
        else
        {
            Con_Printf("AccelByte: LobbyConnection created successfully\n");
            if (g_lobby_connection->connect())
            {
                Con_Printf("AccelByte: Connected to lobby successfully\n");
            }
            else
            {
                Con_Printf("AccelByte: Failed to connect to lobby (WebSocket connection failed)\n");
            }

            // Register match found handler
            if (!g_match_found_handler)
            {
                g_match_found_handler = accelbyte::memory::make_shared_ptr<MatchFoundHandler>();
            }
            if (g_match_found_handler)
            {
                g_lobby_connection->add_message_handler(g_match_found_handler);
                Con_Printf("AccelByte: Match found handler registered\n");
            }
        }
    }

    runner.queue_task([](const accelbyte::String& access_token, const accelbyte::String& displayName, const accelbyte::String& ab_namespace){
        Con_Printf("AccelByte: Login successful! Token: %s\n", access_token.c_str());
        Con_Printf("User Name: %s, Namespace: %s", displayName, ab_namespace );
    }, user->credential()->access_token().value(), user->display_name(), user->user_data()->ab_namespace);
    // Con_Printf("AccelByte: Login successful! User: %s\n", g_display_name);
}

static void OnLoginQueued(const accelbyte::memory::SharedPtr<accelbyte::iam::model::LoginQueueTicket> ticket)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    g_login_status = AB_LOGIN_QUEUED;
    g_queue_ticket = ticket;
    g_last_queue_poll = Sys_DoubleTime();

    Con_Printf("AccelByte: In login queue...\n");
}

static void OnLoginError(const accelbyte::Error& error)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    g_error_message = error.what().c_str();
    g_login_status = AB_LOGIN_FAILED;
    g_queue_ticket = nullptr;

    Con_Printf("AccelByte: Login failed - %s\n", g_error_message.c_str());
}

//------------------------------------------------------------------------------
// Public C API implementation
//------------------------------------------------------------------------------
extern "C" {

void AB_Init(void)
{
    if (g_initialized)
    {
        return;
    }

    // Register cvars
    Cvar_RegisterVariable(&ab_server_url);
    Cvar_RegisterVariable(&ab_client_id);
    Cvar_RegisterVariable(&ab_client_secret);
    Cvar_RegisterVariable(&ab_match_pool);

    // Generate device ID
    g_device_id = GenerateDeviceId();

    Con_Printf("AccelByte: SDK initialized\n");
    Con_Printf("AccelByte: Device ID: %s\n", g_device_id.c_str());

    g_initialized = true;

    // Initialize HTTP executor for REST calls
    auto curlExecutor = std::make_shared<accelbyte::http::CurlRequestExecutorFactory>();
    accelbyte::http::RequestExecutorFactory::set_executor_factory(curlExecutor);

    // Initialize WebSocket factory for lobby connections
    {
        auto factory = accelbyte::memory::make_shared_ptr<accelbyte::cpp_web_socket::CppWebSocketFactory>();
        if (factory)
        {
            accelbyte::web_socket::WebSocketFactory::set_web_socket_factory(factory);
            Con_Printf("AccelByte: WebSocket factory initialized\n");
        }
        else
        {
            Con_Printf("AccelByte: Failed to create WebSocket factory\n");
        }
    }
}

void AB_Shutdown(void)
{
    if (!g_initialized)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    // Disconnect lobby
    if (g_lobby_connection)
    {
        g_lobby_connection->disconnect();
    }
    g_lobby_connection = nullptr;
    g_lobby = nullptr;
    g_match_found_handler = nullptr;

    g_current_user = nullptr;
    g_queue_ticket = nullptr;
    g_login_status = AB_LOGIN_IDLE;
    g_user_id.clear();
    g_display_name.clear();
    g_error_message.clear();
    g_matchmake_status = AB_MM_IDLE;
    g_match_ticket_id.clear();
    g_matchmake_error.clear();
    g_match_id.clear();
    g_initialized = false;

    Con_Printf("AccelByte: SDK shutdown\n");
}

std::future<void> g_dummy_future;
std::future<void> g_match_ticket_future;

void AB_LoginWithDeviceId(void)
{
    if (!g_initialized)
    {
        Con_Printf("AccelByte: SDK not initialized\n");
        return;
    }

    // Check if cvars are configured
    const char* server_url = ab_server_url.string;
    const char* client_id = ab_client_id.string;
    const char* client_secret = ab_client_secret.string;

    if (!server_url || !server_url[0])
    {
        Con_Printf("AccelByte: ab_server_url not configured\n");
        return;
    }
    if (!client_id || !client_id[0])
    {
        Con_Printf("AccelByte: ab_client_id not configured\n");
        return;
    }
    if (!client_secret || !client_secret[0])
    {
        Con_Printf("AccelByte: ab_client_secret not configured\n");
        // return;
    }

    // Configure settings
    g_settings.set_server_url(server_url);
    g_settings.set_client_id(client_id);
    // g_settings.set_client_secret(client_secret);

    // Configure lobby URL from server_url
    // Convert http/https to ws/wss and append /lobby path
    std::string lobby_url = server_url;
    size_t pos = lobby_url.find("https://");
    if (pos != std::string::npos)
    {
        lobby_url.replace(pos, 8, "wss://");
    }
    else
    {
        pos = lobby_url.find("http://");
        if (pos != std::string::npos)
        {
            lobby_url.replace(pos, 7, "ws://");
        }
    }

    // Remove trailing slash if present
    if (!lobby_url.empty() && lobby_url.back() == '/')
    {
        lobby_url.pop_back();
    }

    // Add /lobby path
    lobby_url += "/lobby/";

    g_settings.set_lobby_url(lobby_url.c_str());
    Con_Printf("AccelByte: Lobby URL set to: %s\n", lobby_url.c_str());

    // Set as global settings
    accelbyte::settings::set_global_settings(g_settings);

    // Login with device ID
    Con_Printf("AccelByte: Logging in with device ID...\n");

    accelbyte::user::parameters::LoginWithDeviceId params;
    params.device_id = g_device_id.c_str();
    params.create_headless = true;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_login_status = AB_LOGIN_IN_PROGRESS;
    }
    
    g_dummy_future = std::async(std::launch::async, [params](){
        accelbyte::user::UserLogin::login_with_device_id(
        params,
        OnLoginSuccess,
        OnLoginQueued,
        OnLoginError
    );
    });
}

void AB_Update(void)
{
    if (!g_initialized)
    {
        return;
    }

    // Read lobby messages to process match found notifications
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_lobby_connection && g_lobby_connection->is_connected())
        {
            try
            {
                g_lobby_connection->read();
            }
            catch (const std::exception& e)
            {
                Con_Printf("AccelByte: WebSocket exception during read: %s\n", e.what());
            }
            catch (...)
            {
                Con_Printf("AccelByte: Unknown exception during WebSocket read\n");
            }
        }
    }

    // std::lock_guard<std::mutex> lock(g_mutex);

    // // Handle login queue polling
    // if (g_login_status == AB_LOGIN_QUEUED && g_queue_ticket)
    // {
    //     double current_time = Sys_DoubleTime();
    //     // TODO: Use ticket's player_polling_time_in_seconds when available
    //     double poll_interval = 5.0; // Default 5 seconds

    //     if (current_time - g_last_queue_poll >= poll_interval)
    //     {
    //         g_last_queue_poll = current_time;

    //         // Poll the queue
    //         accelbyte::user::UserLogin::login_with_queue_ticket(
    //             g_queue_ticket,
    //             OnLoginSuccess,
    //             OnLoginQueued,
    //             OnLoginError
    //         );
    //     }
    // }
    runner.execute_task_queue();
}

ab_login_status_t AB_GetLoginStatus(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_login_status;
}

const char* AB_GetUserId(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_login_status == AB_LOGIN_SUCCESS && !g_user_id.empty())
    {
        return g_user_id.c_str();
    }
    return NULL;
}

const char* AB_GetDisplayName(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_login_status == AB_LOGIN_SUCCESS && !g_display_name.empty())
    {
        return g_display_name.c_str();
    }
    return NULL;
}

const char* AB_GetErrorMessage(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_login_status == AB_LOGIN_FAILED && !g_error_message.empty())
    {
        return g_error_message.c_str();
    }
    return NULL;
}

void AB_UpdateUserStatItemValue(const char* stat_code, float value, int strategy)
{
    if (!g_initialized)
    {
        Con_Printf("AccelByte: SDK not initialized\n");
        return;
    }

    if (!stat_code || !stat_code[0])
    {
        Con_Printf("AccelByte: stat_code is empty\n");
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_login_status != AB_LOGIN_SUCCESS || !g_current_user)
    {
        Con_Printf("AccelByte: Not logged in, cannot update stat\n");
        return;
    }

    using UpdateStrategy = accelbyte::social::model::UpdateStatItem::UpdateStrategy;
    UpdateStrategy update_strategy;
    switch (strategy)
    {
    case 0:  update_strategy = UpdateStrategy::OVERRIDE;  break;
    case 1:  update_strategy = UpdateStrategy::INCREMENT;  break;
    case 2:  update_strategy = UpdateStrategy::MAX;        break;
    case 3:  update_strategy = UpdateStrategy::MIN;        break;
    default:
        Con_Printf("AccelByte: Invalid strategy %d (use 0=OVERRIDE, 1=INCREMENT, 2=MAX, 3=MIN)\n", strategy);
        return;
    }

    accelbyte::social::user_statistic::UpdateUserStatItemValueV2 request;
    request.stat_code = stat_code;
    request.user_id = g_user_id.c_str();
    request.body.update_strategy = update_strategy;
    request.body.value = value;

    const accelbyte::tls::SecurityAuthorization& authorization = *g_current_user;

    std::string stat_code_copy(stat_code);

    accelbyte::social::UserStatistic::update_user_stat_item_value_v2(
        authorization,
        request,
        [stat_code_copy](const accelbyte::social::model::StatItemInc& result) {
            Con_Printf("AccelByte: Stat '%s' updated, current value: %f\n",
                stat_code_copy.c_str(), result.current_value);
        },
        [stat_code_copy](const accelbyte::Error& error) {
            Con_Printf("AccelByte: Failed to update stat '%s' - %s\n",
                stat_code_copy.c_str(), error.what().c_str());
        }
    );
}

int AB_IsInitialized(void)
{
    return g_initialized ? 1 : 0;
}

void AB_CreateMatchTicket(void)
{
    if (!g_initialized)
    {
        Con_Printf("AccelByte: SDK not initialized\n");
        return;
    }

    if (AB_GetLoginStatus() != AB_LOGIN_SUCCESS)
    {
        Con_Printf("AccelByte: Not logged in\n");
        return;
    }

    const char* match_pool = ab_match_pool.string;
    if (!match_pool || !match_pool[0])
    {
        Con_Printf("AccelByte: ab_match_pool not configured\n");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_matchmake_status = AB_MM_SEARCHING;
        g_match_ticket_id.clear();
        g_matchmake_error.clear();
        g_match_id.clear();
    }

    Con_Printf("AccelByte: Creating match ticket for pool '%s'...\n", match_pool);

    std::string pool_copy(match_pool);

    g_match_ticket_future = std::async(std::launch::async, [pool_copy](){
        if (!g_current_user)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_matchmake_status = AB_MM_ERROR;
            g_matchmake_error = "User not authenticated";
            Con_Printf("AccelByte: Cannot create match ticket - user not authenticated\n");
            return;
        }

        try
        {
            // Create match ticket request
            accelbyte::match2::match_tickets::CreateMatchTicket request;
            request.body.match_pool = pool_copy.c_str();
            // User latencies can be empty for now
            // request.body.latencies = ...
            // Optional attributes can be added here
            // request.body.attributes = ...

            const accelbyte::tls::SecurityAuthorization& authorization = *g_current_user;

            Con_Printf("AccelByte: Submitting match ticket request for pool '%s'...\n", pool_copy.c_str());

            accelbyte::match2::MatchTickets::create_match_ticket(
                authorization,
                request,
                [pool_copy](const accelbyte::match2::model::MatchTicket& ticket) {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_match_ticket_id = ticket.match_ticket_id.c_str();
                    Con_Printf("AccelByte: Match ticket created successfully!\n");
                    Con_Printf("AccelByte: Ticket ID: %s\n", ticket.match_ticket_id.c_str());
                    Con_Printf("AccelByte: Queue time: %d\n", ticket.queue_time);
                },
                [pool_copy](const accelbyte::Error& error) {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_matchmake_status = AB_MM_ERROR;
                    g_matchmake_error = error.what().c_str();
                    Con_Printf("AccelByte: Failed to create match ticket - %s\n", error.what().c_str());
                }
            );
        }
        catch (const std::exception& e)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_matchmake_status = AB_MM_ERROR;
            g_matchmake_error = e.what();
            Con_Printf("AccelByte: Exception creating match ticket - %s\n", e.what());
        }
    });
}

void AB_CancelMatchTicket(void)
{
    if (!g_initialized)
    {
        return;
    }

    std::string ticket_id_copy;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        ticket_id_copy = g_match_ticket_id;
        if (ticket_id_copy.empty())
        {
            // No active ticket, just mark as cancelled
            g_matchmake_status = AB_MM_CANCELLED;
            Con_Printf("AccelByte: Matchmaking cancelled\n");
            return;
        }
        g_matchmake_status = AB_MM_CANCELLED;
    }

    Con_Printf("AccelByte: Cancelling match ticket %s...\n", ticket_id_copy.c_str());

    g_match_ticket_future = std::async(std::launch::async, [ticket_id_copy](){
        if (!g_current_user)
        {
            Con_Printf("AccelByte: Cannot cancel match ticket - user not authenticated\n");
            return;
        }

        try
        {
            const accelbyte::tls::SecurityAuthorization& authorization = *g_current_user;

            accelbyte::match2::match_tickets::DeleteMatchTicket request;
            request.ticketid = ticket_id_copy.c_str();

            Con_Printf("AccelByte: Submitting cancel request for ticket %s...\n", ticket_id_copy.c_str());

            accelbyte::match2::MatchTickets::delete_match_ticket(
                authorization,
                request,
                []() {
                    Con_Printf("AccelByte: Match ticket cancelled successfully\n");
                },
                [ticket_id_copy](const accelbyte::Error& error) {
                    Con_Printf("AccelByte: Error cancelling match ticket - %s\n", error.what().c_str());
                }
            );
        }
        catch (const std::exception& e)
        {
            Con_Printf("AccelByte: Exception cancelling match ticket - %s\n", e.what());
        }
    });
}

ab_matchmake_status_t AB_GetMatchmakingStatus(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_matchmake_status;
}

const char* AB_GetMatchTicketId(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_match_ticket_id.empty())
    {
        return g_match_ticket_id.c_str();
    }
    return NULL;
}

const char* AB_GetMatchId(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_match_id.empty())
    {
        return g_match_id.c_str();
    }
    return NULL;
}

const char* AB_GetMatchmakingErrorMessage(void)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_matchmake_status == AB_MM_ERROR && !g_matchmake_error.empty())
    {
        return g_matchmake_error.c_str();
    }
    return NULL;
}

void* get_current_user(void)
{
    return nullptr;
}

} // extern "C"
