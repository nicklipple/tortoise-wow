# mod-eluna Compatibility Contract

This is the compatibility contract for Turtle WoW client patch 1.18.1
(build 7272) on the Tortoise core. It is an inventory and a set of rules for
the deliberately small runtime bridge; it does not imply upstream Eluna
parity.

The upstream reference is the Eluna submodule at commit
`58d7652138887783228b9727bd1fa4e08f00342b`. The upstream files are reference
material only; no file under `Eluna/` is compiled by `mod-eluna`.

## Status Vocabulary

Every row below has one of these classifications:

| Classification | Meaning |
| --- | --- |
| `supported now` | Public in the current POC and covered by its regression contract. |
| `direct candidate` | A Tortoise dispatch point is close enough to investigate without a core change, but the event is not public yet. |
| `adapter required` | A Tortoise hook exists, but registration scope, arguments, timing, return values, or mutation behavior differ. |
| `core gap` | No active Tortoise dispatch point expresses the upstream event safely. A future core hook or a different API is required. |
| `runtime-only` | Internal module state, not an Eluna script event. |

Rows classified as anything other than `supported now` are not exposed by
this module. A classification is not an API promise or a claim of parity.

## Supported Now

The first public API set is intentionally limited to the existing POC:

| Public name | Contract | Tortoise owner and dispatch |
| --- | --- | --- |
| `RegisterPlayerEvent(3, callback)` | Registers the login notification using upstream event ID 3. The current POC stores one callback; a later registration replaces it. The callback receives `(3, player)`. Its return values are ignored. | `PlayerScript::OnLogin(Player*)`, dispatched by `src/game/Handlers/CharacterHandler.cpp:1114-1125`. |
| `Player:SendBroadcastMessage(message)` | Sends a system message when the validity-checked player handle resolves to a player with a live session. It returns no value. Empty messages are ignored by the POC. An invalid or stale handle raises a controlled Lua error. | `ChatHandler(player->GetSession()).SendSysMessage(message)` in `modules/mod-eluna/src/ElunaBindings.cpp:87-101`, after `ElunaRuntime::ResolvePlayer`. |

The login callback is synchronous on the world thread. The `player` userdata
contains only the player's GUID, a runtime generation, and the `Player` wrapper
type. Each native method resolves the GUID again through `ObjectAccessor` and
checks the generation, deletion state, world membership, and GUID identity.
`PlayerScript::OnBeforeLogout` invalidates the generation before core logout
cleanup, and runtime teardown invalidates every generation. Scripts may retain
the userdata, but using it after logout, GUID reuse, state teardown, or another
invalidating transition raises a controlled Lua error. No other object method
or global function is supported.

## Runtime Kernel Rules

- The `WorldScript::OnStartup` hook only prepares the runtime object because
  Tortoise invokes that hook on `MainThread`. Lua state creation is deferred to
  the first `PlayerScript` login or `WorldScript` update on `WorldThread`.
- One runtime owns one Lua state. Lua API calls, callback registration,
  callback dispatch, handle resolution, and state teardown must run on that
  owner thread. A non-owner attempt is logged and rejected; no Lua state is
  shared with map, socket, or worker threads.
- `Object`, `WorldObject`, `Unit`, and `Player` are represented by a Lua
  metatable hierarchy. The current public value is a `Player` wrapper; base
  wrappers are established for later phases but have no public constructors or
  methods yet.
- Wrapper userdata is Lua-owned storage with a no-op-to-Tortoise destructor.
  Its payload has no unmanaged Tortoise pointer. Lua garbage collection frees
  only the wrapper storage.
- Integer and enum inputs use checked Lua integer conversion and native range
  validation. Strings use checked Lua strings and are copied before entering
  native code. Boolean inputs must be Lua booleans. GUIDs are copied into
  opaque handle metadata rather than accepted as lossy Lua 5.1 numbers.
- Nil is accepted only by a binding that explicitly documents an optional
  argument. Required objects must be the expected userdata type and must pass
  the runtime validity check before native dereference.

## Lifetime, Thread, and Error Rules

- Tortoise owns all `WorldScript`, `PlayerScript`, `UnitScript`, map, object,
  packet, group, guild, and database-bound script instances. Lua never deletes
  them.
- Integer values, enums, GUIDs, and strings are copied into the Lua call.
- Core object pointers are borrowed for the synchronous callback only. They
  must become validity-checked handles before a script can retain them across
  logout, despawn, map removal, reload, shutdown, or another callback.
- `WorldPacket` values are borrowed for packet callbacks. Tortoise exposes
  `const WorldPacket&` to its packet filters, so upstream packet replacement is
  not available without a separate adapter and core invariant review.
- Callbacks run synchronously on the originating core thread. World and player
  hooks run in the world update/session path. Socket open/close hooks run from
  `WorldSocket`; no Lua callback may be assumed safe on a worker or network
  thread until that path is explicitly marshalled.
- Notification callbacks ignore Lua return values. A callback error is logged
  and must not unwind into the core or prevent unrelated notification work.
- For future allow/reject filters, `true` means allow and `false` means reject;
  an omitted result preserves the core default. Tortoise filter registries
  short-circuit when a rejecting callback is found.
- For future interaction handlers, `true` means handled/prevent the default
  action and `false` means continue the core action. This is distinct from an
  allow/reject filter and must be documented on each binding.
