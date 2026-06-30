---
name: implement
description: Autonomously implement a request on this repository (Telegram Desktop). Accept an inline description, a task-list file, or just a project name with a prepared .ai/<project>/tasks/about.md default task source, normalize it into a project with a testability-split task list, then drive each task to test-approval through an isolated per-task runner subagent that does context, planning, implementation, build, a single review pass, and an in-app test loop. Use when the user wants one prompt — ideally under Goal run mode — to carry work all the way to a tested, approved state with persistent .ai/ artifacts and a thin parent thread. Reuses task-think's phase prompts and the shared test-loop protocol; prefers spawn_agent/wait_agent and keeps the main thread lean.
---

# Implement Pipeline

You are the top orchestrator. Take a request -- an inline description OR a task-list file --
normalize it into a project with a testability-split task list, and drive each task to
test-approval through an isolated per-task **task-runner** subagent. Keep your context lean: hold
only the task list and one compact summary per task. All heavy work (planning, coding, building,
testing) happens inside subagents whose context is discarded.

This tested superset of `task-think` does not re-specify the implementation phases or the test
loop. It reuses:
- `.agents/skills/task-think/PROMPTS.md` — Phase 0-6 prompt templates and the Codex execution-mode
  / wait-ladder / progress-heartbeat / compact-reply rules.
- `.agents/shared/test-loop.md` — the harness-neutral impl⇄test loop (state machine, commit
  handoff, overlay mechanics, `--3way`/re-author, test-account swap, watchdog, escalation).

Read both before orchestrating.

## Inputs

`$ARGUMENTS` = ONE of:
- an inline task description (e.g. `add a dark-mode toggle to settings`)
- a path to a task-list file (e.g. `.ai/communities/tasks.txt` -- a rough list of tasks to refine)
- an existing project name to resume, optionally followed by extra work
- **just a project name with a prepared `.ai/<project>/tasks/about.md`** -- the default task source
  (see Artifacts). With no other input, `implement <project>` plans and implements straight from that
  file, so `implement communities` alone fires the full pipeline off `.ai/communities/tasks/about.md`.

May also reference attached images.

## Config

Runs in the **current checkout** — wherever the skill is invoked. No worktrees are created; paths
below are relative to that repository root.

```
BUILD         = cmake --build ./out --config Debug --target Telegram
EXE           = ./out/Debug/Telegram.exe   (or ./out/Debug/Telegram on WSL/Linux — verify the tree)
TEST_ACCOUNT  = ./out/Debug/test_TelegramForcePortable   (user-prepared golden; launch gate aborts if absent)
MAX_ATTEMPTS  = 4
```

`EXE` is also the process-cleanup scope. Any autonomous kill step must match the full executable
path for THIS checkout's built binary only; never blanket-kill all `Telegram.exe` processes.

Tasks run **sequentially** in this one checkout (the build cache stays warm; app runs must serialize
against the account anyway). To parallelize, run the skill in a different checkout/slot (e.g.
`C:\Telegram\tdesktop`, `D:\Telegram\tdesktop`, `D:\Telegram\twin`). Each run is independent and
single-tree. Never run the **test phase** in two slots against the same account at once; concurrent
clients on one auth key can trigger a session reset, so give parallel slots separate test accounts.

## Artifacts (per project)

- `.ai/<project>/tasks/about.md` — the **default task source**: a human-prepared description (or
  rough list) of the batch to implement, with any mockups dropped beside it in `.ai/<project>/tasks/`.
  This is the file the user prepares; the planner reads it as SOURCE when `implement <project>` is
  invoked with no other input. It is **distinct** from the project blueprint `.ai/<project>/about.md`
  (the `tasks/` subdir is what disambiguates them).
- `.ai/<project>/implementing.md` — the canonical, final, testability-split task list (descriptions
  + status); the main thread is its only writer.
- `.ai/<project>/images/` — illustrations referenced by tasks (`images/01.png`, ...).
- `.ai/<project>/<letter>/` — per-task artifacts (context, plan, review, test, result, overlay, logs).
- `.ai/<project>/about.md` — project blueprint (task-think convention).

## Done (for Goal run mode)

The run is **done** when every task in `implementing.md` has `Status: approved` or
`Status: blocked: <reason>`. Under Goal run mode this is the stop condition. The run is
**resumable**: re-invoking with the project name reads `implementing.md` and continues from the
first unfinished task.

## Phase A: Setup & input resolution (main thread)

1. Record `$START_TIME` (for example with `Get-Date`).
2. **Test-account gate (hard precondition — before any work).** If
   `out/Debug/test_TelegramForcePortable` does not exist, STOP the entire skill immediately and tell
   the user that the test account is not prepared: create `out/Debug/test_TelegramForcePortable`
   (a portable-data folder authed to a throwaway test account) before `implement` can run, because
   autonomous testing is impossible without it. Do no implementation work.
