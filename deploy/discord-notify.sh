#!/bin/sh
set -u

status=${1:-unknown}
webhook=${DISCORD_WEBHOOK:-}

# Notifications are optional so an unavailable Discord endpoint cannot stop a deploy.
[ -n "$webhook" ] || exit 0

if ! command -v curl >/dev/null 2>&1 || ! command -v jq >/dev/null 2>&1; then
  printf 'discord: curl and jq are required for deployment notifications\n' >&2
  exit 0
fi

case "$status" in
  started)
    status_title='Deployment started'
    color=3447003
    ;;
  success)
    status_title='Deployment succeeded'
    color=5763719
    ;;
  failed)
    status_title='Deployment failed'
    color=15548997
    ;;
  canceled)
    status_title='Deployment canceled'
    color=16776960
    ;;
  *)
    status_title='Deployment status'
    color=9807270
    ;;
esac

environment=${DEPLOY_ENV:-${CI_ENVIRONMENT_NAME:-unknown}}
project=${CI_PROJECT_PATH:-tortoise-wow}
ref=${CI_COMMIT_REF_NAME:-unknown}
current_sha=${CI_COMMIT_SHA:-}
previous_sha=${CI_COMMIT_BEFORE_SHA:-}
short_sha=${CI_COMMIT_SHORT_SHA:-unknown}
project_url=${CI_PROJECT_URL:-}
pipeline_url=${CI_PIPELINE_URL:-${CI_JOB_URL:-}}
range_label='previous SHA unavailable'
commits=''
range_available=0

# Use the pipeline ref range so the notification describes the commits in this deploy.
if [ -n "$current_sha" ] && command -v git >/dev/null 2>&1; then
  current_available=0
  previous_available=0
  zero_sha=0000000000000000000000000000000000000000

  if git cat-file -e "${current_sha}^{commit}" >/dev/null 2>&1; then
    current_available=1
  fi

  if [ -n "$previous_sha" ] && [ "$previous_sha" != "$zero_sha" ]; then
    if ! git cat-file -e "${previous_sha}^{commit}" >/dev/null 2>&1; then
      git fetch --no-tags origin "$previous_sha" >/dev/null 2>&1 || true
    fi
    if git cat-file -e "${previous_sha}^{commit}" >/dev/null 2>&1; then
      previous_available=1
    fi
  fi

  if [ "$current_available" -eq 1 ]; then
    if [ "$previous_available" -eq 1 ]; then
      range_label="${previous_sha}..${current_sha}"
      range_available=1
      commits=$(git log --no-decorate --format='- `%h` %s' \
        "${previous_sha}..${current_sha}" 2>/dev/null || true)
    else
      range_label='current revision (previous SHA unavailable)'
      commits=$(git log --no-decorate --format='- `%h` %s' \
        -1 "$current_sha" 2>/dev/null || true)
    fi
  fi
fi

if [ -z "$commits" ]; then
  if [ "$range_available" -eq 1 ]; then
    commits='No new commits in this range.'
  elif [ -n "${CI_COMMIT_TITLE:-}" ]; then
    commits="- \`${short_sha}\` ${CI_COMMIT_TITLE}"
  else
    commits='Commit details were unavailable in the runner checkout.'
  fi
fi

commit_url=''
if [ -n "$project_url" ] && [ -n "$current_sha" ]; then
  commit_url="${project_url}/-/commit/${current_sha}"
fi

image=${CI_REGISTRY_IMAGE:-unknown}
if [ -n "$current_sha" ]; then
  image="${image}:${current_sha}"
fi

timestamp=$(date -u '+%Y-%m-%dT%H:%M:%SZ')
if ! payload=$(jq -n \
  --arg title "$status_title" \
  --arg environment "$environment" \
  --arg project "$project" \
  --arg ref "$ref" \
  --arg short_sha "$short_sha" \
  --arg image "$image" \
  --arg range "$range_label" \
  --arg commits "$commits" \
  --arg commit_url "$commit_url" \
  --arg pipeline_url "$pipeline_url" \
  --arg color "$color" \
  --arg timestamp "$timestamp" \
  '
    def bounded_description:
      if length > 4096 then .[0:4060] + "\n... commit list truncated" else . end;

    ("Project: `" + $project + "`\n" +
     "Environment: `" + $environment + "`\n" +
     "Ref: `" + $ref + "`\n" +
     "Revision: `" + $short_sha + "`" +
       (if $commit_url == "" then "" else " ([view commit](" + $commit_url + "))" end) +
     "\nImage: `" + $image + "`\n" +
     "Commit range: `" + $range + "`\n\n" +
     "**Commits being deployed**\n" + $commits) as $description
    | {
        username: "GitLab Deployments",
        allowed_mentions: {parse: []},
        embeds: [
          ({
            title: ($title + " to " + $environment),
            description: ($description | bounded_description),
            color: ($color | tonumber),
            timestamp: $timestamp
          } + (if $pipeline_url == "" then {} else {url: $pipeline_url} end))
        ]
      }
  '); then
  printf 'discord: could not build deployment notification payload\n' >&2
  exit 0
fi

if ! curl --fail --silent --show-error --retry 2 --retry-delay 1 \
  --connect-timeout 5 --max-time 20 \
  --header 'Content-Type: application/json' \
  --data "$payload" "$webhook" >/dev/null; then
  printf 'discord: failed to post deployment status for %s\n' "$environment" >&2
fi
