# QuakeSpasm Player Statistics Tracking System

## Overview

QuakeSpasm tracks player statistics through a distributed system spanning server-side QuakeC/C code and client-side C code. Statistics include kills/frags, deaths, items picked up, weapon usage, damage taken/dealt, and various game progress metrics. The system uses a network message-based synchronization model where the server continuously broadcasts stat updates to all connected clients.

---

## 1. Statistics Data Structures

### 1.1 Server-Side Player Entity Data (edict_t)

**Location:** `Quake/progs.h:40-59`, `Quake/progdefs.q1:62-141`

The primary stats storage is in the `entvars_t` structure embedded within each player edict:

| Field | Type | Description |
|-------|------|-------------|
| `frags` (line 91) | float | Total kill count (displayed as integer) |
| `health` (line 90) | float | Current player health (0 = dead) |
| `armorvalue` (line 120) | float | Current armor points (0-300) |
| `ammo_shells` (line 96) | float | Shotgun ammo count |
| `ammo_nails` (line 97) | float | Nailgun ammo count |
| `ammo_rockets` (line 98) | float | Rocket launcher ammo count |
| `ammo_cells` (line 99) | float | Lightning ammo count |
| `currentammo` (line 95) | float | Current weapon's ammo count |
| `items` (line 100) | float | Inventory bit flags (weapons, armor, power-ups) |
| `weapon` (line 92) | float | Current weapon ID |
| `weaponmodel` (line 93) | string | Weapon model name reference |
| `weaponframe` (line 94) | float | Animation frame for current weapon |
| `dmg_take` (line 130) | float | Damage taken this frame (accumulated) |
| `dmg_save` (line 131) | float | Armor damage absorption this frame |
| `dmg_inflictor` (line 132) | int | Entity reference of damage source |
| `deadflag` (line 103) | float | Death state (0=alive, 1=dying, 2=dead) |

**Extended fields (from items2):**
- Additional inventory tracking via `GetEdictFieldValue(ent, "items2")` for expanded item support

### 1.2 Server-Side Client Connection State (client_t)

**Location:** `Quake/server.h:96-127`

| Field | Type | Description |
|-------|------|-------------|
| `old_frags` (line 126) | int | Previous frame's frag count (delta detection) |
| `edict` (line 115) | edict_t* | Pointer to player's game entity |
| `name` (line 116) | char[32] | Player name |
| `colors` (line 117) | int | Player color scheme (4-bit hi/lo) |
| `spawn_parms[NUM_SPAWN_PARMS]` (line 123) | float[16] | Persistent stats across level changes |
| `message` (line 112) | sizebuf_t | Outgoing message buffer for this client |

### 1.3 Client-Side Stats Array (client_state_t)

**Location:** `Quake/client.h:142-227`

| Field | Type | Description |
|-------|------|-------------|
| `stats[MAX_CL_STATS]` (line 151) | int[32] | Client's local stat cache (see stat indices below) |
| `items` (line 152) | int | Inventory bit flags |
| `item_gettime[32]` (line 153) | float[32] | Acquisition time for each item (for HUD blink effect) |

**Stats Array Indices (from quakedef.h:107-122):**
- `STAT_HEALTH` (0): Player health
- `STAT_FRAGS` (1): Kill count
- `STAT_WEAPON` (2): Current weapon
- `STAT_AMMO` (3): Current weapon ammo
- `STAT_ARMOR` (4): Armor points
- `STAT_WEAPONFRAME` (5): Weapon animation frame
- `STAT_SHELLS` (6): Shotgun ammo
- `STAT_NAILS` (7): Nailgun ammo
- `STAT_ROCKETS` (8): Rocket ammo
- `STAT_CELLS` (9): Lightning ammo
- `STAT_ACTIVEWEAPON` (10): Active weapon bitfield
- `STAT_TOTALSECRETS` (11): Total secrets on map
- `STAT_TOTALMONSTERS` (12): Total monsters on map
- `STAT_SECRETS` (13): Secrets found
- `STAT_MONSTERS` (14): Monsters killed

### 1.4 Client-Side Scoreboard (scoreboard_t)

**Location:** `Quake/client.h:36-43`

| Field | Type | Description |
|-------|------|-------------|
| `name[MAX_SCOREBOARDNAME]` | char[32] | Player name |
| `entertime` | float | When player joined |
| `frags` | int | Player's frag count |
| `colors` | int | Player color scheme |
| `translations[VID_GRADES*256]` | byte array | Color palette translations |

---

## 2. Kill/Frag Tracking

### 2.1 Frag Update Architecture

Frags are stored in the player edict (`edict->v.frags`) and synchronized via a delta-detection mechanism:

