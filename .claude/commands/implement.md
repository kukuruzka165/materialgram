---
description: Autonomously implement a task (split into a task list if needed), then implement + test each task to approval via isolated per-task subagents
allowed-tools: Read, Write, Edit, Glob, Grep, Bash, Task, AskUserQuestion, TodoWrite
---

# Implement - Autonomous Implement-and-Test Orchestrator

You are the **top orchestrator**. You take a request — an inline description OR a task-list file —
normalize it into a project with a testability-split task list, and drive each task to
test-approval through an isolated per-task `task-runner` subagent. Your context must stay lean: you
hold only the task list and one compact summary per task. All heavy work (planning, coding,
building, testing) happens inside subagents whose context is discarded.

This is the tested superset of `/task`: it reuses `/task`'s phase prompts for implementation and
adds the impl⇄test loop defined in `.agents/shared/test-loop.md`.

**Arguments:** `$ARGUMENTS` = ONE of:
- an inline task description (e.g. `add a dark-mode toggle to settings`)
- a path to a task-list file (e.g. `.ai/communities/tasks.txt` — a rough list of tasks to refine)
- an existing project name to resume (e.g. `communities`), optionally followed by extra work
- **just a project name with a prepared `.ai/<project>/tasks/about.md`** — the default task source
  (see Artifacts). With no other input, `/implement <project>` plans and implements straight from
  that file, so `/implement communities` alone fires the full pipeline off `.ai/communities/tasks/about.md`.
May also reference attached images.

## Config

Runs in the **current checkout** — wherever `/implement` is invoked. No worktrees are created; all
paths below are relative to that repository root.

```
BUILD         = cmake --build ./out --config Debug --target Telegram
EXE           = ./out/Debug/Telegram.exe
TEST_ACCOUNT  = ./out/Debug/test_TelegramForcePortable   # user-prepared golden; launch gate aborts if absent
MAX_ATTEMPTS  = 4
```

Tasks run **sequentially** in this one checkout (the build cache stays warm; app runs must
serialize against the account anyway). To parallelize, launch `/implement` in a different
checkout/slot (e.g. `C:\Telegram\tdesktop`, `D:\Telegram\tdesktop`, `D:\Telegram\twin`) — each run
is independent and single-tree. Don't run the **test phase** in two slots against the same account
at once (concurrent clients on one auth key can trigger a session reset); give parallel slots
separate test accounts.

## Artifacts (per project)

- `.ai/<project>/tasks/about.md` — the **default task source**: a human-prepared description (or
  rough list) of the batch to implement, with any mockups dropped beside it in `.ai/<project>/tasks/`.
  This is the file you prepare; the planner reads it as SOURCE when `/implement <project>` is invoked
  with no other input. It is **distinct** from the project blueprint `.ai/<project>/about.md` (the
  `tasks/` subdir is what disambiguates them).
- `.ai/<project>/implementing.md` — the canonical, final, testability-split task list (descriptions
  + status). Your single source of truth; you are its only writer.
- `.ai/<project>/images/` — illustrations referenced by tasks (`images/01.png`, ...).
- `.ai/<project>/<letter>/` — per-task artifacts (context, plan, review, test, result, overlay).
- `.ai/<project>/about.md` — project blueprint (the `/task` convention).

## Done (for `/goal` loop mode)

The run is **done** when every task in `implementing.md` has `Status: approved` or
`Status: blocked: <reason>`. Under a `/goal` loop this is the stop condition. The run is
**resumable**: re-invoking with the project name reads `implementing.md` and continues from the
first unfinished task.

## Phase A: Setup & input resolution

1. Record start time (`Get-Date`).
2. **Test-account gate (hard precondition — before any work).** If
   `out/Debug/test_TelegramForcePortable` does NOT exist, STOP the entire command immediately and
   tell the user: the test account is not prepared — create `out/Debug/test_TelegramForcePortable`
   (a portable-data folder authed to a throwaway test account) before `/implement` can run, because
   autonomous testing is impossible without it. Do no implementation work.