- Mutable native references are copied into Lua and can only be changed when a
  row explicitly lists a replacement return value. On callback error, the last
  valid native value is preserved.
- The current POC logs script compilation, execution, and callback failures,
  continues loading other scripts where possible, and closes its Lua state on
  world shutdown. These behaviors are regression requirements.

The owner shorthand in the event catalog is:

| Owner | Tortoise type and lifetime |
| --- | --- |
| `W` | `WorldScript`; world-owned. World objects and mutable MOTD/config values are borrowed for the callback. |
| `P` | `PlayerScript`; `Player*` is borrowed for the callback. `ObjectGuid` and scalar arguments are copied. |
| `U` | `UnitScript`; `Unit*`, `Aura*`, and spell metadata are borrowed for the callback. |
| `S` | `ServerScript`; packet/socket objects are borrowed. Packet filtering and socket paths require thread review. |
| `M` | `AllMapScript`, `MapScript`, or `InstanceData`; map/player pointers are borrowed and map-specific instances are core-owned. |
| `C` | `CreatureScript`, `AllCreatureScript`, `ZoneScript`, or creature AI; the selected registry/zone owns the object. |
| `G` | `GameObjectScript`, `AllGameObjectScript`, or `ZoneScript`; gameobjects are borrowed and core-owned. |
| `I` | `ItemScript`, `AllItemScript`, or player inventory; item pointers are borrowed and may be removed during the callback. |
| `Q` | `SpellScript`, `AuraScript`, or `UnitScript`; spell/aura objects are temporary and callback-only. |
| `R` | `GroupScript`, `GuildScript`, `AuctionHouseScript`, `WeatherScript`, `LootScript`, or `GameEventScript`; registry-specific ownership applies and object arguments remain borrowed. |
| `-` | No active Tortoise owner or dispatch point was found. No binding is planned from the row alone. |

## Registration Category Matrix

These are the registration categories from `Hooks::RegisterTypes` and the
category aliases used by `HookTypeTable` in `Eluna/hooks/Hooks.h`. The event
ID catalog below lists the definitions once; aliases intentionally reuse the
same event table.

| Registration category | Upstream event IDs | Tortoise dispatch point | Owner and lifetime | Classification |
| --- | --- | --- | --- | --- |
| `packet` | 5-7 | `ServerScript::CanPacketReceive/CanPacketSend`; `WorldSession::ProcessPackets` and `WorldSession::SendPacket` | `S`; borrowed `WorldPacket`, synchronous packet path | `adapter required` |
| `server` | 1-9, 11-35; 10 unused | `ServerScript`, `WorldScript`, `AllMapScript`, `AreaTriggerScript`, `WeatherScript`, `AuctionHouseScript`, `GameEventScript`, plus module state | `S/W/M/R`; owner-specific borrowed arguments | mixed; see catalog |
| `player` | 1-49, 54; 50-53 unused | `PlayerScript`; selected `UnitScript`, `LootScript`, and `MailScript` paths | `P/U/R`; player and related objects callback-only | mixed; see catalog |
| `guild` | 1-11 | `GuildScript` currently covers add/remove/create/disband only | `R`; guild/player borrowed; rank may be mutable in Tortoise | mixed; see catalog |
| `group` | 1-7 | `GroupScript` currently covers add/remove/leader/disband | `R`; group borrowed and GUIDs copied | mixed; see catalog |
| `creature` | 1-10, 12-15, 19-24, 26-27, 30-31, 34-37 | `CreatureScript`, `AllCreatureScript`, `ZoneScript`, and `ScriptMgr` interaction paths | `C`; entry-bound/zone-bound objects borrowed | mixed; see catalog |
| `creature_unique` | same as `creature` | Upstream unique key is `(event, guid, instanceId)`; Tortoise has no equivalent global unique-event registry | `C`; no retained raw pointer | `adapter required` or `core gap` |
| `vehicle` | 1, 2, 4-6; 3 unused | No active Tortoise vehicle script type or dispatch was found | `-` | `core gap` and expansion-specific |
| `gossip` (HookTypeTable alias) | 1-2 | Shared `GossipEventsTable` for creature, gameobject, item, and player gossip registrations | Owner depends on the specialized gossip category; interaction objects are borrowed | `adapter required` |
| `creature_gossip` | 1-2 | `ScriptMgr::OnGossipHello/OnGossipSelect` and database-bound `CreatureScript` | `C`; creature/player borrowed during interaction | `adapter required` |
| `gameobject` | 1-10, 12-14; 11 unused | `GameObjectScript` interaction methods and `ScriptMgr`; global lifecycle methods are declared but not dispatched | `G`; gameobject/player borrowed | mixed; see catalog |
| `gameobject_gossip` | 1-2 | `ScriptMgr::OnGossipHello/OnGossipSelect` and database-bound `GameObjectScript` | `G`; gameobject/player borrowed during interaction | `adapter required` |
| `spell` | 1-23 | `SpellScript`, `AuraScript`, `PlayerScript::OnSpellCast`, and `UnitScript` aura hooks | `Q/U/P`; spell/aura arguments callback-only | `adapter required` |
| `item` | 1-8 | `ScriptMgr::OnItemUse` and database-bound `ItemScript` cover only part of the surface | `I`; item may be removed while handling use | mixed; see catalog |
| `item_gossip` | 1-2 | No dedicated item gossip registry; item use is the nearest path | `I`; item/player borrowed | `adapter required` or `core gap` |
| `player_gossip` | 1-2 | No player gossip dispatch point was found | `-` | `core gap` |
| `bg` | 1-4 | `AllBattlegroundScript` is declared, but no active lifecycle dispatch was found in the battleground manager | `-` | `core gap` |
| `map` | 1-7 | `AllMapScript::OnCreateMap/OnDestroyMap/OnPlayerEnterAll/OnPlayerLeaveAll/OnMapUpdate`; no grid callbacks | `M`; map/player borrowed, map callbacks are global rather than entry-bound | mixed; see catalog |
| `instance` | 1-7 | `InstanceData::Initialize/Load/Update` and map/zone hooks; no `ElunaInstanceAI` equivalent | `M`; instance data is core-owned and map-bound | mixed; see catalog |