```
Server Frame Loop:
  SV_UpdateToReliableMessages() [sv_main.c:1054-1085]
    ├─ Compare edict->v.frags with client->old_frags for each player
    ├─ If different (delta detected):
    │   ├─ Send svc_updatefrags to ALL clients
    │   │   Format: [byte:msg_type][byte:player_id][short:frag_count]
    │   └─ Update client->old_frags = edict->v.frags
    └─ Write reliable_datagram to all client message buffers
```

### 2.2 Server-Side Frag Update Code

**Location:** `Quake/sv_main.c:1054-1085` (SV_UpdateToReliableMessages)

```c
void SV_UpdateToReliableMessages (void)
{
    for (i=0, host_client = svs.clients; i<svs.maxclients; i++, host_client++)
    {
        if (host_client->old_frags != host_client->edict->v.frags)
        {
            // Broadcast to all clients
            for (j=0, client = svs.clients; j<svs.maxclients; j++, client++)
            {
                if (!client->active) continue;
                MSG_WriteByte (&client->message, svc_updatefrags);
                MSG_WriteByte (&client->message, i);  // player index
                MSG_WriteShort (&client->message, host_client->edict->v.frags);
            }
            host_client->old_frags = host_client->edict->v.frags;
        }
    }
}
```

**Key functions:**
- `SV_SendClientMessages()` [sv_main.c:1117] - Main sending loop, calls SV_UpdateToReliableMessages
- `SV_UpdateToReliableMessages()` [sv_main.c:1054] - Delta-detection and broadcast

### 2.3 Client-Side Frag Reception

**Location:** `Quake/cl_parse.c:1159-1165` (CL_ParseServerMessage)

```c
case svc_updatefrags:
    Sbar_Changed ();  // Mark scoreboard for redraw
    i = MSG_ReadByte ();  // player index
    if (i >= cl.maxclients)
        Host_Error ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
    cl.scores[i].frags = MSG_ReadShort ();
    break;
```

**Key functions:**
- `CL_ParseServerMessage()` [cl_parse.c:~700] - Main message parser
- `Sbar_Changed()` [sbar.c:103] - Marks HUD for redraw

### 2.4 QuakeC Integration

Frags are modified directly by QuakeC code in progs.dat (game logic):
- When a player kills another player, the killer's edict->v.frags is incremented
- When suicide occurs, frags may be decremented
- The server detects changes and broadcasts via svc_updatefrags

No special builtin functions; direct field modification is used.

---

## 3. Item Pickup Tracking

### 3.1 Item Pickup Architecture

Item pickups are tracked via inventory bitflags rather than explicit count fields:

```
Player pickups item:
  Server (QuakeC code):
    edict->v.items |= IT_SHOTGUN (or other item flag)

  Server sends svc_clientdata with items field:
    Format: [byte:msg_type][short:flags][...][long:items]

  Client receives and tracks:
    if (new_items & (1<<j)) && !(old_items & (1<<j))
        cl.item_gettime[j] = cl.time  // For blink effect
```

### 3.2 Item Inventory Bit Flags (quakedef.h:124-149)

| Item | Bit | Flag Name |
|------|-----|-----------|
| Shotgun | 0 | IT_SHOTGUN (1) |
| Super Shotgun | 1 | IT_SUPER_SHOTGUN (2) |
| Nailgun | 2 | IT_NAILGUN (4) |
| Super Nailgun | 3 | IT_SUPER_NAILGUN (8) |
| Grenade Launcher | 4 | IT_GRENADE_LAUNCHER (16) |
| Rocket Launcher | 5 | IT_ROCKET_LAUNCHER (32) |
| Lightning | 6 | IT_LIGHTNING (64) |
| Super Lightning | 7 | IT_SUPER_LIGHTNING (128) |
| Shells | 8 | IT_SHELLS (256) |
| Nails | 9 | IT_NAILS (512) |
| Rockets | 10 | IT_ROCKETS (1024) |
| Cells | 11 | IT_CELLS (2048) |
| Axe | 12 | IT_AXE (4096) |
| Armor 1 (green) | 13 | IT_ARMOR1 (8192) |
| Armor 2 (yellow) | 14 | IT_ARMOR2 (16384) |
| Armor 3 (red) | 15 | IT_ARMOR3 (32768) |
| Megahealth | 16 | IT_SUPERHEALTH (65536) |
| Key 1 | 17 | IT_KEY1 (131072) |
| Key 2 | 18 | IT_KEY2 (262144) |
| Invisibility | 19 | IT_INVISIBILITY (524288) |
| Invulnerability | 20 | IT_INVULNERABILITY (1048576) |
| Radiation Suit | 21 | IT_SUIT (2097152) |
| Quad Damage | 22 | IT_QUAD (4194304) |
| Sigil 1-4 | 28-31 | IT_SIGIL1-4 |