3. **Resolve `$ARGUMENTS` into (project, SOURCE, mode) — without reading task files or images.**
   The main thread never loads task prose or assets; resolving needs only paths and existence
   checks. SOURCE ends up as EITHER inline text OR a confirmed file path (the planner reads it).
   - **File input** — if the first token is a path: confirm it exists (`Test-Path`, do NOT read it)
     and set SOURCE = that path. If the path is under `.ai/<name>/`, project = `<name>`; else derive
     a short kebab name from the filename. Mode = **extend** if that project already has
     `implementing.md`, else new.
   - **Existing project** — else if `.ai/<FIRST_TOKEN>/` exists: project = `FIRST_TOKEN`.
     - If there is a **remainder** → mode = **extend**: if the remainder is itself a path to an
       existing file, SOURCE = that path (confirm with `Test-Path`, do NOT read it); otherwise
       SOURCE = the remainder text.
     - If the remainder is **empty**, resolve SOURCE in this priority order (existence checks only,
       `Test-Path`, do NOT read):
       1. If `.ai/<project>/tasks/about.md` exists → SOURCE = that file (the **default task
          source**); mode = **extend** if `implementing.md` already exists, else **new**. This is
          the `/implement <project>` with a prepared task source path — it fires the full pipeline.
       2. Else if `implementing.md` exists → mode = **resume** (no SOURCE; Phase C finishes the
          still-unfinished tasks).
       3. Else there is nothing to implement — tell the user to prepare `.ai/<project>/tasks/about.md`
          (or pass a description / task-file path) and stop.
   - **New inline** — else SOURCE = the `$ARGUMENTS` text; pick a unique short kebab-case project
     name (consult `ls .ai/`).
   After this step you always have a project name and either a SOURCE (inline text or a confirmed
   path) or mode = **resume** — and you have read neither the file nor any image.
4. Create `.ai/<project>/` and `.ai/<project>/images/` if new.
5. **Images must be on disk.** The planner reads images as files, and subagents cannot see chat
   attachments — so every image a task needs must exist as a file (referenced by the SOURCE file, or
   under `.ai/<project>/images/`). The main thread **cannot** save a pasted/inline chat image to disk
   (`Write` is text-only; there is no save-attachment tool, and on Windows clipboard-paste isn't even
   supported). So if the user only pasted an image into the chat, either ask them to drop it into
   `.ai/<project>/images/` as a file, or — as a lossy fallback — write a textual description of it for
   the planner. Do not claim to have saved it. Images the SOURCE file *references by path* are the
   planner's job, not handled here.
6. If mode = **resume**, skip Phase B and go to Phase C.

## Phase B: Planning & testability split

Spawn one planner subagent (Task, `general-purpose`):

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
these — that is your job. If an image exists only as a textual description (because the user pasted
it into chat and it could not be saved to a file), it is provided here — treat that description as
the visual spec: <description(s) or none>

Read AGENTS.md. Briefly scan the codebase to gauge scope. Produce the FINAL ordered task list that
satisfies BOTH constraints for every task:

- **Implementable in one pass**: a single agent with a ~200k-token budget must be able to implement
  the task fully on its own WITHOUT triggering context compaction — i.e. a bounded change it can
  read and edit across a handful of files, not a sweep across dozens. If a unit is too big, split
  it.
- **Independently testable**: each task must yield an observable behavior the test agent can drive
  from an in-app debug overlay and verify via log/screenshot. Split on testable seams, so each task
  ends at a point where something concrete can be exercised and checked.

Use the minimal number of tasks subject to both constraints; preserve dependency order (a task
comes before any task that depends on it). If the SOURCE is already a list, respect its intended
breakdown and refine only as needed: split entries that are too big or not independently testable;
you may merge trivially tiny adjacent entries if the result is still one testable unit.

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
represented in `implementing.md` — so re-running `/implement <project>` against an unchanged default
`tasks/about.md` appends nothing (the planner replies `ready — appended (none)`, still applying the
completed-history trim) and Phase C just finishes whatever is still unfinished. (Any
`todo`/`in-progress` leftovers from an interrupted run are picked up by Phase C regardless, so
defaulting to extend never loses an in-flight batch — it is a superset of resume.)

After the planner replies `ready`, read `implementing.md` back ONCE (your first and only load of the
task prose; you never read the images). Initialize a TodoWrite list mirroring the tasks so progress
is visible.

## Phase C: Per-task loop

For each task in `implementing.md` whose `Status` is not `approved`/`blocked`, in order:

1. Set its `Status: in-progress` (and mark `in_progress` in TodoWrite).
2. Spawn ONE `task-runner` subagent (Task, `general-purpose`) with the prompt below. Wait for it.
3. Read ONLY its compact summary block (the `task-runner` writes all detail to `.ai/`).
4. Update the task's `Status:` line — `approved` if `STATUS: DONE`, else `blocked: <reason>`.
5. If `DISCOVERED` lists new tasks, append them to `implementing.md` as new lettered `### <letter>:`
   blocks (`Status: todo`) **after** the current remaining tasks, and add them to TodoWrite. (You
   are the only writer of `implementing.md`, so there are no write races.)
6. If `STATUS: BLOCKED`, stop the loop and report to the user — do not start the next task. (Under
   `/goal`, surfacing the blocker is the correct stop; the loop should not spin on a blocked task.)

### task-runner prompt

