# mod-eluna Implementation Plan

## Guiding Principles

These principles are the default decision rules for the work below:

1. **Tortoise is the source of truth.** The Tortoise object model, hook signatures, lifecycle, thread rules, and ownership rules take precedence over upstream Eluna assumptions.
2. **Use upstream as a reference, not a drop-in dependency.** The pinned Eluna submodule provides public API intent, naming, and useful implementation patterns. Do not attempt to compile its full core adapter without proving each compatibility boundary.
3. **Prefer a small native adapter over broad compatibility shims.** Add Tortoise-facing code in `mod-eluna` first. Change core hook sites only when an existing hook cannot safely express a required behavior.
4. **Safety beats API breadth.** Lua must not retain unchecked raw pointers to Tortoise objects. Callback errors, shutdown, despawn, logout, and reload must not crash the server or invoke methods on stale objects.
5. **Preserve Eluna semantics where they fit.** Keep familiar names, event IDs, argument order, and return behavior when practical. Any intentional incompatibility must be documented and tested rather than silently approximated.
6. **Implement vertical slices.** Each phase should deliver a useful script capability, a buildable server, and a focused smoke or regression test. Avoid landing large collections of unverified bindings.
7. **Keep the POC working throughout.** The login callback and `Player:SendBroadcastMessage()` are permanent regression coverage until replaced by stronger tests.
8. **Port by value, not by file count.** Prioritize APIs needed by real Turtle WoW scripts. Do not promise parity with every upstream method or hook before the corresponding Tortoise behavior exists.
9. **Keep the submodule pinned and clean.** Adapt upstream code in the parent module; do not make local edits inside `modules/mod-eluna/Eluna` unless the integration strategy explicitly changes.
10. **Make deployment reproducible.** Configuration paths, Lua script installation, module linkage, and test commands must work from a clean out-of-source build and the supported Docker path.

## Status Tracker

- [x] Phase 0: Build integration and Lua login POC
- [x] Phase 1: Compatibility contract and API inventory
- [ ] Phase 2: Runtime kernel and safe object wrappers
- [ ] Phase 3: Lifecycle, callbacks, and event registry
- [ ] Phase 4: Player vertical slice
- [ ] Phase 5: World, server, and map events
- [ ] Phase 6: Common object and player API expansion
- [ ] Phase 7: Creature, gameobject, gossip, item, and quest APIs
- [ ] Phase 8: Combat, spell, aura, group, guild, vehicle, and instance APIs
- [ ] Phase 9: Database, timers, reload, hardening, and release documentation

Check a phase only after all of its acceptance criteria are met. Keep implementation notes, compatibility decisions, and deferred items in the phase section where they belong.

## Current State

The current implementation is a deliberately small native bridge split across
the runtime, loader, callback registry, error reporter, bindings, and handle
registry under `src/`:

- Prepares the runtime during the core startup hook, then creates and owns one
  Lua 5.1 state on the dedicated world thread. All Lua calls, callback
  registration, and teardown are rejected outside that owner thread.
- Loads `.lua` files recursively from `Eluna.ScriptPath` through
  `Eluna::ScriptLoader`, with deterministic path ordering and per-script error
  context.
- Exposes `RegisterPlayerEvent(3, callback)` for player login through the
  generic `Eluna::CallbackRegistry`; a later registration replaces and unrefs
  the earlier callback, and shutdown clears every registry reference.
- Pushes a `Player` userdata whose payload contains only a GUID, generation,
  and wrapper type. The `Object` -> `WorldObject` -> `Unit` -> `Player`
  metatable hierarchy is established without exposing unmanaged core objects.
- Resolves a player handle on each native call through `ObjectAccessor`,
  checking wrapper type, generation, GUID type, world membership, deletion
  state, and GUID identity. Player generations are invalidated before logout,
  on state teardown, and when a GUID is activated again.
- Logs script compile, execution, and callback failures with both script and
  callback context. Callback failures are consumed by `lua_pcall` and do not
  unwind into Tortoise.
- Defines the Phase 2 conversion boundary: checked Lua integers and strings,
  strict function and boolean types, copied GUIDs/enums, nil only for
  explicitly optional arguments, and validity-checked userdata for objects.
- Builds Lua as a position-independent static library and links `mod-eluna`
  statically. With `BUILD_TESTING=ON`, the standalone
  `mod_eluna_handle_tests` target covers invalidation, generation changes, and
  GUID reuse without requiring client data.

The POC intentionally does not provide reload, timers, database queries, broad
method registration, multiple callbacks per event, or the upstream Eluna
engine. Those remain later-phase work; the existing login callback and
`Player:SendBroadcastMessage()` continue to be the only public script surface.