### 3.3 Client-Side Item Tracking Code

**Location:** `Quake/cl_parse.c:745-755` (CL_ParseClientdata)

```c
// [always sent] if (bits & SU_ITEMS)
    i = MSG_ReadLong ();

    if (cl.items != i)
    {
        Sbar_Changed ();
        for (j = 0; j < 32; j++)
            if ( (i & (1<<j)) && !(cl.items & (1<<j)))
                cl.item_gettime[j] = cl.time;  // Track acquisition time
        cl.items = i;
    }
```

**Key functions:**
- `CL_ParseClientdata()` [cl_parse.c:~650] - Parses svc_clientdata message
- Items sent every frame via `SV_WriteClientdataToMessage()` [sv_main.c:832]

### 3.4 Item Acquisition Time Tracking

The `cl.item_gettime[32]` array stores the client time when each item was last acquired. This is used for:
- HUD icon blinking effects
- Visual feedback for item pickup

No separate item count statistics are maintained; pickup history is implicit in the items bitfield change.

---

## 4. Shot/Weapon Statistics

### 4.1 Weapon Stats Architecture

Weapon statistics are limited in the base engine:

| Statistic | Storage | Method |
|-----------|---------|--------|
| Current weapon | `edict->v.weapon` | Updated by QuakeC on weapon switch |
| Weapon ammo | `edict->v.currentammo`, `ammo_shells/nails/rockets/cells` | Updated by QuakeC on firing |
| Weapon frame | `edict->v.weaponframe` | Animation frame sent to client |
| Active weapons | `edict->v.items` | Inventory bitflags of owned weapons |

No built-in shot counter or accuracy tracking. These must be implemented by game mods via custom fields.

### 4.2 Weapon Data Transmission

**Location:** `Quake/sv_main.c:832-1006` (SV_WriteClientdataToMessage)

Weapons data is sent in svc_clientdata message:

```c
bits |= SU_WEAPON;  // Always sent if player has weapon

if (bits & SU_WEAPON)
    MSG_WriteByte (msg, SV_ModelIndex(PR_GetString(ent->v.weaponmodel)));

if (bits & SU_WEAPONFRAME)
    MSG_WriteByte (msg, ent->v.weaponframe);

// Ammo counts
MSG_WriteByte (msg, ent->v.currentammo);
MSG_WriteByte (msg, ent->v.ammo_shells);
MSG_WriteByte (msg, ent->v.ammo_nails);
MSG_WriteByte (msg, ent->v.ammo_rockets);
MSG_WriteByte (msg, ent->v.ammo_cells);
```

### 4.3 Client-Side Weapon Stats

**Location:** `Quake/cl_parse.c:761-810` (CL_ParseClientdata)

```c
// Weapon tracking
if (bits & SU_WEAPONFRAME)
    cl.stats[STAT_WEAPONFRAME] = MSG_ReadByte ();

if (bits & SU_WEAPON)
    i = MSG_ReadByte ();
if (cl.stats[STAT_WEAPON] != i)
{
    cl.stats[STAT_WEAPON] = i;
    Sbar_Changed ();
}

// Ammo tracking
cl.stats[STAT_AMMO] = MSG_ReadByte ();
cl.stats[STAT_SHELLS] = MSG_ReadByte ();
cl.stats[STAT_NAILS] = MSG_ReadByte ();
cl.stats[STAT_ROCKETS] = MSG_ReadByte ();
cl.stats[STAT_CELLS] = MSG_ReadByte ();
```

**Key functions:**
- `SV_WriteClientdataToMessage()` [sv_main.c:832] - Server sends weapon data
- `CL_ParseClientdata()` [cl_parse.c:~650] - Client receives weapon updates

---

## 5. Damage Statistics

### 5.1 Damage Tracking Architecture

Damage is tracked per-frame in edict fields:

```
Frame sequence:
  Player takes damage (QuakeC code):
    edict->v.dmg_take += damage_amount
    edict->v.dmg_save += armor_save
    edict->v.dmg_inflictor = damaging_entity

  SV_WriteClientdataToMessage():
    Send svc_damage message to affected player
    Reset dmg_take and dmg_save to 0

  Client receives svc_damage:
    Parse damage amount
    Display visual effect (screen flash)
    Play damage sound
```

### 5.2 Server-Side Damage Transmission

**Location:** `Quake/sv_main.c:843-854` (SV_WriteClientdataToMessage)