3. **Resolve `$ARGUMENTS` into (project, SOURCE, mode) — without reading task files or images.**
   The main thread never loads task prose or assets; resolving needs only paths and existence checks.
   SOURCE ends as EITHER inline text OR a confirmed file path that the planner will read.
   - **File input** — if the first token is a path: confirm it exists (for example with `Test-Path`,
     do NOT read it) and set SOURCE = that path. If the path is under `.ai/<name>/`, project =
     `<name>`; else derive a short kebab name from the filename. Mode = **extend** if that project
     already has `implementing.md`, else new.
   - **Existing project** — else if `.ai/<FIRST_TOKEN>/` exists: project = `FIRST_TOKEN`.
     - If there is a **remainder** → mode = **extend**: if the remainder is itself a path to an
       existing file, SOURCE = that path (confirm it exists, do NOT read it); otherwise SOURCE = the
       remainder text.
     - If the remainder is **empty**, resolve SOURCE in this priority order (existence checks only,
       do NOT read):
       1. If `.ai/<project>/tasks/about.md` exists → SOURCE = that file (the **default task
          source**); mode = **extend** if `implementing.md` already exists, else **new**. This is the
          `implement <project>` with a prepared task source path — it fires the full pipeline.
       2. Else if `implementing.md` exists → mode = **resume** (no SOURCE; Phase C finishes the
          still-unfinished tasks).
       3. Else there is nothing to implement — tell the user to prepare `.ai/<project>/tasks/about.md`
          (or pass a description / task-file path) and stop.
   - **New inline** — else SOURCE = all of `$ARGUMENTS`; pick a unique short kebab-case project name
     after consulting `.ai/`.
   After this step you always have a project name and either a SOURCE (inline text or a confirmed
   path) or mode = **resume** — and you have read neither the file nor any image.
4. Create `.ai/<project>/` and `.ai/<project>/images/` if new.
5. **Images must be on disk.** The planner reads images as files, and subagents cannot see chat
   attachments, so every image a task needs must exist as a file (referenced by the SOURCE file, or
   under `.ai/<project>/images/`). The main thread usually cannot save a pasted/inline chat image to
   disk from text-only tools. If the user only pasted an image into chat, either ask them to drop it
   into `.ai/<project>/images/` as a file, or, as a lossy fallback, write a textual description for
   the planner. Do not claim to have saved it. Images the SOURCE file *references by path* are the
   planner's job, not handled here.
6. If mode = **resume**, skip Phase B and go to Phase C.

## Phase B: Planning & testability split (delegate)

Spawn one planner subagent (`fork_context: false`) with this prompt shape:

```
You are a planning/splitting agent for a large C++ codebase (Telegram Desktop).

SOURCE — EITHER an inline request OR a path to a task-list file. If it is a PATH, READ it yourself
(and any task files it points to); the main thread has NOT read it. If it is inline text, use it as
the request:
<the inline description, or the file path>
PROJECT: <project>     MODE: <new | extend>

IMAGES — the SOURCE and/or its task file may reference images by path (resolve them relative to the
SOURCE file's directory, or use absolute paths; when SOURCE is `.ai/<project>/tasks/about.md`, its
sibling files in `.ai/<project>/tasks/` — e.g. the mockup PNGs there — are those images). READ every
referenced image yourself, then COPY
each into `.ai/<project>/images/` with a descriptive kebab-case name, and reference it from the
specific task(s) it pertains to (see "Images per task" below). The main thread did NOT read or move
these — that is your job. If an image exists only as a textual description because the user pasted it
into chat and it could not be saved to a file, it is provided here; treat that description as the
visual spec: <description(s) or none>

Read AGENTS.md. Briefly scan the codebase to gauge scope. Produce the FINAL ordered task list that
satisfies BOTH constraints for every task:

- **Implementable in one pass**: a single agent with a ~200k-token budget must be able to implement
  the task fully on its own WITHOUT triggering context compaction — i.e. a bounded change it can
  read and edit across a handful of files, not a sweep across dozens. If a unit is too big, split it.
- **Independently testable**: each task must yield an observable behavior the test agent can drive
  from an in-app debug overlay and verify via log/screenshot. Split on testable seams, so each task
  ends at a point where something concrete can be exercised and checked.

Use the minimal number of tasks subject to both constraints; preserve dependency order (a task comes
before any task that depends on it). If the SOURCE is already a list, respect its intended breakdown
and refine only as needed: split entries that are too big or not independently testable; you may
merge trivially tiny adjacent entries if the result is still one testable unit.

Write `.ai/<project>/implementing.md` in EXACTLY this format:

# Implementing: <project>

## Goal
<one-line overall goal>

## Tasks

### a: <imperative title>
Status: todo
<2-4 line self-contained description: what to implement and the observable, testable result. Enough
that a fresh agent can act on it.>
Images: images/<file> — <caption>      (this line only if the task uses an image)

### b: <imperative title>
Status: todo
<...>

**Images per task (required).** Every provided image is a design/resource the work must satisfy.
Attach each to the task(s) it pertains to via the `Images:` line, with a caption stating what that
task must match in it (the exact wording on a mockup, the glyph/shape of a resource, etc.). A task
that changes UI / visual / asset behavior MUST cite the specific mockups/resources it has to match;
do not leave such a task without its images, and do not leave a provided image referenced by no task
(if one genuinely applies to none, note why). These per-task references are the oracle the test
phase verifies against — be specific and per-task, not one shared dump on the first task.

Use letters a, b, c... as task ids. Do not plan internals or implement. When done, reply with ONLY a
compact confirmation — `ready — <N> tasks` (extend: `ready — appended <letters>`); do NOT echo the
task list or image contents back, the main thread reads `implementing.md` itself.
```