Important dispatch facts:

- `HookTypeTable` maps both `map` and `instance` to `InstanceEventsTable`.
- `MapScript<TMap>::OnLoadGridMap` and `OnUnloadGridMap` are declared in
  `src/game/ScriptObjects.h`, but no active callers were found.
- `AllCreatureScript::OnCreatureAddWorld` is called from
  `src/game/Objects/Creature.cpp:276-281`; its remove counterpart is declared
  but has no caller. The analogous global gameobject add/remove methods are
  declared but have no callers.
- `CreatureScript`, `GameObjectScript`, and `ItemScript` registrations are
  database-bound. They are not equivalent to arbitrary Lua entry or unique-GUID
  registrations.

## Event ID Catalog

The following rows mirror every event definition in `Eluna/hooks/Hooks.h`.
The classifications describe the compatibility work still required; only the
two rows in `Supported Now` are public.

### Packet Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `PACKET_EVENT_ON_PACKET_RECEIVE` | 5 | `on_receive` | `adapter required` | `S` |
| `PACKET_EVENT_ON_PACKET_RECEIVE_UNKNOWN` | 6 | `on_receive_unk` | `adapter required` | `S` |
| `PACKET_EVENT_ON_PACKET_SEND` | 7 | `on_send` | `adapter required` | `S` |

### Server Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `SERVER_EVENT_ON_NETWORK_START` | 1 | `on_network_start` | `direct candidate` | `S` |
| `SERVER_EVENT_ON_NETWORK_STOP` | 2 | `on_network_stop` | `direct candidate` | `S` |
| `SERVER_EVENT_ON_SOCKET_OPEN` | 3 | `on_socket_open` | `direct candidate` | `S` |
| `SERVER_EVENT_ON_SOCKET_CLOSE` | 4 | `on_socket_close` | `direct candidate` | `S` |
| `SERVER_EVENT_ON_PACKET_RECEIVE` | 5 | `on_packet_receive` | `adapter required` | `S` |
| `SERVER_EVENT_ON_PACKET_RECEIVE_UNKNOWN` | 6 | `on_packet_receive_unk` | `adapter required` | `S` |
| `SERVER_EVENT_ON_PACKET_SEND` | 7 | `on_packet_send` | `adapter required` | `S` |
| `WORLD_EVENT_ON_OPEN_STATE_CHANGE` | 8 | `on_open_state_change` | `direct candidate` | `W` |
| `WORLD_EVENT_ON_CONFIG_LOAD` | 9 | `on_config_load` | `adapter required` | `W` |
| `WORLD_EVENT_ON_SHUTDOWN_INIT` | 11 | `on_shutdown_init` | `adapter required` | `W` |
| `WORLD_EVENT_ON_SHUTDOWN_CANCEL` | 12 | `on_shutdown_cancel` | `direct candidate` | `W` |
| `WORLD_EVENT_ON_UPDATE` | 13 | `on_world_update` | `direct candidate` | `W` |
| `WORLD_EVENT_ON_STARTUP` | 14 | `on_world_startup` | `direct candidate` | `W` |
| `WORLD_EVENT_ON_SHUTDOWN` | 15 | `on_world_shutdown` | `direct candidate` | `W` |
| `ELUNA_EVENT_ON_LUA_STATE_CLOSE` | 16 | `on_lua_state_close` | `runtime-only` | `-` |
| `MAP_EVENT_ON_CREATE` | 17 | `on_map_create` | `direct candidate` | `M` |
| `MAP_EVENT_ON_DESTROY` | 18 | `on_map_destroy` | `direct candidate` | `M` |
| `MAP_EVENT_ON_GRID_LOAD` | 19 | `on_map_grid_load` | `core gap` | `-` |
| `MAP_EVENT_ON_GRID_UNLOAD` | 20 | `on_map_grid_unload` | `core gap` | `-` |
| `MAP_EVENT_ON_PLAYER_ENTER` | 21 | `on_map_player_enter` | `direct candidate` | `M` |
| `MAP_EVENT_ON_PLAYER_LEAVE` | 22 | `on_map_player_leave` | `direct candidate` | `M` |
| `MAP_EVENT_ON_UPDATE` | 23 | `on_map_update` | `direct candidate` | `M` |
| `TRIGGER_EVENT_ON_TRIGGER` | 24 | `on_event_trigger` | `direct candidate` | `R` |
| `WEATHER_EVENT_ON_CHANGE` | 25 | `on_weather_change` | `direct candidate` | `R` |
| `AUCTION_EVENT_ON_ADD` | 26 | `on_auction_add` | `direct candidate` | `R` |
| `AUCTION_EVENT_ON_REMOVE` | 27 | `on_auction_remove` | `direct candidate` | `R` |
| `AUCTION_EVENT_ON_SUCCESSFUL` | 28 | `on_auction_successful` | `direct candidate` | `R` |
| `AUCTION_EVENT_ON_EXPIRE` | 29 | `on_auction_expire` | `direct candidate` | `R` |
| `ADDON_EVENT_ON_MESSAGE` | 30 | `on_addon_message` | `core gap` | `-` |
| `WORLD_EVENT_ON_DELETE_CREATURE` | 31 | `on_world_delete_creature` | `core gap` | `-` |
| `WORLD_EVENT_ON_DELETE_GAMEOBJECT` | 32 | `on_world_delete_gameobject` | `core gap` | `-` |
| `ELUNA_EVENT_ON_LUA_STATE_OPEN` | 33 | `on_lua_state_open` | `runtime-only` | `-` |
| `GAME_EVENT_START` | 34 | `on_game_start` | `direct candidate` | `R` |
| `GAME_EVENT_STOP` | 35 | `on_game_stop` | `direct candidate` | `R` |