```c
// send a damage message
if (ent->v.dmg_take || ent->v.dmg_save)
{
    other = PROG_TO_EDICT(ent->v.dmg_inflictor);
    MSG_WriteByte (msg, svc_damage);
    MSG_WriteByte (msg, ent->v.dmg_save);      // Armor damage
    MSG_WriteByte (msg, ent->v.dmg_take);      // Health damage
    for (i=0; i<3; i++)
        MSG_WriteCoord (msg, other->v.origin[i] + 0.5*(other->v.mins[i] + other->v.maxs[i]));

    ent->v.dmg_take = 0;   // Reset for next frame
    ent->v.dmg_save = 0;
}
```

**Damage message format:**
- `svc_damage` (code 19)
- byte: armor_save (damage absorbed by armor)
- byte: dmg_take (damage to health)
- vec3: source position (origin of attacker)

### 5.3 Client-Side Damage Reception

**Location:** `Quake/view.c:267-326` (V_ParseDamage)

```c
void V_ParseDamage (void)
{
    int armor = MSG_ReadByte ();
    int blood = MSG_ReadByte ();
    vec3_t from;
    for (i=0; i<3; i++)
        from[i] = MSG_ReadCoord (cl.protocolflags);

    count = blood*0.5 + armor*0.5;
    if (count < 10) count = 10;

    cl.faceanimtime = cl.time + 0.2;  // Damage face animation

    // Red screen flash effect
    cl.cshifts[CSHIFT_DAMAGE].percent += 3*count;

    // Determine color based on damage type
    if (armor > blood) { /* blue tint */ }
    else if (armor) { /* yellow tint */ }
    else { /* red tint */ }

    // Calculate camera kick direction
    // ...
}
```

**Key functions:**
- `SV_WriteClientdataToMessage()` [sv_main.c:832] - Server sends damage
- `V_ParseDamage()` [view.c:267] - Client displays damage effect
- `Sbar_Changed()` [sbar.c:103] - Marks HUD for update

### 5.4 Damage Attributes

- **Damage sent per-frame** - Only accumulated damage within the frame
- **Armor tracking** - Separate armor save value sent (not deducted from health damage)
- **Source attribution** - Entity reference of damage source for direction calculation
- **No cumulative history** - Damage resets each frame (no total damage stat)

---

## 6. Death Tracking

### 6.1 Death State Architecture

Death tracking uses the `deadflag` field:

```c
// From progdefs.q1:103
float deadflag;  // 0=alive, 1=dying, 2=dead
```

When a player dies:
1. QuakeC code sets `edict->v.deadflag = DEAD_DEAD` (2)
2. Server broadcasts entity update with deadflag state
3. `edict->v.health` becomes negative
4. Player remains in game with death state until respawn

### 6.2 Death Event Communication

**Location:** `Quake/protocol.h:179-180` (Entity update flags)

Death state is communicated via standard entity updates in `svc_update` messages. The deadflag is part of the baseline entity state sent to clients.

**Network transmission:**
- Entity updates include all edict fields
- `deadflag` and `health` sent as part of entity state
- No explicit "death" message (implicit in health < 0)

### 6.3 Client-Side Death Display

The client displays death via:
1. Player model animation (based on health/deadflag)
2. HUD stat update (health becomes negative)
3. Face animation change (gibbed/dead/alive states)

**Location:** `Quake/sbar.c:42-47` (Face graphics)

```c
static qpic_t *sb_faces[7][2];  // 0=gibbed, 1=dead, 2-6=alive
                                 // 0=static, 1=temporary animation
```

### 6.4 Death Attribution

Deaths are attributed via:
- Last damaging entity: `edict->v.dmg_inflictor`
- Damage type inferred from damaging entity type (weapon, explosion, etc.)
- Frags are decremented on suicide or no killer
- QuakeC determines "who killed whom"

---

## 7. Statistics Update Mechanisms

### 7.1 Update Flow Diagram

```
Server Frame (Host_Frame in host.c):
  ├─ SV_RunClients()        - Process client commands
  ├─ SV_Physics()           - Update entity physics (damage applied)
  │   └─ PR_ExecuteProgram()- Run QuakeC frame functions
  │       └─ edict->v.frags updated (by kill logic)
  │       └─ edict->v.dmg_take/dmg_save accumulated
  │       └─ edict->v.health modified
  ├─ SV_SendClientMessages()- Broadcast updates
  │   └─ SV_UpdateToReliableMessages()
  │       └─ Check edict->v.frags vs client->old_frags (delta)
  │       └─ Send svc_updatefrags if changed
  │   └─ SV_SendClientDatagram()
  │       ├─ SV_WriteClientdataToMessage()
  │       │   ├─ Send svc_damage (if dmg_take/dmg_save)
  │       │   ├─ Send svc_clientdata (stats, items, health)
  │       │   ├─ Weapon, ammo data
  │       │   └─ View angles, velocity
  │       └─ Entity updates (all players/monsters)
  └─ Client receives messages (async):
      ├─ CL_ParseServerMessage()
      │   ├─ svc_updatefrags → cl.scores[i].frags
      │   ├─ svc_damage → V_ParseDamage() → screen flash
      │   ├─ svc_updatestat → cl.stats[i]
      │   └─ svc_clientdata → health, armor, ammo, items
      └─ HUD updates based on stat changes
```

