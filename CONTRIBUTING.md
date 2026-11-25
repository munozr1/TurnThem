# Contributing to TurnThem

Use your own branch for each feature or fix and submit it through a pull request (PR). Keep each PR small enough that a teammate can describe the change in one sentence and verify it in one sitting.

## Pick a change and a base branch

1. Check open issues and PRs for overlapping work. If someone is already working on the same behavior, coordinate on their issue or PR. If you pick up an issue, leave a comment so others know who owns it.
2. Choose one outcome, such as “show which deck slot the player selected.” Include the code, assets, and tests needed for that outcome. Give independently useful fixes and unrelated cleanup their own PRs.
3. Start from the latest `main`. When your change depends on an open PR, branch from that PR’s head, target that branch in your PR, and link the dependency. Once the parent PR merges, check the base and diff again.

To start the deck-selection example from a clone of TurnThem, run

```sh
git fetch origin
git switch --no-track -c feat/deck-selection origin/main
```

Use a short name such as `feat/deck-selection`, `fix/card-hitbox`, or `docs/build-steps`. Push commits to your own branch. Changes reach shared branches through review.

## Follow the data

- Start with the state the feature needs. Identify who owns it, which function updates it, and which code reads it. Use plain structs for records and arrays or vectors for collections processed together. Reuse a class when it already owns the data or resource.
- Search for the behavior and read its callers before adding a function, class, or dependency. For example, card art and animation frame names already live in [the sprite data](assets/sprites_data_file.json). A new card with the same drawing behavior can use that data and the existing [`Atlas` and `CardData` types](include/sprites.h). If a new abstraction is necessary, explain what the existing one cannot represent.
- In the multiplayer simulation, `Match` owns weapon state and steps weapons in placement order. Both `Match` and `MatchView` call `StepWeapon`. A change to weapon timing belongs in that shared function. Preserve tick order and keep rendering and ENet dependencies outside the headless core.

## Verify and open the PR

1. Build using the [README](README.md) on your branch. For a gameplay or UI change, exercise the affected interaction and record what you observed. On branches with the CTest suite, run `(cd build && ctest --output-on-failure)` after building with tests enabled. Extend a nearby test when the change introduces a rule or fixes a regression.
2. Review the diff against your intended base. Keep generated build files and unrelated edits out of your commits. Give each commit a subject that describes the change.
3. Push with `git push -u origin HEAD` and open a PR to the same base you branched from. Describe the behavior, link any issue or parent PR, and record the checks you actually ran. A screenshot or short clip helps reviewers verify visible changes.
4. Request a teammate’s review and address feedback on the same branch. Merge after approval and passing applicable checks. Delete your feature branch after merge when no open PR depends on it.

## Working with an AI assistant

Have your assistant read [AGENTS.md](AGENTS.md) and this guide before editing. Give it the same single outcome and base branch you would give a teammate. Review the full diff yourself and verify the behavior before asking someone else to review it. You own the code and the test results in your PR.