Event ID 10 is unused in the upstream server table.

### Player Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `PLAYER_EVENT_ON_CHARACTER_CREATE` | 1 | `on_character_create` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_CHARACTER_DELETE` | 2 | `on_character_delete` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_LOGIN` | 3 | `on_login` | `supported now` | `P` |
| `PLAYER_EVENT_ON_LOGOUT` | 4 | `on_logout` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_SPELL_CAST` | 5 | `on_spell_cast` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_KILL_PLAYER` | 6 | `on_kill_player` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_KILL_CREATURE` | 7 | `on_kill_creature` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_KILLED_BY_CREATURE` | 8 | `on_killed_by_creature` | `core gap` | `-` |
| `PLAYER_EVENT_ON_DUEL_REQUEST` | 9 | `on_duel_request` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_DUEL_START` | 10 | `on_duel_start` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_DUEL_END` | 11 | `on_duel_end` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_GIVE_XP` | 12 | `on_give_xp` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_LEVEL_CHANGE` | 13 | `on_level_change` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_MONEY_CHANGE` | 14 | `on_money_change` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_REPUTATION_CHANGE` | 15 | `on_reputation_change` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_TALENTS_CHANGE` | 16 | `on_talents_change` | `core gap` | `-` |
| `PLAYER_EVENT_ON_TALENTS_RESET` | 17 | `on_talents_reset` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_CHAT` | 18 | `on_chat` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_WHISPER` | 19 | `on_whisper` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_GROUP_CHAT` | 20 | `on_group_chat` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_GUILD_CHAT` | 21 | `on_guild_chat` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_CHANNEL_CHAT` | 22 | `on_channel_chat` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_EMOTE` | 23 | `on_emote` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_TEXT_EMOTE` | 24 | `on_text_emote` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_SAVE` | 25 | `on_save` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_BIND_TO_INSTANCE` | 26 | `on_bind_to_instance` | `core gap` | `-` |
| `PLAYER_EVENT_ON_UPDATE_ZONE` | 27 | `on_update_zone` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_MAP_CHANGE` | 28 | `on_map_change` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_EQUIP` | 29 | `on_equip` | `core gap` | `-` |
| `PLAYER_EVENT_ON_FIRST_LOGIN` | 30 | `on_first_login` | `core gap` | `-` |
| `PLAYER_EVENT_ON_CAN_USE_ITEM` | 31 | `on_can_use_item` | `adapter required` | `I` |
| `PLAYER_EVENT_ON_LOOT_ITEM` | 32 | `on_loot_item` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_ENTER_COMBAT` | 33 | `on_enter_combat` | `adapter required` | `U` |
| `PLAYER_EVENT_ON_LEAVE_COMBAT` | 34 | `on_leave_combat` | `adapter required` | `U` |
| `PLAYER_EVENT_ON_REPOP` | 35 | `on_repop` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_RESURRECT` | 36 | `on_resurrect` | `core gap` | `-` |
| `PLAYER_EVENT_ON_LOOT_MONEY` | 37 | `on_loot_money` | `direct candidate` | `R` |
| `PLAYER_EVENT_ON_QUEST_ABANDON` | 38 | `on_quest_abandon` | `core gap` | `-` |
| `PLAYER_EVENT_ON_LEARN_TALENTS` | 39 | `on_learn_talents` | `core gap` | `-` |
| `PLAYER_EVENT_ON_ENVIRONMENTAL_DEATH` | 40 | `on_environmental_death` | `core gap` | `-` |
| `PLAYER_EVENT_ON_TRADE_ACCEPT` | 41 | `on_trade_accept` | `core gap` | `-` |
| `PLAYER_EVENT_ON_COMMAND` | 42 | `on_command` | `adapter required` | `P` |
| `PLAYER_EVENT_ON_SKILL_CHANGE` | 43 | `on_skill_change` | `core gap` | `-` |
| `PLAYER_EVENT_ON_LEARN_SPELL` | 44 | `on_learn_spell` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_ACHIEVEMENT_COMPLETE` | 45 | `on_achievement_complete` | `core gap` | `-` |
| `PLAYER_EVENT_ON_DISCOVER_AREA` | 46 | `on_discover_area` | `core gap` | `-` |
| `PLAYER_EVENT_ON_UPDATE_AREA` | 47 | `on_update_area` | `direct candidate` | `P` |
| `PLAYER_EVENT_ON_TRADE_INIT` | 48 | `on_trade_init` | `core gap` | `-` |
| `PLAYER_EVENT_ON_SEND_MAIL` | 49 | `on_send_mail` | `adapter required` | `R` |
| `PLAYER_EVENT_ON_QUEST_STATUS_CHANGED` | 54 | `on_quest_status_changed` | `core gap` | `-` |

Player event IDs 50-53 are unused in the upstream player table.

The direct player dispatch sites include `CharacterHandler.cpp`,
`WorldSession.cpp`, `Player.cpp`, `Spell.cpp`, `SpellEffects.cpp`,
`DuelHandler.cpp`, `ChatHandler.cpp`, `MovementHandler.cpp`, and
`LootHandler.cpp`. `ChatHandler::OnBeforeSendChatMessage` is notification-only
and cannot provide the upstream chat cancellation result. `CanUseGroupChat`
can suppress a group line, but does not provide an upstream `Group*` argument.

### Guild Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `GUILD_EVENT_ON_ADD_MEMBER` | 1 | `on_add_member` | `adapter required` | `R` |
| `GUILD_EVENT_ON_REMOVE_MEMBER` | 2 | `on_remove_member` | `adapter required` | `R` |
| `GUILD_EVENT_ON_MOTD_CHANGE` | 3 | `on_motd_change` | `core gap` | `-` |
| `GUILD_EVENT_ON_INFO_CHANGE` | 4 | `on_info_change` | `core gap` | `-` |
| `GUILD_EVENT_ON_CREATE` | 5 | `on_create` | `direct candidate` | `R` |
| `GUILD_EVENT_ON_DISBAND` | 6 | `on_disband` | `direct candidate` | `R` |
| `GUILD_EVENT_ON_MONEY_WITHDRAW` | 7 | `on_money_withdraw` | `core gap` | `-` |
| `GUILD_EVENT_ON_MONEY_DEPOSIT` | 8 | `on_money_deposit` | `core gap` | `-` |
| `GUILD_EVENT_ON_ITEM_MOVE` | 9 | `on_item_move` | `core gap` | `-` |
| `GUILD_EVENT_ON_EVENT` | 10 | `on_event` | `core gap` | `-` |
| `GUILD_EVENT_ON_BANK_EVENT` | 11 | `on_bank_event` | `core gap` | `-` |

`GuildScript` is called from `src/game/Guild/Guild.cpp:172-175`,
`264-270`, `582-585`, and `898-900`. Its rank width, remove flags, and lack
of MOTD/info/bank hooks require explicit adapters.

### Group Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `GROUP_EVENT_ON_MEMBER_ADD` | 1 | `on_add_member` | `direct candidate` | `R` |
| `GROUP_EVENT_ON_MEMBER_INVITE` | 2 | `on_invite_member` | `core gap` | `-` |
| `GROUP_EVENT_ON_MEMBER_REMOVE` | 3 | `on_remove_member` | `direct candidate` | `R` |
| `GROUP_EVENT_ON_LEADER_CHANGE` | 4 | `on_leader_change` | `direct candidate` | `R` |
| `GROUP_EVENT_ON_DISBAND` | 5 | `on_disband` | `direct candidate` | `R` |
| `GROUP_EVENT_ON_CREATE` | 6 | `on_create` | `core gap` | `-` |
| `GROUP_EVENT_ON_MEMBER_ACCEPT` | 7 | `on_member_accept` | `core gap` | `-` |

`GroupScript` has no active invite, create, or member-accept hook. Its active
callbacks are synchronous registry notifications in `src/game/Group/Group.cpp`.

### Vehicle Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `VEHICLE_EVENT_ON_INSTALL` | 1 | `on_install` | `core gap` | `-` |
| `VEHICLE_EVENT_ON_UNINSTALL` | 2 | `on_uninstall` | `core gap` | `-` |
| `VEHICLE_EVENT_ON_INSTALL_ACCESSORY` | 4 | `on_install_accessory` | `core gap` | `-` |
| `VEHICLE_EVENT_ON_ADD_PASSENGER` | 5 | `on_add_passenger` | `core gap` | `-` |
| `VEHICLE_EVENT_ON_REMOVE_PASSENGER` | 6 | `on_remove_passenger` | `core gap` | `-` |

Vehicle event 3 is unused. The upstream implementation is guarded by
`ELUNA_EXPANSION >= EXP_WOTLK`, while this target is the classic 1.18.1 client
and no active Tortoise vehicle event registry was found.

### Creature Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `CREATURE_EVENT_ON_ENTER_COMBAT` | 1 | `on_enter_combat` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_LEAVE_COMBAT` | 2 | `on_leave_combat` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_TARGET_DIED` | 3 | `on_target_died` | `core gap` | `-` |
| `CREATURE_EVENT_ON_DIED` | 4 | `on_died` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_SPAWN` | 5 | `on_spawn` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_REACH_WP` | 6 | `on_reach_wp` | `core gap` | `-` |
| `CREATURE_EVENT_ON_AIUPDATE` | 7 | `on_ai_update` | `core gap` | `-` |
| `CREATURE_EVENT_ON_RECEIVE_EMOTE` | 8 | `on_receive_emote` | `core gap` | `-` |
| `CREATURE_EVENT_ON_DAMAGE_TAKEN` | 9 | `on_damage_taken` | `core gap` | `-` |
| `CREATURE_EVENT_ON_PRE_COMBAT` | 10 | `on_pre_combat` | `core gap` | `-` |
| `CREATURE_EVENT_ON_OWNER_ATTACKED` | 12 | `on_owner_attacked` | `core gap` | `-` |
| `CREATURE_EVENT_ON_OWNER_ATTACKED_AT` | 13 | `on_owner_attacked_at` | `core gap` | `-` |
| `CREATURE_EVENT_ON_HIT_BY_SPELL` | 14 | `on_hit_by_spell` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_SPELL_HIT_TARGET` | 15 | `on_spell_hit_target` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_JUST_SUMMONED_CREATURE` | 19 | `on_just_summoned_creature` | `core gap` | `-` |
| `CREATURE_EVENT_ON_SUMMONED_CREATURE_DESPAWN` | 20 | `on_summoned_creature_despawn` | `core gap` | `-` |
| `CREATURE_EVENT_ON_SUMMONED_CREATURE_DIED` | 21 | `on_summoned_creature_died` | `core gap` | `-` |
| `CREATURE_EVENT_ON_SUMMONED` | 22 | `on_summoned` | `core gap` | `-` |
| `CREATURE_EVENT_ON_RESET` | 23 | `on_reset` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_REACH_HOME` | 24 | `on_reach_home` | `core gap` | `-` |
| `CREATURE_EVENT_ON_CORPSE_REMOVED` | 26 | `on_corpse_removed` | `core gap` | `-` |
| `CREATURE_EVENT_ON_MOVE_IN_LOS` | 27 | `on_move_in_los` | `core gap` | `-` |
| `CREATURE_EVENT_ON_DUMMY_EFFECT` | 30 | `on_dummy_effect` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_QUEST_ACCEPT` | 31 | `on_quest_accept` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_QUEST_REWARD` | 34 | `on_quest_reward` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_DIALOG_STATUS` | 35 | `on_dialog_status` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_ADD` | 36 | `on_add` | `adapter required` | `C` |
| `CREATURE_EVENT_ON_REMOVE` | 37 | `on_remove` | `core gap` | `-` |