For **extend** mode, instead instruct the planner to FIRST read the existing `implementing.md`, then
rewrite it as: (1) a TRIMMED completed-history — keep only the **three most recent** `Status: approved`
task blocks (the three nearest the bottom of the file) and drop all earlier approved ones; (2) every
still-unfinished task left untouched, in place and with its status — that is all `todo`, `in-progress`,
and `blocked` blocks (never drop these); then (3) APPEND new lettered tasks (continuing the letter
sequence from the highest letter still present after the trim) after them. The trim only removes
already-approved entries from the list — it never touches the per-task `.ai/<project>/<letter>/`
artifacts on disk, so a follow-up letter can still read an earlier letter's `context.md` even after its
block was trimmed out of `implementing.md`. It must append ONLY tasks from SOURCE not already
represented in `implementing.md` — so re-running `implement <project>` against an unchanged default
`tasks/about.md` appends nothing (the planner replies `ready — appended (none)`, still applying the
completed-history trim) and Phase C just finishes whatever is still unfinished. (Any
`todo`/`in-progress` leftovers from an interrupted run are picked up by Phase C regardless, so
defaulting to extend never loses an in-flight batch — it is a superset of resume.)

After the planner replies `ready`, read `implementing.md` back ONCE (your first and only load of the
task prose; you never read the images). Initialize a progress list mirroring the tasks so progress
is visible.

## Phase C: Per-task loop (main thread orchestrates)

For each task whose `Status` is not `approved`/`blocked`, in order:

1. Set `Status: in-progress` and mark the corresponding progress item in progress. Spawn ONE
   **task-runner** worker (`fork_context: false`, request `model: gpt-5.4`,
   `reasoning_effort: xhigh` when supported) with the prompt below. Apply task-think's wait ladder
   (5-min waits while in progress, 1-2 min near completion; inspect the task's progress/result
   artifacts on timeout; one follow-up then one fresh retry before escalating).
2. Read only its compact reply block. Detail is in `.ai/`.
3. Update the task's `Status:` — `approved` (STATUS DONE) or `blocked: <reason>`.
4. Append any `DISCOVERED` tasks as new lettered `### <letter>:` blocks (`Status: todo`) after the
   remaining ones, and add them to the progress list. The main thread is the only writer of
   `implementing.md`.
5. On BLOCKED, stop and report — do not start the next task. Under Goal run mode, surfacing the
   blocker is the correct stop; the loop should not spin on a blocked task.

### task-runner prompt