### 7.2 Reliable vs Unreliable Updates

**Reliable messages (guaranteed delivery):**
- Frag updates via `svc_updatefrags` [sv_main.c:1068]
- Name/color changes via `svc_updatename` and `svc_updatecolors`
- Stored in `sv.reliable_datagram` [sv_main.c:73-74]
- Sent via TCP-equivalent protocol

**Unreliable messages (best-effort):**
- Damage (`svc_damage`) [sv_main.c:846]
- Client data (`svc_clientdata`) [sv_main.c:931]
- Entity updates (`svc_update`)
- Sent via UDP-equivalent protocol

### 7.3 Server-Side Update Functions

| Function | File:Line | Purpose |
|----------|-----------|---------|
| `SV_UpdateToReliableMessages()` | sv_main.c:1054 | Delta-detect frags, broadcast updates |
| `SV_WriteClientdataToMessage()` | sv_main.c:832 | Encode player stats into message |
| `SV_SendClientDatagram()` | sv_main.c:1013 | Send unreliable updates to client |
| `SV_SendClientMessages()` | sv_main.c:1117 | Main loop, calls above functions |

### 7.4 Client-Side Update Functions

| Function | File:Line | Purpose |
|----------|-----------|---------|
| `CL_ParseServerMessage()` | cl_parse.c:~700 | Main message dispatcher |
| `CL_ParseClientdata()` | cl_parse.c:~650 | Parse svc_clientdata (stats, health, ammo) |
| `V_ParseDamage()` | view.c:267 | Handle svc_damage, screen effects |
| `Sbar_Changed()` | sbar.c:103 | Mark HUD for redraw |

---

## 8. Statistics Display/HUD

### 8.1 Scoreboard Display

The scoreboard shows all players' names and frag counts:

**Location:** `Quake/sbar.c:1122-1199` (Sbar_DeathmatchOverlay)

```
RANKING
[Color bar] [frags] [name]
  [Color bar] [frags] [name]
  ...
```

**Data source:**
- Player names: `cl.scores[i].name`
- Frags: `cl.scores[i].frags`
- Colors: `cl.scores[i].colors`

**Sorting function:** `Sbar_SortFrags()` [sbar.c:~1100]
- Sorts players by descending frag count
- Fills `fragsort[MAX_CLIENTS]` array

### 8.2 HUD Status Bar

Displays personal player stats:

**Location:** `Quake/sbar.c` (Sbar_Draw and related functions)

Typical layout:
```
[Health] [Armor] [Weapon] [Ammo/Shells/Nails/Rockets/Cells] [Items]
[Face icon (pain/death state)]
```

Stats displayed:
- Health (STAT_HEALTH)
- Armor (STAT_ARMOR)
- Current weapon (STAT_WEAPON)
- Current ammo (STAT_AMMO)
- Ammo reserves (STAT_SHELLS, STAT_NAILS, STAT_ROCKETS, STAT_CELLS)
- Items (via icons and blink effects based on item_gettime)

### 8.3 Console Status Command

**Location:** `Quake/host_cmd.c:461-509` (Host_Status_f)

Displays server-side stats:
```
host:    [hostname]
version: [version]
map:     [mapname]
players: [count] active ([max] max)

#1  [name]  [frags]  [uptime]
    [player_address]
...
```

**Source:** Server directly accesses `edict->v.frags` (not synced stats)

**Code:**
```c
print_fn ("#%-2u %-16.16s  %3i  %2i:%02i:%02i\n",
    j+1, client->name, (int)client->edict->v.frags, hours, minutes, seconds);
```

### 8.4 Console "Frags" Variable

Direct access to player stats via cvar/builtin:
- QuakeC can read/write `edict->v.frags` directly
- No dedicated command; game logic handles stat modification

---

## 9. Statistics Persistence

### 9.1 Level Transition Persistence

**Spawn parameters** (quakedef.h:86, server.h:123):

```c
float spawn_parms[NUM_SPAWN_PARMS];  // 16 floats per client
```

Preserved across level changes via:
1. `SV_SaveSpawnparms()` [server.h:244]
2. `SetChangeParms()` - QuakeC function stores current stats to spawn_parms
3. `SetNewParms()` - QuakeC function retrieves spawn_parms on new level

**Location:** `Quake/sv_main.c` (called from host.c level changes)

### 9.2 Frags Persistence Mechanism

