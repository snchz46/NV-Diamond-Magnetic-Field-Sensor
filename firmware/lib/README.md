# `lib/` Directory

Use this directory for reusable firmware components that are shared across the application but do not belong to a single feature area.

Examples:
- third-party helpers that are vendored locally
- generic digital filters
- utility serialization helpers
- board abstraction layers

If a component is specific to one subsystem, keep it under `src/` instead.