`CreatureScript` and `ScriptMgr` provide database-bound gossip, quest, dialog,
and dummy-effect paths. `ZoneScript` provides map-scoped creature lifecycle
callbacks. Neither is an equivalent to upstream entry plus unique-GUID
registration. `AllCreatureScript::OnCreatureAddWorld` is global and active,
but `OnCreatureRemoveWorld` has no caller.

### GameObject Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `GAMEOBJECT_EVENT_ON_AIUPDATE` | 1 | `on_ai_update` | `core gap` | `-` |
| `GAMEOBJECT_EVENT_ON_SPAWN` | 2 | `on_spawn` | `core gap` | `-` |
| `GAMEOBJECT_EVENT_ON_DUMMY_EFFECT` | 3 | `on_dummy_effect` | `adapter required` | `G` |
| `GAMEOBJECT_EVENT_ON_QUEST_ACCEPT` | 4 | `on_quest_accept` | `adapter required` | `G` |
| `GAMEOBJECT_EVENT_ON_QUEST_REWARD` | 5 | `on_quest_reward` | `adapter required` | `G` |
| `GAMEOBJECT_EVENT_ON_DIALOG_STATUS` | 6 | `on_dialog_status` | `adapter required` | `G` |
| `GAMEOBJECT_EVENT_ON_DESTROYED` | 7 | `on_destroyed` | `core gap` | `-` |
| `GAMEOBJECT_EVENT_ON_DAMAGED` | 8 | `on_damaged` | `core gap` | `-` |
| `GAMEOBJECT_EVENT_ON_LOOT_STATE_CHANGE` | 9 | `on_loot_state_change` | `core gap` | `-` |
| `GAMEOBJECT_EVENT_ON_GO_STATE_CHANGED` | 10 | `on_go_state_changed` | `core gap` | `-` |
| `GAMEOBJECT_EVENT_ON_ADD` | 12 | `on_add` | `core gap` | `-` |
| `GAMEOBJECT_EVENT_ON_REMOVE` | 13 | `on_remove` | `core gap` | `-` |
| `GAMEOBJECT_EVENT_ON_USE` | 14 | `on_use` | `adapter required` | `G` |

