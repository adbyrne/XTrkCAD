\page development-process Development Process Specification

*Author: Martin Fischer.*

How the [Mercurial repository](https://sourceforge.net/p/xtrkcad-fork/xtrkcad/) and the
SourceForge trackers are used during development: which branch a change belongs on, and where
bugs and feature requests are filed. See the \ref index "Developer Documentation" page for
everything else, and \ref building "Building and Testing" for building and testing a change.

\tableofcontents

# Purpose {#devproc-purpose}

This specification defines the process for using Mercurial and the issue tracking system during
development. It is motivated by a recent increase in bug reports and Mercurial branches, which has
reduced clarity rather than improved order. The goal is to restore a process that provides
complete change documentation, a comprehensible Mercurial repository, and a clear path for users
to report issues and submit suggestions.

# Issue Tracking {#devproc-issue-tracking}

- **Bug tickets** shall document bugs from the user's perspective. They are filed in the
  [Bugs](https://sourceforge.net/p/xtrkcad-fork/bugs/) tracker.
- A **separate feature tracker** shall be used for new feature requests and ideas: the
  [Feature Requests](https://sourceforge.net/p/xtrkcad-fork/feature-requests/) tracker.

Problems found while a feature is being developed are not filed at all; see
\ref devproc-issues-during-feature-development "Issues During Feature Development".

# Branching Model {#devproc-branching-model}

The branches below are Mercurial
[named branches](https://wiki.mercurial-scm.org/NamedBranches).

## Release Branches {#devproc-release-branches}

- Development of each new release shall take place in its own dedicated branch.

## Feature Branches {#devproc-feature-branches}

- Each new feature for a release shall be developed in a feature branch created from the
  corresponding release branch.
- All commits related to the feature shall be made to this feature branch until the developer has
  verified that the feature is complete and free of errors.
- Once verified, the feature branch shall be merged into the release branch.
- After the merge, the feature branch shall be closed to indicate that the feature is finished.

## Issues During Feature Development {#devproc-issues-during-feature-development}

- Problems encountered while developing a feature shall **not** be filed as bug tickets. They are
  considered a normal part of the development process.

# Error Handling {#devproc-error-handling}

Two scenarios apply when correcting errors:

## Localized Fixes {#devproc-localized-fixes}

- Fixes affecting only one or a few places in the code shall be applied directly to the
  \ref devproc-release-branches "release branch".
- Examples: missing initializations, minor logic adjustments.

## Broad-Impact Fixes {#devproc-broad-impact-fixes}

- Errors with wider implications shall be documented and handled in a dedicated **bugfix
  branch**.
- The procedure shall mirror \ref devproc-feature-branches "feature development": multiple
  commits, followed by a merge into the release branch, followed by closing the branch.
- Example: a bugfix in the FormAPI.

# Expected Outcome {#devproc-expected-outcome}

Adhering to this process shall ensure:

- Complete documentation of all changes.
- An easily understandable Mercurial repository.
- An obvious way for users to report discovered errors and submit suggestions.