The upstream inventory is large enough to require deliberate scope control:

- The closest upstream adapter is `methods/VMangos`, matching this core's vMaNGOS lineage, with 20 method headers and approximately 969 registered bindings. `methods/Mangos` remains a useful secondary reference.
- The upstream hook surface contains 13 hook source files and approximately 195 event rows across packet, server, player, guild, group, vehicle, creature, gameobject, spell, item, gossip, battleground, map, and instance categories.
- Tortoise already provides useful native dispatch through `WorldScript`, `PlayerScript`, `UnitScript`, `ServerScript`, `MapScript`, `AllMapScript`, `CreatureScript`, `GameObjectScript`, `ItemScript`, `AllSpellScript`, `GroupScript`, and `GuildScript`.

## Phase 0: Build Integration and Lua Login POC

**Status: Complete.** This phase establishes the smallest end-to-end proof that the module, Lua runtime, configuration, script installation, and Tortoise hooks work together.

### Scope

- Keep the Eluna submodule present and pinned in the parent repository.
- Fetch and statically link Lua 5.1.5 with a fixed hash.
- Enable `mod-eluna` through the static module build.
- Load the configured Lua directory at world startup.
- Dispatch the player login event and expose `SendBroadcastMessage()`.

### Acceptance Criteria

- [x] A clean out-of-source configure succeeds with `mod-eluna` statically enabled.
- [x] The server starts with `Eluna.Enabled = 1` and logs script loading.
- [x] The POC Lua script receives login event 3 and sends the expected player message.
- [x] Invalid Lua produces a log error without preventing server shutdown.
- [x] World shutdown destroys the Lua state cleanly.
- [x] The parent repository and submodule remain at their intended commits.

## Phase 1: Compatibility Contract and API Inventory

**Status: Complete.** `COMPATIBILITY.md` records the Tortoise compatibility contract,
the full upstream event catalog, registration-category dispatch matrix, and the
VMangos method inventory. `tools/check_compatibility.py` verifies the catalog,
category coverage, method-header coverage, supported POC surface, and regression
requirements.

### Scope

- Define the supported Eluna public surface for the Tortoise client/core version.
- Map upstream event categories and public event IDs to Tortoise hook classes and signatures.
- Identify methods that can be implemented directly, methods requiring an adapter, and methods that are unsupported.
- Record differences in return values, cancellation, mutable arguments, object lifetime, thread affinity, and expansion-specific behavior.
- Choose naming and error behavior for Tortoise-only APIs.

### Acceptance Criteria

- [x] A compatibility matrix lists every planned event category and its Tortoise dispatch point.
- [x] The first supported API set is explicitly listed; unsupported upstream APIs are not implied to work.
- [x] Each planned binding identifies its owning Tortoise type and lifetime rules.
- [x] Return-value and cancellation semantics are documented for notification and filter hooks.
- [x] The matrix distinguishes `methods/VMangos` reference code from code that is safe to reuse directly.
- [x] The POC behavior and public names are recorded as regression requirements.

### Verification

- [x] `python3 -B modules/mod-eluna/tools/check_compatibility.py` passes with all 195 upstream event rows covered.
- [x] The standard module-enabled out-of-source build was run and passed by the user.

## Phase 2: Runtime Kernel and Safe Object Wrappers

**Status: In progress.** The runtime foundation is split into focused
components, owns Lua on the world thread, and uses generation-checked
non-owning object handles. Broader event dispatch and public method expansion
remain deferred to later phases.

### Scope

- Split the current monolithic POC into runtime, script loading, callback registration, error handling, and binding responsibilities.
- Define one Lua state ownership model and main-thread execution policy.
- Add a generic callback registry with unregister and cleanup behavior.
- Replace raw retained `Player*` userdata with validity-checked, non-owning handles.
- Establish wrapper behavior for `Object`, `WorldObject`, `Unit`, and `Player`.
- Define conversion and validation rules for integers, strings, booleans, GUIDs, enums, nil, and optional objects.

### Acceptance Criteria

- [x] Existing login scripts work through the new runtime components without behavior changes.
- [x] A Lua callback error is logged with script and callback context and does not unwind into the core.
- [x] A retained or invalidated object handle returns a controlled Lua error or nil rather than dereferencing stale memory.
- [x] Wrapper destruction and Lua garbage collection never delete Tortoise-owned objects.
- [x] Callback registration can be removed during shutdown without invoking freed Lua functions.
- [x] Focused C++ regression coverage exercises invalid handles, generation changes, and repeated activation after invalidation; the standalone test target is available where the test harness permits.