Gameobject event 11 is unused. `ScriptMgr::OnGameObjectUse` currently calls
the database-bound `GameObjectScript::OnGossipHello`, so upstream `on_use`
cannot be exposed without documenting that intentional behavior difference.

### Spell Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `SPELL_EVENT_ON_CAST` | 1 | `on_cast` | `adapter required` | `Q/P` |
| `SPELL_EVENT_ON_AURA_APPLICATION` | 2 | `on_aura_application` | `adapter required` | `Q/U` |
| `SPELL_EVENT_ON_DISPEL` | 3 | `on_dispel` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_PERIODIC_TICK` | 4 | `on_periodic_tick` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_PERIODIC_UPDATE` | 5 | `on_periodic_update` | `core gap` | `-` |
| `SPELL_EVENT_ON_AURA_CALC_AMOUNT` | 6 | `on_aura_calc_amount` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_CALC_PERIODIC` | 7 | `on_calc_periodic` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_CHECK_PROC` | 8 | `on_check_proc` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_PROC` | 9 | `on_proc` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_CHECK_CAST` | 10 | `on_check_cast` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_BEFORE_CAST` | 11 | `on_before_cast` | `core gap` | `-` |
| `SPELL_EVENT_ON_AFTER_CAST` | 12 | `on_after_cast` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_OBJECT_AREA_TARGET` | 13 | `on_object_area_target` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_OBJECT_TARGET` | 14 | `on_object_target` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_DEST_TARGET` | 15 | `on_dest_target` | `core gap` | `-` |
| `SPELL_EVENT_ON_EFFECT_LAUNCH` | 16 | `on_effect_launch` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_EFFECT_LAUNCH_TARGET` | 17 | `on_effect_launch_target` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_EFFECT_CALC_ABSORB` | 18 | `on_effect_calc_absorb` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_EFFECT_HIT` | 19 | `on_effect_hit` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_BEFORE_HIT` | 20 | `on_before_hit` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_EFFECT_HIT_TARGET` | 21 | `on_effect_hit_target` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_HIT` | 22 | `on_hit` | `adapter required` | `Q` |
| `SPELL_EVENT_ON_AFTER_HIT` | 23 | `on_after_hit` | `adapter required` | `Q` |

