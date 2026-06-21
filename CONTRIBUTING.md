# Contributing

## Development setup

The ESPHome source is vendored as a git submodule (`vendor/esphome`) so that IDE
imports/includes resolve while developing `components/wmmm/`. Initialise it after
cloning:

```sh
git submodule update --init --recursive
```

This submodule is a **dev-only IDE aid** and is unrelated to how end users consume a
release (see [Consuming a specific version](#consuming-a-specific-version) below).

## Commits & pull requests

This project uses **[Conventional Commits](https://www.conventionalcommits.org)** for
pull-request titles. A GitHub Action validates every PR title and blocks non-conventional
ones.

Allowed types:

`feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `style`

Example:

```
feat: support extended WMMM opcode 0x3F
```

A scope is optional (`fix(parser): ...`). Scopes are not currently enforced.

We use **squash-merge**. The PR title becomes the single commit on `main`, so the title
*is* the commit message -- keep it conventional.

## Branching

- `main` is the integration and release branch. Open feature PRs against `main`.

## Releases

Merging a PR into `main` triggers
[release-please](https://github.com/googleapis/release-please), which opens a
`chore: release X.Y.Z` pull request. Merge that PR to cut a tag (`vX.Y.Z`),
publish a [GitHub Release](https://github.com/dangreco/wmmm/releases), and update
[`CHANGELOG.md`](CHANGELOG.md).

Releases follow [Semantic Versioning](https://semver.org). Only `feat`, `fix`, `perf`,
and `BREAKING CHANGE` trigger a release; `chore`, `ci`, `docs`, etc. do not.

## Consuming a specific version

This is a source library with no registry artifact. Pin a release tag in your ESPHome
config:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dangreco/wmmm.git
      ref: v1.2.3
    components: [wmmm]
```

End users do not need the `vendor/esphome` submodule — ESPHome fetches the component
itself via `external_components:`.
