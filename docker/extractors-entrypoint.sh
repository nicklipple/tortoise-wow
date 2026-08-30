#!/usr/bin/env bash
set -Eeuo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: extractors-entrypoint.sh [--clean] [MoveMapGen options...]

Environment:
  CLIENT_DIR   Read-only Turtle WoW client root containing Data/ (default: /client)
  OUTPUT_DIR   Host-mounted directory for dbc/, maps/, vmaps/, and mmaps/ (default: /output)
  WORK_ROOT    Temporary workspace parent (default: /tmp)

All arguments other than --clean are passed to MoveMapGen.
EOF
}

die() {
  printf 'extractors: %s\n' "$*" >&2
  exit 1
}

has_entries() {
  local directory=$1
  local entries=()
  [[ -d "$directory" ]] || return 1
  shopt -s nullglob
  entries=("$directory"/*)
  shopt -u nullglob
  ((${#entries[@]} > 0))
}

require_entries() {
  local directory=$1
  local description=$2
  has_entries "$directory" || die "$description produced no files in $directory"
}

client_dir=${CLIENT_DIR:-/client}
output_dir=${OUTPUT_DIR:-/output}
work_root=${WORK_ROOT:-/tmp}
clean_output=0
mmap_args=()

for argument in "$@"; do
  case "$argument" in
    --clean)
      clean_output=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      mmap_args+=("$argument")
      ;;
  esac
done

[[ -d "$client_dir/Data" ]] || die "client directory must contain Data/: $client_dir"
mkdir -p "$output_dir" "$work_root"

for artifact in dbc maps vmaps mmaps; do
  artifact_path="$output_dir/$artifact"
  if [[ -e "$artifact_path" ]]; then
    if ((clean_output)); then
      rm -rf "$artifact_path"
    else
      die "$artifact_path already exists; use --clean to replace extractor output"
    fi
  fi
done

run_dir=$(mktemp -d "$work_root/turtle-wow-extract.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT

printf 'extractors: reading client data from %s\n' "$client_dir"
printf 'extractors: staging results in %s\n' "$run_dir"

mapextractor -i "$client_dir" -o "$run_dir" -e 3
require_entries "$run_dir/dbc" 'mapextractor'
require_entries "$run_dir/maps" 'mapextractor'

(
  cd "$run_dir"
  vmapextractor -d "$client_dir/Data"
)
require_entries "$run_dir/Buildings" 'vmapextractor'

mkdir "$run_dir/vmaps"
vmap_assembler "$run_dir/Buildings" "$run_dir/vmaps"
require_entries "$run_dir/vmaps" 'vmap_assembler'

# MoveMapGen returns 1 on successful completion when --silent is used. Its
# negative error returns are non-zero shell statuses, so accept only 1 here.
set +e
(
  cd "$run_dir"
  MoveMapGen --silent "${mmap_args[@]}"
)
mmap_status=$?
set -e
((mmap_status == 1)) || die "MoveMapGen failed with exit status $mmap_status"
require_entries "$run_dir/mmaps" 'MoveMapGen'

for artifact in dbc maps vmaps mmaps; do
  cp -a "$run_dir/$artifact" "$output_dir/"
done

printf 'extractors: wrote dbc, maps, vmaps, and mmaps to %s\n' "$output_dir"