Tortoise has database-bound `SpellScript` and `AuraScript` callbacks plus
global unit aura hooks, but no upstream-style Lua event registry. Many native
spell callbacks are mutable or return `SpellCastResult`, not a boolean.

### Item Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `ITEM_EVENT_ON_DUMMY_EFFECT` | 1 | `on_dummy_effect` | `core gap` | `-` |
| `ITEM_EVENT_ON_USE` | 2 | `on_use` | `adapter required` | `I` |
| `ITEM_EVENT_ON_QUEST_ACCEPT` | 3 | `on_quest_accept` | `adapter required` | `I` |
| `ITEM_EVENT_ON_EXPIRE` | 4 | `on_expire` | `core gap` | `-` |
| `ITEM_EVENT_ON_REMOVE` | 5 | `on_remove` | `core gap` | `-` |
| `ITEM_EVENT_ON_ADD` | 6 | `on_add` | `core gap` | `-` |
| `ITEM_EVENT_ON_EQUIP` | 7 | `on_equip` | `core gap` | `-` |
| `ITEM_EVENT_ON_UNEQUIP` | 8 | `on_unequip` | `core gap` | `-` |

### Gossip Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `GOSSIP_EVENT_ON_HELLO` | 1 | `on_hello` | `adapter required` | `C/G/I/-` |
| `GOSSIP_EVENT_ON_SELECT` | 2 | `on_select` | `adapter required` | `C/G/I/-` |

The same two event IDs are registered independently for creature, gameobject,
item, and player gossip categories. Creature and gameobject interactions use
`ScriptMgr` and database-bound scripts. Item and player gossip do not have
equivalent dedicated Tortoise dispatch points.

### Battleground Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `BG_EVENT_ON_START` | 1 | `on_start` | `core gap` | `-` |
| `BG_EVENT_ON_END` | 2 | `on_end` | `core gap` | `-` |
| `BG_EVENT_ON_CREATE` | 3 | `on_create` | `core gap` | `-` |
| `BG_EVENT_ON_PRE_DESTROY` | 4 | `on_pre_destroy` | `core gap` | `-` |

### Instance Events

| Upstream symbol | ID | Lua name | Classification | Owner |
| --- | ---: | --- | --- | --- |
| `INSTANCE_EVENT_ON_INITIALIZE` | 1 | `on_initialize` | `adapter required` | `M` |
| `INSTANCE_EVENT_ON_LOAD` | 2 | `on_load` | `adapter required` | `M` |
| `INSTANCE_EVENT_ON_UPDATE` | 3 | `on_update` | `adapter required` | `M` |
| `INSTANCE_EVENT_ON_PLAYER_ENTER` | 4 | `on_player_enter` | `adapter required` | `M` |
| `INSTANCE_EVENT_ON_CREATURE_CREATE` | 5 | `on_creature_create` | `adapter required` | `M/C` |
| `INSTANCE_EVENT_ON_GAMEOBJECT_CREATE` | 6 | `on_gameobject_create` | `adapter required` | `M/G` |
| `INSTANCE_EVENT_ON_CHECK_ENCOUNTER_IN_PROGRESS` | 7 | `on_check_encounter_in_progress` | `adapter required` | `M` |

`InstanceData::Initialize`, `Load`, `Update`, and
`IsEncounterInProgress` are map-bound native methods. They do not expose the
upstream `ElunaInstanceAI` object or its map-ID/instance-ID registration model.

## VMangos Method Inventory

The `methods/VMangos` directory is the closest upstream reference because it
targets the same vMaNGOS lineage. It contains approximately 969 registered
bindings across these 20 header-only domains. `Methods.cpp` includes these
headers through the upstream `Eluna*` runtime, so none is safe to include or
reuse directly in the current bridge.

