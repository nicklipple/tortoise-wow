# Agent Instructions

## Build Validation

- Do not run builds or compilation commands proactively.
- Leave build validation to the user unless the user explicitly requests it in the current task.
- Do not retry or resume a build after a timeout or source change without that explicit request.
