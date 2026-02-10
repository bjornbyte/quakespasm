# AccelByte Matchmaking Integration Plan

## Context

The QuakeSpasm project has a matchmaking menu that currently displays "searching for a match..." but doesn't actually submit a matchmaking ticket to AccelByte or connect to the lobby for notifications. The goal is to:

1. Connect to AccelByte Lobby to receive match found notifications
2. Submit a real matchmaking ticket via the AccelByte Match2 service
3. Update the UI from "searching for a match..." to "match found!" when a match is found

This work builds on the existing AccelByte SDK integration in `Quake/ABIntegration/` which already handles authentication and stats. Later, we'll add code to connect to the server, but for now we're only focused on finding the match.

## Implementation Approach

### Phase 1: Add Lobby Connection Management

**File: `Quake/ABIntegration/ab_integration.cpp`**

Add lobby connection infrastructure:
- Create global state for lobby connection (`g_lobby_connection`)
- Create a lobby instance (`g_lobby`)
- Implement a message handler class that extends `TypedMessageHandler<OnMatchFound>`
- Add lobby connection/disconnection logic
- Ensure lobby connection is established after successful login

The lobby connection is required because AccelByte sends match found notifications through the lobby WebSocket connection, not as a direct HTTP response.

### Phase 2: Implement Match Found Notification Handler

**File: `Quake/ABIntegration/ab_integration.cpp`**

Create a notification handler class:
```cpp
class MatchFoundHandler : public accelbyte::lobby::TypedMessageHandler<accelbyte::lobby::notifications::OnMatchFound>
{
    void handle(const accelbyte::lobby::notifications::OnMatchFound& message) override;
};
```

When a match is found:
1. Update `g_matchmake_status` to `AB_MM_FOUND`
2. Store match details (match_id, game session info) for later use
3. Queue a console message on the main thread via `ABTaskRunner`

### Phase 3: Implement Real Match Ticket Creation

**File: `Quake/ABIntegration/ab_integration.cpp`**

Update `AB_CreateMatchTicket()` function to:
1. Add a cvar `ab_match_pool` for configuring the match pool name
2. Build a `CreateMatchTicket` request with:
   - Match pool from cvar
   - User latencies (can be empty for now)
   - Optional attributes
3. Call `accelbyte::match2::MatchTickets::create_match_ticket()`
4. Handle success callback:
   - Store ticket ID from response
   - Update status to `AB_MM_SEARCHING`
5. Handle error callback:
   - Update status to `AB_MM_ERROR`
   - Store error message for display

**File: `Quake/ABIntegration/ab_integration.h`**

Add function to retrieve error messages:
- `const char* AB_GetMatchmakingError()` - returns the error message if status is `AB_MM_ERROR`

### Phase 4: Implement Match Ticket Cancellation

**File: `Quake/ABIntegration/ab_integration.cpp`**

Update `AB_CancelMatchTicket()` function to:
1. Check if we have an active ticket ID
2. Call `accelbyte::match2::MatchTickets::delete_match_ticket()`
3. Handle success: update status to `AB_MM_CANCELLED`
4. Handle error: log error but still mark as cancelled locally

### Phase 5: Update Menu to Display Match Found and Errors

**File: `Quake/menu.c`**

Modify `M_Matchmake_Draw()` function (line 2593-2604) to:
1. Query matchmaking status via `AB_GetMatchmakingStatus()`
2. Display different text based on status:
   - `AB_MM_SEARCHING`: "Searching for a match..."
   - `AB_MM_FOUND`: "Match found!"
   - `AB_MM_ERROR`: "Error: [retrieved from AB_GetMatchmakingError()]"
   - `AB_MM_CANCELLED`: "Matchmaking cancelled"
3. For error state, display the actual error message from the AccelByte API (e.g., "Invalid match pool" or "Matchmaking service unavailable")

This already follows the existing pattern - the menu system redraws every frame and polls current state.

### Phase 6: Add Lobby Reading in Main Loop

**File: `Quake/ABIntegration/ab_integration.cpp`**

Update `AB_Update()` function (line 312-342) to:
1. Check if lobby connection exists and is connected
2. Call `lobby_connection->read()` to process incoming messages
3. This allows the lobby to dispatch OnMatchFound notifications

