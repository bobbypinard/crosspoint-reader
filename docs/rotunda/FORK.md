# Rotunda fork: rules for agents

This repository is a personal downstream fork of CrossPoint Reader that builds
**Rotunda Psalter**, a Daily Office front end for the Xteink X3. Read this with
`AGENTS.md`.

## What still applies from AGENTS.md

Everything about engineering quality: the HAL and SDK boundaries, memory rules
(about 380 KB usable RAM, no PSRAM, one framebuffer), heap discipline, activity
lifecycle, input ownership, formatting, and the testing workflow. Cite file
paths and lines for claims. Justify every heap allocation. Tell me how to verify
every change on the device.

## What is different in this fork

- **Scope.** `SCOPE.md` describes upstream's scope. Rotunda's screens, theme and
  Daily Office features are in scope here even though upstream has frozen new
  themes. Do not propose sending Rotunda work upstream.
- **Branches.** Work on `rotunda` (branched from upstream `develop`). Upstream
  is merged in with `git merge upstream/develop` when I ask.
- **Reviews.** Use a lighter handoff than `firmware-handoff`: run the build and
  the relevant host tests, do one review pass of your own diff, and list what I
  must check on the device. Skip the four independent reviewer agents unless
  I ask for them.
- **Git.** You may create local commits after I approve the change. Never push
  and never open PRs unless I say so.
- **Device.** X3 only (`pio run -e default`). Portrait only for Rotunda
  screens; leave the other orientations working for the stock reader.

## Where the design lives

- `docs/rotunda/DESIGN.md` — tokens, layout, office hours, buttons.
- `docs/rotunda/DATA.md` — the SD-card files Rotunda reads and writes.
- `docs/rotunda/screens/*.png` — target renders, 528 × 792. Match them.
- `docs/rotunda/rotunda-spec-revB.pdf` — the annotated spec sheet.
- `docs/rotunda/sd-sample/` — sample SD data for October 2026.
- `docs/rotunda/PLAN.md` — the build plan (written in step 1, kept current).

## Working style

Small steps. One screen or one data module per change. Before writing code for a
step, say which files you will touch and why, and wait for my go-ahead.