```
You are a task-runner for ONE task in an autonomous implement-and-test workflow on Telegram
Desktop (C++ / Qt). You own this task end to end and isolate its context from the orchestrator.
You MUST use subagents (spawn_agent/wait_agent) for each phase, keeping the parent thread lean.

PROJECT: <project>   TASK: <letter> — <title>
TASK DESCRIPTION:
<the task's full description block from implementing.md>
IMAGES: <referenced .ai/<project>/images/* paths, or none — Read them if present>
TASK_DIR: .ai/<project>/<letter>/   TASK_ID: <project>-<letter>
Config (paths relative to this checkout): BUILD/EXE/MAX_ATTEMPTS = <values>. Test account = the
out/Debug/ portable-data folders (see test-loop.md "Test account").

Read first: AGENTS.md; REVIEW.md; `.agents/skills/task-think/PROMPTS.md` (Phase 1-6 templates +
execution rules); `.agents/shared/test-loop.md` (testing). Read any IMAGES listed above. For a
follow-up letter, also read `.ai/<project>/about.md` and the previous letter's `context.md`.
Create `<TASK_DIR>/` and `<TASK_DIR>/logs/`.

Pipeline for THIS task only, spawning a fresh subagent per phase (so each phase's output stays in
YOUR context, not the orchestrator's), writing prompt/progress/result logs per task-think:
1. CONTEXT  — task-think Phase 1 (or 1F) -> context.md (+ about.md).
2. PLAN     — Phase 2 -> plan.md.
3. ASSESS   — Phase 3.
4. IMPLEMENT— Phase 4, one subagent per plan phase. Implementation agents do NOT commit yet; you
   commit after build passes.
5. BUILD    — Phase 5 (prefer same-thread build; fix errors). On file-lock errors, run the
   path-scoped kill of THIS checkout's binary only (see test-loop.md "Serialize app runs") and retry
   once; if the lock persists, return BLOCKED/UNRECOVERABLE with the lock reason. This overrides the
   generic task-think stop-on-lock rule for this autonomous implement workflow.
6. REVIEW   — Phase 6 but a SINGLE pass (one 6a, one 6b if NEEDS_CHANGES, rebuild).
7. COMMIT   — stage the task's intended changes and git commit with a concise plain-language subject
   (≤ ~50-60 chars, matching recent `git log` style; usually the whole message — add a short plain
   body only if the subject can't carry it). NO `Autotask:`/attempt trailer and NO
   `Co-Authored-By:`/attribution line (overrides the default; see test-loop.md "Commit message").
   Commit submodules first if dirty, then bump the pointer. Record the commit SHA as IMPL_SHA (track
   the attempt number yourself).
8. TEST     — run `.agents/shared/test-loop.md` to APPROVED / BLOCKED / attempt cap. Spawn a
   test-author subagent and feed it BOTH sides per test-loop.md "Design the tests from THIS task":
   (1) the TASK SPEC — this task's full description block above PLUS its referenced IMAGES (have it
   READ the mockups; they show the intended result), and (2) the implementation — `git show
   <IMPL_SHA>` + touched files. It designs a falsifiable oracle per change and writes the plan into
   `<TASK_DIR>/test.md` BEFORE running (visual/asset changes compare the tight crop vs old vs
   intended-new art — judged VISUALLY, never by hash/byte; mobile mockups are not pixel targets),
   covers every surface the task names, and never reuses another task's navigate+screenshot. You
   drive RUN/ASSESS yourself, ADVERSARIALLY (no pass-by-inference; missing evidence = TEST_FLAW;
   no-difference-from-before = IMPL_BUG), and keep the human-readable `<TASK_DIR>/test.md` report.
   Spawn an impl-fix subagent on IMPL_BUG (it commits the next attempt → new IMPL_SHA). After each
   run, save the overlay patch into TASK_DIR and `git reset --hard <IMPL_SHA>` so the checkout
   returns to impl-only. Run the test-account SETUP before each launch and honor every test-account
   hard rule (serialize app runs; avoid destructive calls).

Skip TEST only for docs/config-only tasks (say so). On Windows, after approval run task-think
Phase 7 (CRLF / no-BOM) on the task's touched source/config files.

Reply with only the compact summary block from test-loop.md
(TASK/STATUS/VERDICT/ATTEMPTS/TOUCHED/DISCOVERED/NOTES).
```

## Completion

When the loop ends (all tasks approved/blocked, or a blocked task stopped it):
1. Summarize per task: approved vs blocked, attempts, files touched, key test evidence.
2. List any discovered tasks that were added.
3. Note the project name for `implement <project> <follow-up>`.
4. Show total elapsed time (`Xh Ym Zs`, omit zero components).
5. Remind that test overlays are saved as `.ai/<project>/<letter>/test-overlay.patch` and the
   checkout is left at each task's implementation commit (overlays reset away).

## Error handling

- Follow task-think's retry ladder for stuck phases; a task-runner returning BLOCKED stops the loop.
- If `implementing.md` or any artifact is malformed, re-spawn that step with tighter instructions.
- For file-lock build errors, run the autonomous path-scoped kill from test-loop.md and retry once.
  Kill only the resolved `EXE` for this checkout; `taskkill /IM Telegram.exe /F` and other
  image-name-wide kills are forbidden. If the lock persists after the scoped retry, return BLOCKED
  with the lock reason instead of asking for user input.
- The launch gate (Phase A) guarantees the test account exists before any work begins.
- Keep `.ai/` artifacts and edited text files LF/no-BOM on WSL; run CRLF normalization only on a
  native Windows checkout.

## User invocation

`Use local implement skill: <request or path to a task file>` — ideally under Goal run mode so it
loops to a tested state. Resume/extend: `Use local implement skill: <project> [additional change]`.
Default task source: `Use local implement skill: <project>` with a prepared `.ai/<project>/tasks/about.md`
runs the whole pipeline from that file with no other input.