### Verification

- [x] `git diff --check` passes for the implementation.
- [x] `python3 -B modules/mod-eluna/tools/check_compatibility.py` continues to pass the Phase 1 catalog and POC regression guard.
- [x] `make dev-eluna` compiles every current `modules/mod-eluna/src/*.cpp` source with the configured module flags.
- [ ] The module-enabled out-of-source build and `mod_eluna_handle_tests` runtime check must be run by the user because repository guidance forbids autonomous build/test-build execution.

## Phase 3: Lifecycle, Callbacks, and Event Registry

**Status: Not started.** Turn the login-only registration function into a general event mechanism.

### Scope

- Support named and numeric event registration while preserving the intended Eluna event IDs.
- Dispatch multiple callbacks per event with deterministic registration order.
- Define callback return handling for notification, mutable, and cancellable hooks.
- Add world startup, shutdown, update, and configuration lifecycle events.
- Add player login, before-logout, logout, create, save, zone, area, and map-change events.
- Add callback cleanup for logout, object destruction, Lua shutdown, and module shutdown.

### Acceptance Criteria

- [ ] Multiple callbacks for one event run in deterministic order.
- [ ] A callback failure does not prevent later callbacks from running unless the documented semantics require short-circuiting.
- [ ] Tortoise mutable references and boolean return values map to documented Lua return values.
- [ ] Startup, shutdown, login, logout, and reload-related lifecycle ordering is documented and tested.
- [ ] The POC login script remains valid without special-case code in the dispatcher.
- [ ] No callback runs after its Lua state or target object has been invalidated.

## Phase 4: Player Vertical Slice

**Status: Not started.** Deliver the first useful script-authoring capability using existing Tortoise player hooks.

### Scope

- Implement identity and session methods such as name, GUID, level, map, position, and message sending.
- Add chat, emote, spell-cast, XP, level, reputation, and loot event arguments where Tortoise already dispatches them.
- Add the minimum player state mutation methods needed by a real script, with explicit validation.
- Keep the first method batches small, approximately 10 to 30 methods per change.

### Acceptance Criteria

- [ ] A Lua script can register and receive login, logout, chat, level, and map events with documented arguments.
- [ ] A Lua script can read player identity and location reliably.
- [ ] At least one safe player mutation is covered by a runtime smoke test.
- [ ] Event arguments are invalidated or copied according to their documented lifetime.
- [ ] A script can register more than one player event without replacing unrelated callbacks.
- [ ] Build and runtime verification are recorded for each method batch.

## Phase 5: World, Server, and Map Events

**Status: Not started.** Expand lifecycle coverage using the hook sites already present in Tortoise.

### Scope

- Expose world startup, shutdown, update, open-state, MOTD, and config-load events.
- Expose network start/stop and socket lifecycle events.
- Add map creation, destruction, update, grid, player-enter, and player-leave events where the Tortoise map APIs permit.
- Defer packet filtering and packet mutation until their return semantics are stable.

### Acceptance Criteria

- [ ] A Lua script observes world startup and shutdown exactly once per server lifecycle.
- [ ] World update callbacks have a documented tick and execution context.
- [ ] Map callbacks receive the correct map identity and do not retain invalid map objects.
- [ ] Configuration reload behavior distinguishes before-load and after-load callbacks.
- [ ] Network and packet callbacks cannot block or corrupt the core packet path.
- [ ] A failing world or map callback is isolated and logged.

## Phase 6: Common Object and Player API Expansion

**Status: Not started.** Port the high-value common methods in use-case-sized bundles rather than importing all upstream headers.

### Scope

- Implement common `Object`, `WorldObject`, and `Unit` methods first.
- Expand `Player` APIs in separate bundles for identity/session/chat/location, inventory/items/currency, spells/talents, quests/reputation, groups/guilds, and movement/combat.
- Add `Creature`, `GameObject`, `Item`, `Quest`, `Map`, and `Corpse` read-only methods where wrappers are stable.
- Reuse upstream names and signatures only after checking the Tortoise equivalent.

### Acceptance Criteria

- [ ] Every new method has a Tortoise implementation or an explicit unsupported result.
- [ ] Methods validate nil, type, map, ownership, and permission conditions before touching core state.
- [ ] Mutating methods document side effects and are covered by a Lua smoke script.
- [ ] Methods never expose internal pointers as unmanaged Lua-owned memory.
- [ ] Each bundle has a focused change boundary and does not require unrelated upstream domains to compile.
- [ ] Existing scripts continue to load when a later bundle fails to initialize.