```
Level 1 → Level 2 Transition:
  1. SetChangeParms() executes (QuakeC)
  2. spawn_parms[0..15] stored in client_t
  3. New map loaded
  4. SetNewParms() executes (QuakeC)
  5. spawn_parms[] copied back to new edict
  6. frags persist (via parm1 or custom field)
```

Exact parm mapping is game-specific (defined by progs.dat). Standard Quake typically uses:
- `parm1` - Health
- `parm2-7` - Ammo counts
- Other fields for various stats

### 9.3 Demo Recording Persistence

**Location:** `Quake/cl_demo.c:339-374` (CL_WriteFrame)

Demos record all stats for playback:

```c
// Record frags
MSG_WriteByte (&net_message, svc_updatefrags);
MSG_WriteByte (&net_message, i);
MSG_WriteShort (&net_message, cl.scores[i].frags);

// Record stats
MSG_WriteByte (&net_message, svc_updatestat);
MSG_WriteByte (&net_message, STAT_TOTALSECRETS);
MSG_WriteLong (&net_message, cl.stats[STAT_TOTALSECRETS]);
// ... etc for all stats
```

On demo playback, stats are restored from recorded messages.

### 9.4 Savegame Persistence

Savegames preserve entity data via:
- `ED_Write()` [progs.h:89] - Serialize edict fields including frags
- `ED_ParseEdict()` [progs.h:90] - Deserialize on load

This preserves all edict stats including frags, health, ammo, etc.

### 9.5 No Persistent Server-Side Stats

- QuakeSpasm does NOT maintain permanent stat databases
- Stats reset on server restart
- Per-map/session only
- Game mods can implement persistence via custom code

---

## 10. QuakeC Integration

### 10.1 QuakeC Field Access to Stats

QuakeC code accesses player stats via edict fields:

```qc
// Typical QuakeC code
entity player = find(world, classname, "player");

// Read stats
if (player.frags > 0) { /* do something */ }
local float health = player.health;
local float ammo = player.currentammo;

// Modify stats
player.frags = player.frags + 1;  // Increment kills
player.health = 100;
player.ammo_shells = player.ammo_shells - 2;
```

### 10.2 Key QuakeC Global Functions

From `Quake/progdefs.q1:globalvars_t`:

| Function | Line | Purpose |
|----------|------|---------|
| `PlayerPreThink()` | 52 | Called before physics each frame |
| `PlayerPostThink()` | 53 | Called after physics each frame |
| `ClientConnect()` | 55 | Player joins server |
| `PutClientInServer()` | 56 | Respawn/spawn player |
| `ClientDisconnect()` | 57 | Player leaves |
| `SetNewParms()` | 58 | Restore stats on level change |
| `SetChangeParms()` | 59 | Save stats for level change |

**Typical flow for frags:**
1. `PlayerPostThink()` checks for kill conditions
2. Killer's edict->v.frags incremented
3. Next frame: `SV_UpdateToReliableMessages()` detects delta
4. `svc_updatefrags` sent to all clients

### 10.3 Builtin Functions for Stats

No dedicated builtin functions for stats. All access is via direct edict field modification:

```c
// From pr_edict.c
E_FLOAT(e,o) - Get float field from edict
E_INT(e,o) - Get int field from edict
E_VECTOR(e,o) - Get vector field from edict
```

Used internally by QuakeC VM to access edict->v.frags, etc.

### 10.4 Custom Stat Fields

Game mods can add custom fields via:
1. Extending entvars_t structure (recompile needed)
2. Using "items2" field for extended inventory
3. Using unused spawn_parms for session data

**Example (items2):**
```c
// In C code
val = GetEdictFieldValue(ent, "items2");
if (val)
    items = (int)ent->v.items | ((int)val->_float << 23);
```

---

## 11. Complete Message Flow Example

### 11.1 Frag Kill Scenario