| Reference file | Intended Tortoise owner | Classification | Reuse decision |
| --- | --- | --- | --- |
| `ObjectMethods.h` | `Object` | `direct candidate after safe wrapper` | Reference only; no direct source reuse. |
| `WorldObjectMethods.h` | `WorldObject` | `direct candidate after safe wrapper` | Reference only; no direct source reuse. |
| `UnitMethods.h` | `Unit` | `direct candidate after safe wrapper` | Reference only; no direct source reuse. |
| `PlayerMethods.h` | `Player` | `adapter required` | Only the independent POC `SendBroadcastMessage` is shipped. |
| `CreatureMethods.h` | `Creature` | `adapter required` | Reference only; entry/map ownership must be checked. |
| `GameObjectMethods.h` | `GameObject` | `adapter required` | Reference only; no unmanaged object pointers. |
| `ItemMethods.h` | `Item` plus player inventory | `adapter required` | Reference only; items can be removed during use. |
| `QuestMethods.h` | `Quest` template | `direct candidate after safe wrapper` | Reference only; templates are core-owned and read-only by default. |
| `MapMethods.h` | `Map` | `adapter required` | Reference only; map lifetime and thread rules are core-specific. |
| `CorpseMethods.h` | `Corpse` | `adapter required` | Reference only; corpse removal must invalidate handles. |
| `SpellMethods.h` | `Spell` | `adapter required` | Reference only; spell objects are temporary and mutable. |
| `AuraMethods.h` | `Aura` | `adapter required` | Reference only; aura application/removal can invalidate the object. |
| `VehicleMethods.h` | `Vehicle` | `core gap` | Expansion-specific and no active Tortoise vehicle surface. |
| `GroupMethods.h` | `Group` | `adapter required` | Reference only; group membership changes during callbacks. |
| `GuildMethods.h` | `Guild` | `adapter required` | Reference only; guild operations have database and player state effects. |
| `BattleGroundMethods.h` | `BattleGround` | `core gap` | Reference only; no active global battleground lifecycle dispatch. |
| `WorldPacketMethods.h` | `WorldPacket` | `adapter required` | Reference only; packet ownership and mutation are restricted. |
| `GlobalMethods.h` | world/object/database managers | `adapter required` | Reference only; global methods need explicit thread and permission rules. |
| `ElunaQueryMethods.h` | database query result | `core gap` | Deferred until synchronous/asynchronous ownership rules exist. |
| `BigIntMethods.h` | Lua value conversion | `adapter required` | Reference only; audit Lua 5.1 integer/number behavior first. |

Upstream `methods/Mangos` is a secondary reference. Upstream templates,
`ElunaIncludes.h`, `ElunaLoader.cpp`, and `ElunaTemplate.h` assume a different
core adapter, pointer tracking model, expansion set, and runtime ownership;
copying them would violate this contract.

## Return and Cancellation Matrix

| Hook family | Tortoise behavior | Eluna compatibility rule |
| --- | --- | --- |
| Notification (`WorldScript`, most `PlayerScript`, map and group/guild notifications) | Native methods return `void`; registry walks all registered scripts. | Lua return values are ignored. A callback error is logged and dispatch continues. |
| Packet allow/reject | `ServerScript::CanPacketSend/CanPacketReceive` returns `bool`; the caller stops sending/handling when a script rejects. | `true` allows, `false` rejects. No packet replacement is supported by the Tortoise hook. |
| Group chat allow/reject | `PlayerScript::CanUseGroupChat` returns `bool`; `false` suppresses the line. | `true` allows, `false` suppresses. The group object is not an argument. |
| Gossip, quest, item, and gameobject handlers | `ScriptMgr` treats `true` as handled and short-circuits the default action. | `true` prevents/handles the default action; `false` continues. Menus are cleared by the core before selected database-bound callbacks. |
| Mutable player values | `OnGiveXP`, `OnMoneyChanged`, `OnReputationChange`, and chat hooks receive native references. | A documented numeric/string return replaces the value; missing or invalid return preserves the current value. |
| Combat and spell modifiers | Unit/spell/aura hooks mutate references or return core-specific results. | No generic boolean translation. Each future binding must name the exact native result and invariant. |
| Login POC | `PlayerScript::OnLogin` is notification-only. | Callback return values are ignored. The public argument order remains `(event, player)`. |

## POC Regression Contract

The following names and behavior must remain unchanged until a stronger test
replaces them:

- `modules/mod-eluna/lua_scripts/eluna_poc.lua` registers
  `RegisterPlayerEvent(3, OnLogin)`.
- Its callback calls
  `player:SendBroadcastMessage("Eluna POC: Lua login event received.")`.
- With `Eluna.Enabled = 1`, the module loads scripts from `Eluna.ScriptPath`
  and logs script loading.
- A Lua compile, execution, or callback error is logged and does not prevent
  clean world shutdown.
- World shutdown destroys the Lua state exactly once.
- The module remains static-only and the upstream submodule remains pinned and
  unmodified.

## Phase 1 Verification

Run the read-only compatibility guard from the repository root:

```bash
python3 modules/mod-eluna/tools/check_compatibility.py
```

The guard compares the event definitions in `Eluna/hooks/Hooks.h` with this
catalog, checks the supported POC names, and checks that the required lifetime,
return, adapter, and VMangos-reference sections remain present.