## Critical Files to Modify

1. **`Quake/ABIntegration/ab_integration.h`**
   - Add function to get match found details: `AB_GetMatchId()`, `AB_GetMatchSessionId()`
   - Add error getter: `AB_GetMatchmakingError()` - returns error message if status is `AB_MM_ERROR`
   - Add any additional status getters if needed

2. **`Quake/ABIntegration/ab_integration.cpp`**
   - Add lobby connection management
   - Implement MatchFoundHandler class
   - Update `AB_Init()` to add `ab_match_pool` cvar
   - Update `AB_LoginWithDeviceId()` success callback to connect to lobby
   - Implement real `AB_CreateMatchTicket()` with Match2 API
   - Implement real `AB_CancelMatchTicket()` with Match2 API
   - Update `AB_Update()` to read lobby messages
   - Add `AB_Shutdown()` cleanup for lobby connection

3. **`Quake/menu.c`** (lines 2593-2604)
   - Update `M_Matchmake_Draw()` to show dynamic status text

## Required AccelByte SDK Headers

Add these includes to `ab_integration.cpp`:
```cpp
#include <accelbyte/lobby/Lobby.h>
#include <accelbyte/lobby/LobbyConnection.h>
#include <accelbyte/lobby/TypedMessageHandler.h>
#include <accelbyte/lobby/notifications/OnMatchFound.h>
#include <accelbyte/match2/MatchTickets.h>
#include <accelbyte/match2/match_tickets/CreateMatchTicket.h>
#include <accelbyte/match2/match_tickets/DeleteMatchTicket.h>
#include <accelbyte/match2/models/MatchTicketRequest.h>
#include <accelbyte/match2/models/MatchTicket.h>
```

## Configuration

Add new cvar:
- `ab_match_pool` (default: "") - The match pool name configured in AccelByte admin portal

## Data Flow

```
User selects Matchmaking
    ↓
M_Menu_Matchmake_f() calls AB_CreateMatchTicket()
    ↓
AB_CreateMatchTicket() submits ticket to Match2 service
    ↓
Match2 service processes matchmaking
    ↓
When match found: Lobby receives OnMatchFound notification
    ↓
MatchFoundHandler updates g_matchmake_status to AB_MM_FOUND
    ↓
M_Matchmake_Draw() (called every frame) polls status and displays "Match found!"
```

## Testing Approach

1. **Prerequisites:**
   - Valid AccelByte credentials configured via cvars
   - Match pool created in AccelByte admin portal
   - `ab_match_pool` cvar set to match pool name

2. **Test Steps:**
   - Launch game and authenticate (existing functionality)
   - Navigate to Multiplayer → Matchmaking
   - Verify console shows "Creating match ticket..." message
   - Verify menu displays "Searching for a match..."
   - Wait for matchmaking (or use AccelByte admin tools to force a match)
   - Verify menu updates to "Match found!" when match is ready
   - Test cancellation by pressing ESC during search
   - Verify error handling by using invalid match pool name

3. **Edge Cases to Handle:**
   - User not logged in when creating ticket
   - Invalid match pool name
   - Network errors during ticket creation
   - Lobby disconnection during matchmaking
   - Multiple rapid ticket creation/cancellation

## Follow-up Work (Not in This Plan)

After match is found, we'll need to:
1. Join the AccelByte game session associated with the match
2. Extract server connection details from the game session
3. Connect to the dedicated server using Quake's networking layer
4. Transition from menu to in-game state
5. Handle connection failures and timeouts

## Notes

- The existing pattern of polling status in the draw function (every frame) is the correct approach for Quake's immediate-mode GUI
- We use `ABTaskRunner` to queue console messages from worker threads to avoid thread-safety issues
- The lobby connection must remain active during matchmaking to receive notifications
- Match2 and Lobby are separate services - Match2 handles ticket submission, Lobby handles real-time notifications
- Error handling is critical: when ticket creation fails, update status to `AB_MM_ERROR` and store the error message so the menu can display it to the user
- Error messages should be user-friendly and help diagnose configuration issues (invalid match pool, missing credentials, network errors, etc.)
