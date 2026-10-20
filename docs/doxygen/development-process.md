\page development-process Development Process Specification

*Author: Martin Fischer.*

## Purpose

This specification defines the process for using Mercurial and the issue tracking system during development. It is motivated by a recent increase in bug reports and Mercurial branches, which has reduced clarity rather than improved order. The goal is to restore a process that provides complete change documentation, a comprehensible Mercurial repository, and a clear path for users to report issues and submit suggestions.

## Issue Tracking

- **Bug tickets** shall document bugs from the user's perspective.
- A **separate feature tracker** shall be used for new feature requests and ideas.

## Branching Model

### Release Branches

- Development of each new release shall take place in its own dedicated branch.

### Feature Branches

- Each new feature for a release shall be developed in a feature branch created from the corresponding release branch.
- All commits related to the feature shall be made to this feature branch until the developer has verified that the feature is complete and free of errors.
- Once verified, the feature branch shall be merged into the release branch.
- After the merge, the feature branch shall be closed to indicate that the feature is finished.

### Issues During Feature Development

- Problems encountered while developing a feature shall **not** be filed as bug tickets. They are considered a normal part of the development process.

## Error Handling

Two scenarios apply when correcting errors:

### Localized Fixes

- Fixes affecting only one or a few places in the code shall be applied directly to the release branch.
- Examples: missing initializations, minor logic adjustments.

### Broad-Impact Fixes

- Errors with wider implications shall be documented and handled in a dedicated **bugfix branch**.
- The procedure shall mirror feature development: multiple commits, followed by a merge into the release branch, followed by closing the branch.
- Example: a bugfix in the FormAPI.

## Expected Outcome

Adhering to this process shall ensure:

- Complete documentation of all changes.
- An easily understandable Mercurial repository.
- An obvious way for users to report discovered errors and submit suggestions.
