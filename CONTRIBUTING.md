# Contributing to Chimedes

Thanks for your interest in Chimedes! Bug reports, ideas and pull requests are all welcome.

## Reporting a bug or suggesting a feature

Open an [issue](https://github.com/bfailach/Chimedes/issues/new/choose) using one of the templates. For bugs, the most useful things are the steps to reproduce the problem, your operating system and Chimedes version, and, if possible, the `.chimedes` project that shows it.

## Building

See [Building from source](README.md#building-from-source) in the README. For quick iterations on the model and engine you only need the tests, which do not depend on JUCE:

```sh
cmake -S . -B build-tests -DCHIMEDES_BUILD_APP=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## Pull requests

- Keep each pull request focused on one change.
- Code, comments, commit messages and UI text are in English.
- Follow the existing style: C++20, four-space indentation, JUCE naming conventions in `Source/ui` and `Source/audio`, plain C++ with no JUCE dependency in `Source/model` and `Source/engine`.
- Add or update tests in `Tests/` when you change the model or the engine.
- Make sure CI passes.
- Add a line under **Unreleased** in [CHANGELOG.md](CHANGELOG.md) for user-visible changes.

## Making a release (maintainers)

1. Update `project(Chimedes VERSION x.y.z)` in `CMakeLists.txt`.
2. Move the **Unreleased** notes in `CHANGELOG.md` to a new `## [x.y.z] - YYYY-MM-DD` section.
3. Commit, then tag and push: `git tag vx.y.z && git push origin vx.y.z`.

The **Release** workflow builds `Chimedes.exe`, runs the tests and publishes the GitHub release with the notes from the changelog.

## Licence

By contributing, you agree that your contributions are licensed under the [GNU AGPL v3](LICENSE), like the rest of the project.
