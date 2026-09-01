#!/bin/sh
set -eu

usage() {
  cat <<'EOF'
Usage:
  deploy.sh --env <dev-1|dev-2|prod> --image <image> [--pull]
  deploy.sh --env <dev-1|dev-2|prod> --build-local
EOF
}

die() {
  printf 'deploy: %s\n' "$*" >&2
  exit 1
}

env_name=
image=
build_local=0
pull=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --env)
      [ "$#" -ge 2 ] || die '--env requires a value'
      env_name=$2
      shift 2
      ;;
    --image)
      [ "$#" -ge 2 ] || die '--image requires a value'
      image=$2
      shift 2
      ;;
    --build-local)
      build_local=1
      shift
      ;;
    --pull)
      pull=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      die "unknown option: $1"
      ;;
  esac
done

[ -n "$env_name" ] || die '--env is required'
case "$env_name" in
  dev-1|dev-2|prod) ;;
  *) die "unsupported environment: $env_name" ;;
esac

if [ "$env_name" = "prod" ] && [ "${CI_COMMIT_BRANCH:-}" != "master" ]; then
  die 'prod deployments are only allowed from master'
fi

if [ "$build_local" -eq 1 ] && [ -n "$image" ]; then
  die '--build-local and --image cannot be used together'
fi
if [ "$build_local" -eq 0 ] && [ -z "$image" ]; then
  die 'one of --build-local or --image is required'
fi

config_dir=${TORTOISE_DEPLOY_CONFIG_DIR:-/etc/tortoise-wow}
config_file="${config_dir}/${env_name}.env"
[ -r "$config_file" ] || die "cannot read environment config: $config_file"

# The environment file is server-managed and contains Compose-compatible shell
# assignments, including credentials that must not be committed.
set -a
. "$config_file"
set +a

: "${COMPOSE_FILE:?COMPOSE_FILE is required in $config_file}"
: "${COMPOSE_PROJECT_NAME:?COMPOSE_PROJECT_NAME is required in $config_file}"
: "${DATA_PATH:?DATA_PATH is required in $config_file}"

case "$COMPOSE_FILE" in
  /*) ;;
  *) die "COMPOSE_FILE must be an absolute path: $COMPOSE_FILE" ;;
esac
case "$DATA_PATH" in
  /*) ;;
  *) die "DATA_PATH must be an absolute path: $DATA_PATH" ;;
esac

[ -f "$COMPOSE_FILE" ] || die "Compose file does not exist: $COMPOSE_FILE"
[ -d "$DATA_PATH" ] || die "client data directory does not exist: $DATA_PATH"
for artifact in dbc maps vmaps mmaps; do
  [ -d "${DATA_PATH}/${artifact}" ] || die "missing client data directory: ${DATA_PATH}/${artifact}"
done

if [ "$build_local" -eq 1 ]; then
  script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
  repo_root=$(CDPATH= cd -- "${script_dir}/.." && pwd)
  local_user=${USER:-local}
  local_user=$(printf '%s' "$local_user" | tr -c 'A-Za-z0-9_.-' '-')
  local_tag="$(date -u '+%Y%m%d%H%M%S')-$$"
  image="tortoise-wow:local-${local_user}-${local_tag}"

  docker build --pull \
    --file "${repo_root}/Dockerfile" \
    --tag "$image" \
    "$repo_root"
fi

export TURTLE_IMAGE="$image"
compose() {
  docker compose \
    --project-name "$COMPOSE_PROJECT_NAME" \
    --env-file "$config_file" \
    --file "$COMPOSE_FILE" "$@"
}

compose config --quiet
if [ "$pull" -eq 1 ]; then
  compose pull
  pull_policy=never
else
  pull_policy=missing
fi
compose up -d --pull "$pull_policy" --remove-orphans
