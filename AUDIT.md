# Audit: how the student's current code aligns with this objective

This audit is about the objective:

> On input an SO(6) matrix `S` at even single-T depth `D`, predict a double-T action that sends `S` to exact LUT depth `D-2`.

## Bottom line

The student's current double-T code does **not** directly optimize this objective.

It is not just a small mismatch.  The training target is a different mathematical object.

## Main misalignments

### 1. The current code supervises against one cached witness path

In the student's `so6_synthesis_task_v1.py`, `correct_next_vids()` returns only one next action: the next action on one cached gold path.

That means the current training loop treats

- the cached action as correct, and
- other equally good depth-reducing actions as wrong,

unless search later rediscovers them by accident.

For your desired objective, that is incorrect.

### 2. The current v1 data are tied to one fixed shell

The current v1 setup samples from one precomputed LUT layer (`opt_paths_layer=5` in the checked-in config), rather than uniformly from random even depths up to 12.

So the student code is not studying the local one-step objective on random even-depth states in the sense you asked for.

### 3. The current success criterion is different

The student's v1 path evaluation checks whether the final residual is Clifford / `LDE = 0`, not whether the *next* action reduces exact single-T depth by 2.

That is a different objective.

### 4. The structured trainer is path-centric

The structured trainer can sometimes accept off-gold successful rollouts, but its warm-up and its notion of a “correct next action” are still anchored to one cached witness path.

So even there, the basic supervision target is not the set of all exact depth-reducing double-T actions.

## What the replacement code changes

The replacement code in this directory changes the target to the mathematically correct set-valued one:

- sample a random exact LUT state at even depth `D <= 12`
- compute every double-T action that lands at exact depth `D-2`
- train with the set-valued loss

\[
-\log \sum_{a \in G(S)} p(a \mid S)
\]

where `G(S)` is the full set of good actions.

That is the correct local objective for your question.

## What I could and could not evaluate here

I could evaluate the *alignment* of the student's code with this objective by reading the code.

I could **not** produce a reliable numerical benchmark of her code on this exact objective in this environment, because that would require

- your compiled C++ LUT backend,
- a depth-12 LUT generated from it, and
- either modifying her code to call that backend or reproducing her original training run with that exact oracle.

So the judgment here is:

- **code-level alignment:** poor
- **numerical performance on the corrected objective:** not measured here

If you build the pybind bridge and run the training script, you will then have the right benchmark to compare against her current setup.