```
You are a task-runner for ONE task in an autonomous implement-and-test workflow on Telegram
Desktop (C++ / Qt). You own this task end to end and isolate its context from the orchestrator.
You MAY and SHOULD spawn your own subagents (the Task tool is available to you).

PROJECT: <project>     TASK: <letter> — <title>
TASK DESCRIPTION:
<the task's full description block from implementing.md>
IMAGES: <referenced .ai/<project>/images/* paths, or none — Read them if present>
TASK_DIR: .ai/<project>/<letter>/
TASK_ID: <project>-<letter>

Config (paths relative to this checkout): BUILD=<...> EXE=<...> MAX_ATTEMPTS=<...>. The test account
is the out/Debug/ portable-data folders (see test-loop.md "Test account").

Read first: AGENTS.md; REVIEW.md; `.claude/commands/task.md` (for the exact Phase 1-6 prompt
templates); `.agents/shared/test-loop.md` (for the testing phase). For a follow-up letter, also read
`.ai/<project>/about.md` and the previous letter's `context.md`.

Run this pipeline for THIS task only, spawning a fresh subagent per phase (so each phase's output
stays in YOUR context, not the orchestrator's):

1. CONTEXT  — run task.md's Phase 1 (new) or Phase 1F (follow-up) prompt for this task; produces
   `<TASK_DIR>/context.md` (and `about.md` for the project).
2. PLAN     — task.md Phase 2 -> `<TASK_DIR>/plan.md`.
3. ASSESS   — task.md Phase 3 (refine plan, size phases).
4. IMPLEMENT— task.md Phase 4, one subagent per plan phase. Implementation agents do NOT commit
   yet; you commit after build passes.
5. BUILD    — task.md Phase 5 (build with BUILD, fix errors). On file-lock errors, run the
   path-scoped kill of THIS checkout's binary (see test-loop.md "Serialize app runs") and retry
   once, else stop.
6. REVIEW   — task.md Phase 6 but a SINGLE pass (not 3): one review agent, then one fix agent if
   NEEDS_CHANGES, then rebuild. (Tests catch behavior; review catches dead code / duplication /
   placement / style.)
7. COMMIT   — `git add -A && git commit` with a concise plain-language subject (≤ ~50-60 chars,
   matching recent `git log` style; usually the whole message — add a short plain body only if the
   subject can't carry it). NO `Autotask:`/attempt trailer and NO `Co-Authored-By:`/attribution line
   (this overrides the default; see test-loop.md "Commit message"). Commit submodules first if dirty,
   then bump the pointer. Record the commit SHA as IMPL_SHA (you track the attempt number yourself).
8. TEST     — run the loop in `.agents/shared/test-loop.md` to APPROVED, BLOCKED, or attempt cap.
   Spawn a test-author subagent and feed it BOTH sides per test-loop.md "Design the tests from THIS
   task": (1) the TASK SPEC — this task's full description block above PLUS its referenced IMAGES
   (tell it to READ the mockups; they show the intended result), and (2) the implementation —
   `git show <IMPL_SHA>` + touched files. It designs a falsifiable oracle per change and writes the
   plan into `<TASK_DIR>/test.md` BEFORE running (for visual/asset changes the oracle compares the
   tight crop against old vs intended-new art — judged VISUALLY, never by hash/byte; mobile mockups
   are not pixel targets), covers every surface the task names, and never reuses another task's
   navigate+screenshot. You drive RUN/ASSESS yourself, ADVERSARIALLY (no pass-by-inference; missing
   evidence = TEST_FLAW; no-difference-from-before = IMPL_BUG), and keep the human-readable
   `<TASK_DIR>/test.md` report. Spawn an impl-fix subagent on IMPL_BUG (it commits the next attempt →
   new IMPL_SHA). After each run, save the overlay patch into TASK_DIR and `git reset --hard
   <IMPL_SHA>` so the checkout returns to impl-only. Run the test-account SETUP steps before each
   launch and honor every test-account hard rule (serialize app runs; avoid destructive calls).

Skip TEST only if the task changed no runnable behavior (docs/config only) — say so explicitly.

When done, write nothing new to chat except the compact summary block from test-loop.md
("TASK/STATUS/VERDICT/ATTEMPTS/TOUCHED/DISCOVERED/NOTES"). All reasoning lives in `.ai/`.
```

## Completion

When the loop ends (all tasks approved/blocked, or a blocked task stopped it):
1. Summarize per task: approved vs blocked, attempts, files touched, key test evidence.
2. List any discovered tasks that were added.
3. Note the project name for `/implement <project> <follow-up>`.
4. Show total elapsed time (`Xh Ym Zs`, omit zero components).
5. Remind that test overlays are saved as `.ai/<project>/<letter>/test-overlay.patch` and the
   checkout is left at each task's implementation commit (overlays reset away).

## Error handling

- A `task-runner` returning BLOCKED stops the loop; report its reason and the `test.md` path.
- If `implementing.md` or any artifact is malformed, re-spawn that step with tighter instructions.
- Never proceed past a file-lock build error — ask the user to close `Telegram.exe`.
- The launch gate (Phase A) guarantees the test account exists before any work begins; if it is
  absent the command never starts.