## Phase 7: Creature, GameObject, Gossip, Item, and Quest APIs

**Status: Not started.** Add database-bound and interaction-driven scripting after the common wrappers are reliable.

### Scope

- Map creature and gameobject gossip callbacks to Tortoise database-bound script classes.
- Add creature and gameobject spawn, use, quest, damage, state, and removal events supported by Tortoise.
- Add item use, quest, equip, remove, and expire events where core call sites provide safe hooks.
- Define registration by entry, unique identifier, or global event explicitly for each category.

### Acceptance Criteria

- [ ] A database-bound script can register for creature or gameobject interaction without changing unrelated scripts.
- [ ] Gossip return values correctly select, suppress, or continue the core action.
- [ ] Item and quest callbacks receive valid owner and object handles.
- [ ] Despawn, removal, and logout invalidate associated Lua handles.
- [ ] At least one creature, gameobject, item, and quest integration script is tested.

## Phase 8: Combat, Spell, Aura, Group, Guild, Vehicle, and Instance APIs

**Status: Not started.** Port higher-risk domains only after callback and object semantics are proven.

### Scope

- Start with unit notifications for damage, healing, aura application/removal, combat, death, and update.
- Add spell callbacks incrementally, beginning with notification-only cast and hit events.
- Add mutable or cancellable combat/spell behavior only after the corresponding Tortoise hook semantics are tested.
- Add group, guild, vehicle, battleground, instance, and related map APIs one domain at a time.
- Treat AI and spell wrapper code from upstream as reference material, not an assumed compatible implementation.

### Acceptance Criteria

- [ ] Damage, healing, aura, and spell callbacks preserve core invariants under normal and error paths.
- [ ] Callback return values cannot accidentally skip required cleanup or state transitions.
- [ ] Combat callbacks have bounded execution and clear main-thread requirements.
- [ ] Group, guild, vehicle, and instance object lifetime rules are tested.
- [ ] At least one high-frequency event is profiled or otherwise checked for unacceptable overhead.
- [ ] Unsupported expansion-specific APIs fail clearly rather than compiling with incorrect behavior.

## Phase 9: Database, Timers, Reload, Hardening, and Release Documentation

**Status: Not started.** Finish operational features only after state ownership and compatibility behavior are stable.

### Scope

- Add `ElunaQuery` and database access with explicit synchronous/asynchronous and thread rules.
- Add global and object timers using the runtime event processor.
- Add manual reload with state transition, callback cleanup, script cache behavior, and object invalidation rules.
- Add command/configuration support and production logging/metrics as needed.
- Document the supported API, known differences from upstream Eluna, installation, examples, and troubleshooting.
- Add regression coverage for startup, reload, shutdown, stale objects, script errors, and duplicate registration.

### Acceptance Criteria

- [ ] Database calls cannot run on an unsafe thread or retain invalid database resources.
- [ ] Timers are cancelled or invalidated when their owner or Lua state disappears.
- [ ] Reload leaves no callbacks, timers, or object handles pointing at the old state.
- [ ] Reload failure preserves a safe server state and reports the failing script.
- [ ] A clean build and supported Docker deployment install the same module/config/script layout.
- [ ] User-facing documentation lists the supported API and explicit incompatibilities.
- [ ] The final regression suite passes with Eluna enabled and disabled.

## Standard Verification

Use the repository's out-of-source build rules. For a module-enabled Linux build, use the relevant existing dependency options and keep Turtle addon support enabled, for example:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PLAYERBOTS=ON \
  -DMODULES=static \
  -DALLOW_TURTLE_ADDONS=ON
cmake --build build -j$(nproc)
```

Every phase should also include, as applicable:

- A Lua smoke script under `modules/mod-eluna/lua_scripts/`.
- Startup and shutdown log review.
- A runtime check of the affected event or method.
- A negative-path check for Lua errors, nil objects, invalid arguments, or denied operations.
- A clean-worktree check that excludes unrelated local configuration and user edits.

There is no repository-wide lint, typecheck, formatter, or CI command to substitute for these checks.

## Deferred or Explicitly Unsupported Until Proven

- Full upstream Eluna engine compilation.
- Blind inclusion of upstream `ElunaIncludes.h`, `ElunaTemplate.h`, `ElunaLoader.cpp`, or all `methods/VMangos` headers.
- Raw pointer retention across logout, despawn, reload, or shutdown.
- Packet mutation, combat cancellation, and spell/aura mutation without verified Tortoise semantics.
- Database queries before thread and ownership rules are documented.
- File-watcher reload before manual state teardown is reliable.
- API parity as a goal independent of actual Turtle WoW use cases.