```
Timeline of events when Player A shoots Player B:

T=100ms (Server frame N):
  QuakeC code fires_weapon() → damage dealt to Player B
  Player B health: 50 → 0 (dies)
  Player B deadflag: 0 → 2
  Player A edict->v.frags: 10 → 11

T=101ms (Server frame N+1):
  SV_Frame():
    SV_UpdateToReliableMessages():
      host_client[A].old_frags (10) != edict->v.frags (11)
      DELTA DETECTED
      For each client:
        MSG_WriteByte(&client->message, svc_updatefrags)  // 14
        MSG_WriteByte(&client->message, 0)                // Player A = slot 0
        MSG_WriteShort(&client->message, 11)              // New frag count
      host_client[A].old_frags = 11  // Update for next frame

    SV_SendClientDatagram():
      SV_WriteClientdataToMessage(Player B):
        edict->v.dmg_take = 50, dmg_save = 0, dmg_inflictor = Player A
        MSG_WriteByte(msg, svc_damage)
        MSG_WriteByte(msg, 0)   // armor_save
        MSG_WriteByte(msg, 50)  // dmg_take
        MSG_WriteCoord(msg, Player A origin)
        edict->v.dmg_take = 0   // Reset
        edict->v.dmg_save = 0

      Entity updates sent (includes all entities including dead Player B)

T=102ms (Client receives):
  CL_ParseServerMessage():
    case svc_updatefrags:
      i = MSG_ReadByte()          // 0 (Player A)
      cl.scores[0].frags = MSG_ReadShort()  // 11
      Sbar_Changed()  // Mark scoreboard dirty

    case svc_damage:
      armor = MSG_ReadByte()      // 0
      blood = MSG_ReadByte()      // 50
      from = MSG_ReadCoord(3)     // Position of Player A
      V_ParseDamage():            // Visual feedback
        Show red screen flash
        Play "ouch" sound
        Update face animation
        Screen kick direction toward Player A

  Screen shows:
    Scoreboard: Player A now has 11 frags
    Player B's death ragdoll animation begins
    Damage flash/sound feedback
```

### 11.2 Item Pickup Scenario

```
T=105ms (Server frame M):
  Player picks up shotgun
  QuakeC: player.items |= IT_SHOTGUN

  SV_WriteClientdataToMessage(player):
    items = (int)player.items | (serverflags << 28)  // = 0x00000001
    MSG_WriteLong(msg, items)

T=106ms (Client receives):
  CL_ParseClientdata():
    bits = MSG_ReadShort()  // SU_ITEMS always set
    i = MSG_ReadLong()      // 0x00000001 (IT_SHOTGUN)
    if (cl.items != i):
      for (j=0; j<32; j++)
        if ((i & (1<<j)) && !(cl.items & (1<<j)))
          cl.item_gettime[0] = cl.time  // Item 0 (shotgun) acquired
      cl.items = i

  Screen shows:
    Shotgun icon appears in HUD
    Icon blinks for ~30 frames as visual feedback
    Ammo count updates if ammo also included
```

---

## 12. Protocol Definitions

### 12.1 Network Messages for Stats

**Location:** `Quake/protocol.h:160-179`

| Message | Code | Format | Purpose |
|---------|------|--------|---------|
| `svc_updatestat` | 3 | [byte:stat_id][long:value] | Update single stat |
| `svc_updatefrags` | 14 | [byte:player_id][short:frags] | Update player frags |
| `svc_updatename` | 13 | [byte:player_id][string:name] | Update player name |
| `svc_updatecolors` | 17 | [byte:player_id][byte:colors] | Update player colors |
| `svc_damage` | 19 | [byte:armor][byte:health][vec3:origin] | Damage notification |
| `svc_clientdata` | 15 | [short:bits][...data...] | Player stats/state |

**Note:** Extended protocol (FitzQuake 666) adds:
- `svc_spawnbaseline2` (42) - Large modelindex, framenum, alpha
- High-byte ammo values (SU_SHELLS2, SU_NAILS2, etc.)
- Weaponframe extensions (SU_WEAPONFRAME2)

---

## 13. Summary of Key Code Locations

| Aspect | File | Line(s) | Function |
|--------|------|---------|----------|
| **Server Frags Update** | sv_main.c | 1054-1085 | SV_UpdateToReliableMessages |
| **Server Sends Stats** | sv_main.c | 832-1006 | SV_WriteClientdataToMessage |
| **Server Sends Damage** | sv_main.c | 843-854 | SV_WriteClientdataToMessage (damage section) |
| **Client Receives Frags** | cl_parse.c | 1159-1165 | CL_ParseServerMessage (svc_updatefrags) |
| **Client Receives Stats** | cl_parse.c | 1232-1237 | CL_ParseServerMessage (svc_updatestat) |
| **Client Receives Damage** | cl_parse.c | 1101-1102 | CL_ParseServerMessage (svc_damage) |
| **Client Damage Display** | view.c | 267-326 | V_ParseDamage |
| **Client Items Pickup** | cl_parse.c | 745-755 | CL_ParseClientdata (items section) |
| **Scoreboard Display** | sbar.c | 1122-1199 | Sbar_DeathmatchOverlay |
| **Status Command** | host_cmd.c | 461-509 | Host_Status_f |
| **Spawn Params Save** | sv_main.c | varies | SV_SaveSpawnparms |
| **Entity Structure** | progdefs.q1 | 62-141 | entvars_t definition |
| **Client Structure** | server.h | 96-127 | client_t definition |
| **Stats Indices** | quakedef.h | 107-122 | STAT_* defines |
| **Item Flags** | quakedef.h | 124-149 | IT_* defines |
| **Demo Recording Stats** | cl_demo.c | 339-374 | CL_WriteFrame |
| **Protocol Defs** | protocol.h | 160-179 | svc_* message codes |

