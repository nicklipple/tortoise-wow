# Agent Notes

## Repository Shape

- This is a CMake/C++17 Turtle WoW server targeting client patch 1.18.1, build 7272.
- `src/` builds `mangosd` and `realmd`; `modules/` contains auto-discovered extensions; `src/modules/PlayerBots/` is a separately linked vendored playerbot library.
- Client-derived runtime data is not committed. A working server needs `dbc`, `maps`, `vmaps`, and `mmaps` extracted from the matching client with the tools under `tools/`.
- `modules/mod-dungeon-clear/CLAUDE.upstream.md` is reference-only and explicitly says its workflow rules do not apply to this tree.

## Build

- Agents must not run configure, build, compile, or test-build commands autonomously (`cmake`, `make`, `ninja`, or equivalent); leave build validation to the user because it can consume substantial CPU. When build verification is relevant, report the exact command for the user to run instead.
- Builds must be out of source: use `cmake -S . -B build ...`; an in-source configure is rejected.
- The normal Linux playerbot build is `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_PLAYERBOTS=ON -DALLOW_TURTLE_ADDONS=ON`, followed by `cmake --build build -j$(nproc)`.
- `BUILD_PLAYERBOTS` defaults to `OFF`, so omit it only when a no-bot build is intended. Keep `ALLOW_TURTLE_ADDONS=ON`; disabling it makes the matching client crash with `interface corrupt`.
- Add `-DUSE_EXTRACTORS=ON` when building the client-data extraction targets (`mapextractor`, `vmapextractor`, `vmap_assembler`, `MoveMapGen`).
- Choose `CMAKE_INSTALL_PREFIX` at configure time. On Linux the prefix contributes to the compiled `SYSCONFDIR`; moving an install or changing the prefix requires reconfiguring and rebuilding.
- Modules default to disabled. Use `-DMODULES=static` (or a sanitized per-module `-DMODULE_<NAME>=static`); `mod-dungeon-clear` also requires `BUILD_PLAYERBOTS=ON`.
- Windows builds should pass `ACE_ROOT` and `BOOST_ROOT` as `-D` cache entries; do not add the vcpkg toolchain file because Windows intentionally mixes bundled OpenSSL/MySQL/zlib with externally supplied ACE/Boost.
- There is no repo-wide lint, typecheck, formatter, or CI workflow. Do not invent a passing command for one.

## Focused Verification

- For `mod-dungeon-clear`, configure a fresh build with `cmake -S . -B build -DBUILD_PLAYERBOTS=ON -DMODULES=static -DBUILD_TESTING=ON`, then run `bash modules/mod-dungeon-clear/t/run_tests.sh`.
- The focused runner builds `dungeon_clear_tests` and applies its filtered gtest suite. It assumes the build cache already has the playerbot module enabled because the runner itself only adds `BUILD_TESTING=ON`.
- Run the module source guards directly when relevant: `python3 modules/mod-dungeon-clear/tools/check_config_reads.py`, `bash modules/mod-dungeon-clear/tools/check_determinism.sh`, and `python3 modules/mod-dungeon-clear/tools/check_msvc_portability.py`.
- Tier-2 nav tests skip when `modules/mod-dungeon-clear/t/fixtures/mapdata/mmaps/` is empty; generate gitignored slices with `python3 modules/mod-dungeon-clear/tools/slice_mapdata.py --datadir <data-dir> --map <id>`.

## SQL And Runtime

- Put new SQL work in `sql/wip_updates/`, name scripts for their table, and comment SQL blocks. Avoid direct spell-table edits; prefer `spell_affect`, `spell_proc_event`, `spell_scripts`, or backend code, with `spell_mod` only for rare runtime edits.
- `sql/setup_databases.sh` does not import `sql/base`; import the complete base world content separately. A playerbot build also needs the classic world and character SQL under `src/modules/PlayerBots/sql/`.
- The base dump and migration history are not inherently aligned. Do not blindly enable the auto-updater on a dump-restored database; follow the documented disable, force-apply, and migration-recording procedure in `INSTALL-LINUX.md` (or use the Docker init path).
- Native `mangosd` must have stdin held open, commonly with the FIFO setup in `INSTALL-LINUX.md`; otherwise it exits on EOF. The Docker entrypoint creates and holds this console FIFO.
- Docker Compose requires `DB_ROOT_PASSWORD` and `DB_PASSWORD` in `.env`; `db-init` must complete before `realmd` and `mangosd`, and the first bot-enabled start can take a long time while caches and travel data are built.