---

## 14. Design Notes

### 14.1 Architecture Principles

1. **Client-Side Storage**: Client maintains authoritative scoreboard and personal stats cache
2. **Server Broadcasting**: Server broadcasts frags via delta-detection to ensure consistency
3. **Frame-Based Updates**: Damage, items reset each frame; cumulative stats (frags) persist
4. **QuakeC Ownership**: Game logic determines stat changes; engine transmits them
5. **Reliable Frags**: Frag updates guaranteed (TCP); other stats best-effort (UDP)

### 14.2 Limitations and Extensions

**Base Engine Limitations:**
- No shot-fired or accuracy statistics
- No damage-dealt tracking (only damage-taken)
- Item pickups implicit in inventory bitflags
- No persistent global leaderboards

**Extension Methods:**
- Custom QuakeC fields for detailed stats
- Extended inventory via items2 field
- Mods can intercept svc_updatestat for custom handling
- Savegames preserve all edict data

### 14.3 Network Efficiency

- Frags sent only on change (delta-compression)
- Stats sent every frame (client data always includes svc_clientdata)
- Damage sent only if accumulated (optimizes unreliable messages)
- Items sent every frame as part of clientdata
- Scoreboard synced via reliable channel (guaranteed delivery)

---

## Appendix A: Stat Index Reference

```c
#define	STAT_HEALTH		0		// Current health
#define	STAT_FRAGS		1		// Total kills
#define	STAT_WEAPON		2		// Current weapon index
#define	STAT_AMMO		3		// Current weapon ammo
#define	STAT_ARMOR		4		// Armor value
#define	STAT_WEAPONFRAME	5	// Weapon animation frame
#define	STAT_SHELLS		6		// Shotgun ammo count
#define	STAT_NAILS		7		// Nailgun ammo count
#define	STAT_ROCKETS	8		// Rocket ammo count
#define	STAT_CELLS		9		// Lightning ammo count
#define	STAT_ACTIVEWEAPON	10	// Owned weapons bitfield
#define	STAT_TOTALSECRETS	11	// Total secrets on map
#define	STAT_TOTALMONSTERS	12	// Total monsters on map
#define	STAT_SECRETS	13		// Secrets found by player
#define	STAT_MONSTERS	14		// Monsters killed by player
```

---

## Appendix B: Network Message Sequence

```
Frag Update Sequence:
  Server: SV_UpdateToReliableMessages() → Check delta
  Server: MSG_WriteByte(svc_updatefrags)
  Server: MSG_WriteByte(player_id)
  Server: MSG_WriteShort(frags)
  Client: CL_ParseServerMessage() → svc_updatefrags handler
  Client: cl.scores[player_id].frags = value
  Client: Sbar_Changed() → Redraw scoreboard

Damage Update Sequence:
  Server: SV_WriteClientdataToMessage() → dmg_take > 0
  Server: MSG_WriteByte(svc_damage)
  Server: MSG_WriteByte(dmg_save)
  Server: MSG_WriteByte(dmg_take)
  Server: MSG_WriteCoord(from_x)
  Server: MSG_WriteCoord(from_y)
  Server: MSG_WriteCoord(from_z)
  Client: CL_ParseServerMessage() → svc_damage handler
  Client: V_ParseDamage() → Visual feedback
```

---

## Appendix C: File Dependencies

```
Statistics System Dependencies:
├─ Server-Side:
│  ├─ host.c (main loop calls SV_SendClientMessages)
│  ├─ sv_main.c (frags/damage update logic)
│  ├─ server.h (client_t structure)
│  ├─ progdefs.q1 (entvars_t fields)
│  ├─ pr_exec.c (QuakeC VM executes game logic)
│  └─ protocol.h (svc_* message codes)
├─ Client-Side:
│  ├─ cl_parse.c (svc_* message handlers)
│  ├─ client.h (client_state_t, scoreboard_t)
│  ├─ view.c (damage display)
│  ├─ sbar.c (HUD and scoreboard rendering)
│  └─ protocol.h (svc_* message codes)
├─ Shared:
│  ├─ quakedef.h (STAT_* and IT_* defines)
│  └─ progs.h (edict_t structure)
└─ Supporting:
   ├─ host_cmd.c (status command)
   ├─ cl_demo.c (demo recording)
   └─ zone.c (memory management)
```

---

**Document Version:** 1.0
**Last Updated:** 2026-02-03
**QuakeSpasm Version:** 0.97.0
**Scope:** Complete player statistics tracking system
