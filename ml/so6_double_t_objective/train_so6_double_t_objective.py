#!/usr/bin/env python3
from __future__ import annotations

"""Minimal two-stage SO(6) double-T trainer.

Stage 1 learns a proposal distribution that concentrates probability mass on
any reducing action, where "reducing" means a state in LUT depth D becomes a
state in LUT depth D-2 after one double-T action.

Once stage 1 has gone 3 straight epochs without improving top-5 reducing
accuracy, restore its best proposal model and switch to stage 2.

Stage 2 freezes the proposal model, reranks its top 3 actions, and stops after
3 straight epochs without improving top-1 reducing accuracy.
"""

import argparse
import importlib
import json
import math
import os
import random
import secrets
import time
from dataclasses import dataclass
from itertools import count
from pathlib import Path
from typing import Optional

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from train_io import (
    clear_progress_line,
    finish_run_io,
    human_seconds,
    install_exit_signal_handlers,
    log,
    render_epoch_progress,
)


# -----------------------------------------------------------------------------
# User-editable settings
# -----------------------------------------------------------------------------

MAX_T_DEPTH = 12
THREADS = 0  # 0 means use all CPU threads
BATCH_SIZE = 256
STEPS_PER_EPOCH = 500
EVAL_BATCHES = 5
NON_IMPROVEMENT_LIMIT = 3
PROPOSAL_LR = 3e-4
RERANKER_LR = 6e-5
WEIGHT_DECAY = 1e-2
SAVE_DIR: Optional[str] = None
DEPTH_SAMPLING_ALPHA = 1.0


# -----------------------------------------------------------------------------
# Fixed problem sizes / model sizes
# -----------------------------------------------------------------------------

NUM_ACTIONS = 165
NUM_FEATURES = 108
RERANK_SHORTLIST = 3
LUT_PROGRESS_MODE = "bars"

PROPOSAL_D_MODEL = 128
PROPOSAL_NHEAD = 4
PROPOSAL_NUM_LAYERS = 4
PROPOSAL_DROPOUT = 0.1
RERANKER_HIDDEN = 192
RERANKER_MOVE_DIM = 32


@dataclass
class Batch:
    features: torch.Tensor
    reducing_mask: torch.Tensor
    states: Optional[np.ndarray] = None


@dataclass
class CandidateBatch:
    state_features: torch.Tensor
    next_features: torch.Tensor
    action_ids: torch.Tensor
    proposal_scores: torch.Tensor
    reducing_mask: torch.Tensor


@dataclass
class Metrics:
    top1_reducing: float
    top5_reducing: float


@dataclass
class EpochStats:
    loss: float
    metrics: Metrics
    elapsed_seconds: float


@dataclass
class BestSnapshot:
    score: float = float("-inf")
    metrics: Optional[Metrics] = None
    state: Optional[dict[str, dict[str, torch.Tensor]]] = None


@dataclass
class CoverageStats:
    total_size: int
    total_draws: int
    expected_unique_touched: float
    expected_fraction_touched: float
    per_depth: list[dict[str, float | int]]


@dataclass
class LayerEvalResult:
    depth: int
    samples: int
    metrics: Metrics


@dataclass(frozen=True)
class CliConfig:
    max_t_depth: int
    batch_size: int
    steps_per_epoch: int
    seed: int
    threads: int
    eval_batches: int
    save_dir: Optional[str]
    depth_sampling_alpha: float


class ProposalModel(nn.Module):
    def __init__(self) -> None:
        super().__init__()
        self.d_model = PROPOSAL_D_MODEL
        self.ic_sq_proj = nn.Linear(1, PROPOSAL_D_MODEL)
        self.dc_proj = nn.Linear(1, PROPOSAL_D_MODEL)
        self.pos_embed = nn.Embedding(NUM_FEATURES, PROPOSAL_D_MODEL)

        feature_scale = torch.empty(NUM_FEATURES, dtype=torch.float32)
        feature_scale[0::3] = 1.0 / 32768.0
        feature_scale[1::3] = 1.0 / 32768.0
        feature_scale[2::3] = 1.0 / 32.0
        self.register_buffer("feature_scale", feature_scale, persistent=False)

        ic_sq_idx = torch.tensor([i for i in range(NUM_FEATURES) if i % 3 != 2], dtype=torch.long)
        dc_idx = torch.tensor([i for i in range(2, NUM_FEATURES, 3)], dtype=torch.long)
        self.register_buffer("ic_sq_idx", ic_sq_idx, persistent=False)
        self.register_buffer("dc_idx", dc_idx, persistent=False)

        layer = nn.TransformerEncoderLayer(
            d_model=PROPOSAL_D_MODEL,
            nhead=PROPOSAL_NHEAD,
            dim_feedforward=4 * PROPOSAL_D_MODEL,
            dropout=PROPOSAL_DROPOUT,
            batch_first=True,
        )
        self.transformer = nn.TransformerEncoder(layer, num_layers=PROPOSAL_NUM_LAYERS)
        self.head = nn.Linear(PROPOSAL_D_MODEL, NUM_ACTIONS)
        self._init_weights()

    def _init_weights(self) -> None:
        nn.init.normal_(self.ic_sq_proj.weight, std=0.02)
        nn.init.zeros_(self.ic_sq_proj.bias)
        nn.init.normal_(self.dc_proj.weight, std=0.02)
        nn.init.zeros_(self.dc_proj.bias)
        nn.init.normal_(self.pos_embed.weight, std=0.02)
        nn.init.normal_(self.head.weight, std=0.02)
        nn.init.zeros_(self.head.bias)

    def forward(self, features: torch.Tensor) -> torch.Tensor:
        batch_size = features.shape[0]
        x = torch.empty(batch_size, NUM_FEATURES, self.d_model, device=features.device, dtype=torch.float32)

        features = features * self.feature_scale
        ic_sq_values = features.index_select(1, self.ic_sq_idx).unsqueeze(-1)
        dc_values = features.index_select(1, self.dc_idx).unsqueeze(-1)

        x.index_copy_(1, self.ic_sq_idx, self.ic_sq_proj(ic_sq_values))
        x.index_copy_(1, self.dc_idx, self.dc_proj(dc_values))

        positions = torch.arange(NUM_FEATURES, device=features.device).unsqueeze(0).expand(batch_size, -1)
        x = x + self.pos_embed(positions)
        x = self.transformer(x)
        return self.head(x.mean(dim=1))


class Reranker(nn.Module):
    def __init__(self) -> None:
        super().__init__()
        self.state_encoder = nn.Sequential(
            nn.Linear(NUM_FEATURES, RERANKER_HIDDEN),
            nn.GELU(),
            nn.Linear(RERANKER_HIDDEN, RERANKER_HIDDEN),
            nn.GELU(),
        )
        self.move_embed = nn.Embedding(NUM_ACTIONS, RERANKER_MOVE_DIM)
        self.score_head = nn.Sequential(
            nn.Linear(2 * RERANKER_HIDDEN + RERANKER_MOVE_DIM + 1, RERANKER_HIDDEN),
            nn.GELU(),
            nn.Linear(RERANKER_HIDDEN, 1),
        )
        self._init_weights()

    def _init_weights(self) -> None:
        for module in self.modules():
            if isinstance(module, nn.Linear):
                nn.init.normal_(module.weight, std=0.02)
                nn.init.zeros_(module.bias)
            elif isinstance(module, nn.Embedding):
                nn.init.normal_(module.weight, std=0.02)

        final_linear = self.score_head[-1]
        nn.init.zeros_(final_linear.weight)
        nn.init.zeros_(final_linear.bias)

    def forward(
        self,
        state_features: torch.Tensor,
        next_features: torch.Tensor,
        action_ids: torch.Tensor,
        proposal_scores: torch.Tensor,
    ) -> torch.Tensor:
        batch_size, k = action_ids.shape
        state_embed = self.state_encoder(state_features).unsqueeze(1).expand(-1, k, -1)
        next_embed = self.state_encoder(next_features.reshape(batch_size * k, NUM_FEATURES)).reshape(batch_size, k, -1)
        action_embed = self.move_embed(action_ids)
        x = torch.cat([state_embed, next_embed, action_embed, proposal_scores.unsqueeze(-1)], dim=-1)
        delta = self.score_head(x).squeeze(-1)
        return proposal_scores + delta


class ReadOnlyLUT:
    def __init__(self, max_t_depth: int, threads: int, cached_depths: list[int]) -> None:
        log("importing LUT module 'readonly_lut'")
        module = importlib.import_module("readonly_lut")
        t0 = time.time()
        self.lut = module.ReadOnlyLUT(
            max_t_depth=max_t_depth,
            threads=threads,
            progress_mode=LUT_PROGRESS_MODE,
            verbose_build=False,
            debug=False,
            log_calls=False,
            cached_depths=cached_depths,
        )
        log(f"ReadOnlyLUT ready in {human_seconds(time.time() - t0)}")

    def layer_size(self, depth: int) -> int:
        return int(self.lut.layer_size(int(depth)))

    def sample_without_states(self, depth: int, n: int, seed: int) -> tuple[np.ndarray, np.ndarray]:
        features_obj, mask_obj = self.lut.sample_feature_mask_at_t_depth(int(depth), int(n), int(seed))
        return np.asarray(features_obj, dtype=np.float32), np.asarray(mask_obj, dtype=np.bool_)

    def sample_with_states(self, depth: int, n: int, seed: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        features_obj, states_obj, mask_obj = self.lut.sample_labeled_features_at_t_depth(
            int(depth), int(n), int(seed)
        )
        return (
            np.asarray(features_obj, dtype=np.float32),
            np.asarray(states_obj),
            np.asarray(mask_obj, dtype=np.bool_),
        )

    def apply_actions(self, states: np.ndarray, action_ids: np.ndarray) -> np.ndarray:
        out = self.lut.apply_tt_features_batch(
            np.asarray(states),
            np.asarray(action_ids, dtype=np.int64),
        )
        return np.asarray(out, dtype=np.float32)


class Sampler:
    def __init__(self, lut: ReadOnlyLUT, depths: list[int], seed: int, depth_sampling_alpha: float = 1.0) -> None:
        self.lut = lut
        self.depths = depths
        self.rng = np.random.default_rng(seed)
        self.layer_sizes = {depth: max(self.lut.layer_size(depth), 0) for depth in depths}
        self.draw_counts = {depth: 0 for depth in depths}
        self.depth_sampling_alpha = depth_sampling_alpha

        sizes = np.array([self.layer_sizes[depth] for depth in depths], dtype=np.float64)
        if float(sizes.sum()) <= 0:
            raise ValueError(f"all requested LUT layers are empty: {depths}")
        tempered = np.power(sizes, self.depth_sampling_alpha, dtype=np.float64)
        if float(tempered.sum()) <= 0:
            raise ValueError(f"invalid tempered depth weights for alpha={self.depth_sampling_alpha}: {depths}")
        self.depth_probs = tempered / tempered.sum()

    def sample(self, batch_size: int, with_states: bool) -> Batch:
        chosen_depths = self.rng.choice(self.depths, size=batch_size, replace=True, p=self.depth_probs)
        features = np.empty((batch_size, NUM_FEATURES), dtype=np.float32)
        reducing_mask = np.zeros((batch_size, NUM_ACTIONS), dtype=np.bool_)
        states = np.empty((batch_size, 6, 6), dtype=np.uint64) if with_states else None

        for depth in sorted(set(int(d) for d in chosen_depths.tolist())):
            rows = np.where(chosen_depths == depth)[0]
            self.draw_counts[depth] += int(len(rows))
            seed = int(self.rng.integers(0, 2**31 - 1))
            if with_states:
                batch_features, batch_states, batch_mask = self.lut.sample_with_states(depth, len(rows), seed)
                states[rows] = batch_states
            else:
                batch_features, batch_mask = self.lut.sample_without_states(depth, len(rows), seed)
            features[rows] = batch_features
            reducing_mask[rows] = batch_mask

        return Batch(
            features=torch.from_numpy(features),
            reducing_mask=torch.from_numpy(reducing_mask),
            states=states,
        )

    def coverage_stats(self) -> CoverageStats:
        total_size = int(sum(self.layer_sizes.values()))
        total_draws = int(sum(self.draw_counts.values()))
        per_depth: list[dict[str, float | int]] = []
        expected_unique_total = 0.0

        for depth in self.depths:
            layer_size = self.layer_sizes[depth]
            draws = self.draw_counts[depth]
            if layer_size <= 0:
                expected_unique = 0.0
            elif draws <= 0:
                expected_unique = 0.0
            else:
                expected_unique = float(layer_size) * (1.0 - math.exp(draws * math.log1p(-1.0 / float(layer_size))))
            expected_fraction = (expected_unique / float(layer_size)) if layer_size > 0 else 0.0
            expected_unique_total += expected_unique
            per_depth.append(
                {
                    "depth": depth,
                    "layer_size": layer_size,
                    "draws": draws,
                    "expected_unique_touched": expected_unique,
                    "expected_fraction_touched": expected_fraction,
                }
            )

        expected_fraction_total = (expected_unique_total / float(total_size)) if total_size > 0 else 0.0
        return CoverageStats(
            total_size=total_size,
            total_draws=total_draws,
            expected_unique_touched=expected_unique_total,
            expected_fraction_touched=expected_fraction_total,
            per_depth=per_depth,
        )


def set_seed(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed % (2**32))
    torch.manual_seed(seed)
    if torch.cuda.is_available():
        torch.cuda.manual_seed_all(seed)


def parse_args() -> CliConfig:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--max-t-depth", type=int, default=MAX_T_DEPTH, help="Maximum even LUT depth used for training")
    parser.add_argument("--batch-size", type=int, default=BATCH_SIZE, help="Batch size for train and probe evaluation")
    parser.add_argument("--steps-per-epoch", type=int, default=STEPS_PER_EPOCH, help="Optimizer steps per epoch")
    parser.add_argument("--seed", type=int, default=None, help="Override the random seed; default is system-random")
    parser.add_argument("--threads", type=int, default=THREADS, help="CPU thread count for LUT construction; 0 uses all CPUs")
    parser.add_argument("--eval-batches", type=int, default=EVAL_BATCHES, help="Number of fixed probe batches per evaluation")
    parser.add_argument("--save-dir", type=str, default=SAVE_DIR, help="Optional output directory for model and metrics")
    parser.add_argument(
        "--depth-sampling-alpha",
        type=float,
        default=DEPTH_SAMPLING_ALPHA,
        help="Training depth-sampling exponent. Uses p(depth) proportional to layer_size(depth)^alpha",
    )
    args = parser.parse_args()
    if args.max_t_depth < 2 or args.max_t_depth % 2 != 0:
        parser.error("--max-t-depth must be an even integer >= 2")
    if args.batch_size <= 0:
        parser.error("--batch-size must be positive")
    if args.steps_per_epoch <= 0:
        parser.error("--steps-per-epoch must be positive")
    if args.eval_batches <= 0:
        parser.error("--eval-batches must be positive")
    if args.threads < 0:
        parser.error("--threads must be >= 0")
    if args.seed is not None and args.seed < 0:
        parser.error("--seed must be nonnegative")
    if args.depth_sampling_alpha < 0.0:
        parser.error("--depth-sampling-alpha must be nonnegative")
    return CliConfig(
        max_t_depth=int(args.max_t_depth),
        batch_size=int(args.batch_size),
        steps_per_epoch=int(args.steps_per_epoch),
        seed=int(args.seed) if args.seed is not None else secrets.randbelow(2**32),
        threads=int(args.threads),
        eval_batches=int(args.eval_batches),
        save_dir=args.save_dir,
        depth_sampling_alpha=float(args.depth_sampling_alpha),
    )


def cpu_state_dict(model: nn.Module) -> dict[str, torch.Tensor]:
    return {name: value.detach().cpu().clone() for name, value in model.state_dict().items()}


def snapshot(proposal_model: ProposalModel, reranker: Optional[Reranker] = None) -> dict[str, dict[str, torch.Tensor]]:
    state = {"proposal_model": cpu_state_dict(proposal_model)}
    if reranker is not None:
        state["reranker_model"] = cpu_state_dict(reranker)
    return state


def proposal_loss(logits: torch.Tensor, reducing_mask: torch.Tensor) -> torch.Tensor:
    log_probs = F.log_softmax(logits, dim=-1)
    selected = torch.where(reducing_mask, log_probs, torch.full_like(log_probs, float("-inf")))
    return -torch.logsumexp(selected, dim=-1).mean()


def reranker_loss(scores: torch.Tensor, reducing_mask: torch.Tensor) -> torch.Tensor:
    bce = F.binary_cross_entropy_with_logits(scores, reducing_mask.float())

    pairwise_terms = []
    for row_scores, row_mask in zip(scores, reducing_mask):
        pos = row_scores[row_mask]
        neg = row_scores[~row_mask]
        if pos.numel() == 0 or neg.numel() == 0:
            continue
        pairwise_terms.append(F.softplus(neg.unsqueeze(0) - pos.unsqueeze(1)).mean())

    pairwise = torch.stack(pairwise_terms).mean() if pairwise_terms else scores.new_tensor(0.0)
    return bce + pairwise


def build_candidates(batch: Batch, proposal_logits: torch.Tensor, lut: ReadOnlyLUT) -> CandidateBatch:
    if batch.states is None:
        raise ValueError("stage 2 needs cached states")

    k = min(RERANK_SHORTLIST, proposal_logits.shape[1])
    topk = torch.topk(proposal_logits, k=k, dim=-1)
    action_ids = topk.indices.detach().cpu().to(torch.long)
    proposal_scores = topk.values.detach().cpu().to(torch.float32)

    action_ids_np = action_ids.numpy().astype(np.int64, copy=False)
    next_features_np = lut.apply_actions(batch.states, action_ids_np)
    next_features = torch.from_numpy(next_features_np.reshape(action_ids.shape[0], action_ids.shape[1], NUM_FEATURES))

    reducing_np = np.take_along_axis(batch.reducing_mask.numpy().astype(np.bool_, copy=False), action_ids_np, axis=1)
    reducing_mask = torch.from_numpy(reducing_np)

    return CandidateBatch(
        state_features=batch.features,
        next_features=next_features,
        action_ids=action_ids,
        proposal_scores=proposal_scores,
        reducing_mask=reducing_mask,
    )


def merged_action_order(proposal_logits: torch.Tensor, action_ids: torch.Tensor, rerank_scores: torch.Tensor) -> torch.Tensor:
    proposal_order = torch.argsort(proposal_logits, dim=-1, descending=True)
    local_order = torch.argsort(rerank_scores, dim=-1, descending=True)
    reranked_shortlist = torch.gather(action_ids, 1, local_order)

    rows = []
    for shortlist, full_order in zip(reranked_shortlist.tolist(), proposal_order.tolist()):
        chosen = set(shortlist)
        rows.append(shortlist + [action for action in full_order if action not in chosen])
    return torch.tensor(rows, dtype=torch.long)


def reducing_metrics_from_proposal_logits(proposal_logits: torch.Tensor, reducing_mask: torch.Tensor) -> Metrics:
    top1 = proposal_logits.argmax(dim=-1)
    row_ids = torch.arange(reducing_mask.shape[0], device=proposal_logits.device)
    hit1 = reducing_mask[row_ids, top1].float().mean().item()
    top5 = proposal_logits.topk(k=min(5, proposal_logits.shape[1]), dim=-1).indices
    hit5 = reducing_mask.gather(1, top5).any(dim=1).float().mean().item()
    return Metrics(top1_reducing=float(hit1), top5_reducing=float(hit5))


def reducing_metrics_from_reranker(
    proposal_logits: torch.Tensor,
    rerank_scores: torch.Tensor,
    action_ids: torch.Tensor,
    reducing_mask: torch.Tensor,
) -> Metrics:
    order = merged_action_order(proposal_logits.cpu(), action_ids, rerank_scores.cpu())
    ordered_mask = reducing_mask.gather(1, order)
    hit1 = ordered_mask[:, :1].any(dim=1).float().mean().item()
    hit5 = ordered_mask[:, :5].any(dim=1).float().mean().item()
    return Metrics(top1_reducing=float(hit1), top5_reducing=float(hit5))


def train_epoch(
    stage: str,
    proposal_model: ProposalModel,
    reranker: Optional[Reranker],
    optimizer: torch.optim.Optimizer,
    sampler: Sampler,
    device: torch.device,
    batch_size: int,
    steps_per_epoch: int,
) -> EpochStats:
    if stage == "stage1":
        proposal_model.train()
    else:
        if reranker is None:
            raise ValueError("stage2 training requires a reranker")
        proposal_model.eval()
        reranker.train()

    losses = []
    top1_hits = []
    top5_hits = []
    t0 = time.time()
    last_progress = 0.0

    for step in range(1, steps_per_epoch + 1):
        batch = sampler.sample(batch_size, with_states=(stage == "stage2"))
        features = batch.features.to(device)
        optimizer.zero_grad(set_to_none=True)

        if stage == "stage1":
            proposal_logits = proposal_model(features)
            reducing_mask = batch.reducing_mask.to(device)
            loss = proposal_loss(proposal_logits, reducing_mask)
            batch_metrics = reducing_metrics_from_proposal_logits(proposal_logits, reducing_mask)
            trainable_params = proposal_model.parameters()
        else:
            with torch.no_grad():
                proposal_logits = proposal_model(features)
            candidates = build_candidates(batch, proposal_logits, sampler.lut)
            scores = reranker(
                candidates.state_features.to(device),
                candidates.next_features.to(device),
                candidates.action_ids.to(device),
                candidates.proposal_scores.to(device),
            )
            loss = reranker_loss(scores, candidates.reducing_mask.to(device))
            batch_metrics = reducing_metrics_from_reranker(
                proposal_logits=proposal_logits.detach(),
                rerank_scores=scores.detach(),
                action_ids=candidates.action_ids,
                reducing_mask=batch.reducing_mask,
            )
            trainable_params = reranker.parameters()

        loss.backward()
        torch.nn.utils.clip_grad_norm_(trainable_params, 1.0)
        optimizer.step()

        losses.append(float(loss.item()))
        top1_hits.append(batch_metrics.top1_reducing)
        top5_hits.append(batch_metrics.top5_reducing)
        now = time.time()
        if step == 1 or step == steps_per_epoch or (now - last_progress) >= 1.0:
            render_epoch_progress(0, 0, stage, step, steps_per_epoch, now - t0)
            last_progress = now

    clear_progress_line()
    return EpochStats(
        loss=float(np.mean(losses)),
        metrics=Metrics(
            top1_reducing=float(np.mean(top1_hits)),
            top5_reducing=float(np.mean(top5_hits)),
        ),
        elapsed_seconds=time.time() - t0,
    )


def build_fixed_probe(sampler: Sampler, num_batches: int, batch_size: int) -> list[Batch]:
    return [sampler.sample(batch_size, with_states=True) for _ in range(num_batches)]


def build_fixed_depth_probe(
    lut: ReadOnlyLUT,
    depth: int,
    num_batches: int,
    batch_size: int,
    seed: int,
) -> list[Batch]:
    rng = np.random.default_rng(seed)
    batches = []
    for _ in range(num_batches):
        batch_seed = int(rng.integers(0, 2**31 - 1))
        features_np, states_np, mask_np = lut.sample_with_states(depth, batch_size, batch_seed)
        batches.append(
            Batch(
                features=torch.from_numpy(features_np),
                reducing_mask=torch.from_numpy(mask_np),
                states=states_np,
            )
        )
    return batches


@torch.no_grad()
def evaluate_batches(
    stage: str,
    proposal_model: ProposalModel,
    reranker: Optional[Reranker],
    lut: ReadOnlyLUT,
    batches: list[Batch],
    device: torch.device,
    progress_label: str,
) -> Metrics:
    proposal_model.eval()
    if stage == "stage2" and reranker is None:
        raise ValueError("stage2 evaluation requires a reranker")
    if reranker is not None:
        reranker.eval()

    top1_hits = []
    top5_hits = []
    t0 = time.time()

    for batch_idx, batch in enumerate(batches, start=1):
        features = batch.features.to(device)
        proposal_logits = proposal_model(features)

        if stage == "stage1":
            batch_metrics = reducing_metrics_from_proposal_logits(proposal_logits, batch.reducing_mask.to(device))
        else:
            candidates = build_candidates(batch, proposal_logits, lut)
            rerank_scores = reranker(
                candidates.state_features.to(device),
                candidates.next_features.to(device),
                candidates.action_ids.to(device),
                candidates.proposal_scores.to(device),
            )
            batch_metrics = reducing_metrics_from_reranker(
                proposal_logits=proposal_logits,
                rerank_scores=rerank_scores,
                action_ids=candidates.action_ids,
                reducing_mask=batch.reducing_mask,
            )

        top1_hits.append(batch_metrics.top1_reducing)
        top5_hits.append(batch_metrics.top5_reducing)

        if batch_idx == len(batches) or batch_idx % 20 == 0:
            render_epoch_progress(0, 0, progress_label, batch_idx, len(batches), time.time() - t0)

    clear_progress_line()
    return Metrics(top1_reducing=float(np.mean(top1_hits)), top5_reducing=float(np.mean(top5_hits)))


def start_stage2(proposal_model: ProposalModel, device: torch.device) -> tuple[Reranker, torch.optim.Optimizer]:
    for param in proposal_model.parameters():
        param.requires_grad_(False)
    proposal_model.eval()

    reranker = Reranker().to(device)
    optimizer = torch.optim.AdamW(reranker.parameters(), lr=RERANKER_LR, weight_decay=WEIGHT_DECAY)
    return reranker, optimizer


def format_coverage_stats(stats: CoverageStats) -> list[str]:
    lines = [
        "training data coverage (expected unique states touched under with-replacement sampling)",
        f"overall | expected_unique={stats.expected_unique_touched:.1f}/{stats.total_size} "
        f"| coverage={100.0 * stats.expected_fraction_touched:.4f}% "
        f"| total_draws={stats.total_draws}",
    ]
    for row in stats.per_depth:
        lines.append(
            f"depth={row['depth']} | expected_unique={row['expected_unique_touched']:.1f}/{row['layer_size']} "
            f"| coverage={100.0 * float(row['expected_fraction_touched']):.4f}% "
            f"| draws={row['draws']}"
        )
    return lines


def format_layer_eval_results(results: list[LayerEvalResult]) -> list[str]:
    lines = ["final per-depth accuracy on fresh random samples"]
    for result in results:
        lines.append(
            f"depth={result.depth} | samples={result.samples} "
            f"| top1_reducing={result.metrics.top1_reducing:.4f} "
            f"| top5_reducing={result.metrics.top5_reducing:.4f}"
        )
    return lines


def save_artifacts(
    save_dir: Path,
    state: dict[str, dict[str, torch.Tensor]],
    metrics: Metrics,
    stage_name: str,
    coverage: CoverageStats,
    per_depth_results: list[LayerEvalResult],
    config: CliConfig,
) -> None:
    save_dir.mkdir(parents=True, exist_ok=True)
    torch.save({"mode": stage_name, **state}, save_dir / "model.pt")

    with open(save_dir / "metrics.json", "w", encoding="utf-8") as f:
        json.dump(
            {
                "best_stage": stage_name,
                "top1_reducing": metrics.top1_reducing,
                "top5_reducing": metrics.top5_reducing,
                "training_coverage": {
                    "total_size": coverage.total_size,
                    "total_draws": coverage.total_draws,
                    "expected_unique_touched": coverage.expected_unique_touched,
                    "expected_fraction_touched": coverage.expected_fraction_touched,
                    "per_depth": coverage.per_depth,
                },
                "final_per_depth_accuracy": [
                    {
                        "depth": result.depth,
                        "samples": result.samples,
                        "top1_reducing": result.metrics.top1_reducing,
                        "top5_reducing": result.metrics.top5_reducing,
                    }
                    for result in per_depth_results
                ],
            },
            f,
            indent=2,
        )

    with open(save_dir / "settings.json", "w", encoding="utf-8") as f:
        json.dump(
            {
                "MAX_T_DEPTH": config.max_t_depth,
                "THREADS": config.threads,
                "SEED": config.seed,
                "BATCH_SIZE": config.batch_size,
                "STEPS_PER_EPOCH": config.steps_per_epoch,
                "EVAL_BATCHES": config.eval_batches,
                "NON_IMPROVEMENT_LIMIT": NON_IMPROVEMENT_LIMIT,
                "PROPOSAL_LR": PROPOSAL_LR,
                "RERANKER_LR": RERANKER_LR,
                "WEIGHT_DECAY": WEIGHT_DECAY,
                "RERANK_SHORTLIST": RERANK_SHORTLIST,
                "SAVE_DIR": str(save_dir),
                "DEPTH_SAMPLING_ALPHA": config.depth_sampling_alpha,
            },
            f,
            indent=2,
        )

    log(f"saved artifacts to {save_dir}")


def main() -> None:
    previous_signal_handlers = install_exit_signal_handlers()
    try:
        config = parse_args()
        set_seed(config.seed)
        device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        resolved_threads = config.threads if config.threads > 0 else max(1, os.cpu_count() or 1)
        depths = list(range(2, config.max_t_depth + 1, 2))

        log(f"max_t_depth : {config.max_t_depth}")
        log(f"batch_size  : {config.batch_size}")
        log(f"steps/epoch : {config.steps_per_epoch}")
        log(f"seed        : {config.seed}")
        log(f"threads     : {resolved_threads}")
        log(f"eval_batches: {config.eval_batches}")
        log(f"save_dir    : {config.save_dir}")
        log(f"train depth sampling alpha : {config.depth_sampling_alpha:.4f}")
        log("building / attaching exact LUT")

        lut = ReadOnlyLUT(config.max_t_depth, resolved_threads, depths)
        train_sampler = Sampler(lut, depths, seed=config.seed, depth_sampling_alpha=config.depth_sampling_alpha)
        train_probe_sampler = Sampler(lut, depths, seed=config.seed + 1)
        probe_sampler = Sampler(lut, depths, seed=config.seed + 2)
        train_probe_batches = build_fixed_probe(train_probe_sampler, config.eval_batches, config.batch_size)
        probe_batches = build_fixed_probe(probe_sampler, config.eval_batches, config.batch_size)
        log(f"built fixed train probe with {len(train_probe_batches) * config.batch_size} states")
        log(f"built fixed eval probe with {len(probe_batches) * config.batch_size} states")

        proposal_model = ProposalModel().to(device)
        reranker: Optional[Reranker] = None
        optimizer = torch.optim.AdamW(proposal_model.parameters(), lr=PROPOSAL_LR, weight_decay=WEIGHT_DECAY)

        stage = "stage1"
        epochs_without_improvement = 0
        best_stage1 = BestSnapshot()
        best_stage2 = BestSnapshot()
        train_start = time.time()

        for epoch in count(1):
            lr = PROPOSAL_LR if stage == "stage1" else RERANKER_LR
            log(f"epoch {epoch} start | stage={stage} | lr={lr:.6g}")

            train_stats = train_epoch(
                stage=stage,
                proposal_model=proposal_model,
                reranker=reranker,
                optimizer=optimizer,
                sampler=train_sampler,
                device=device,
                batch_size=config.batch_size,
                steps_per_epoch=config.steps_per_epoch,
            )
            train_probe_metrics = evaluate_batches(
                stage=stage,
                proposal_model=proposal_model,
                reranker=reranker,
                lut=lut,
                batches=train_probe_batches,
                device=device,
                progress_label="train_probe",
            )
            probe_metrics = evaluate_batches(
                stage=stage,
                proposal_model=proposal_model,
                reranker=reranker,
                lut=lut,
                batches=probe_batches,
                device=device,
                progress_label="probe",
            )

            if stage == "stage1":
                stage_score = probe_metrics.top5_reducing
                improved = stage_score > best_stage1.score
                if improved:
                    best_stage1 = BestSnapshot(
                        score=stage_score,
                        metrics=probe_metrics,
                        state={"proposal_model": cpu_state_dict(proposal_model)},
                    )
                    log("new best stage1 proposal (by fixed probe top5_reducing)")
            else:
                stage_score = probe_metrics.top1_reducing
                improved = stage_score > best_stage2.score
                if improved:
                    best_stage2 = BestSnapshot(
                        score=stage_score,
                        metrics=probe_metrics,
                        state=snapshot(proposal_model, reranker),
                    )
                    log("new best stage2 reranker (by fixed probe top1_reducing)")

            epochs_without_improvement = 0 if improved else epochs_without_improvement + 1
            monitor_name = "top5_reducing" if stage == "stage1" else "top1_reducing"
            train_gap_top1 = train_stats.metrics.top1_reducing - probe_metrics.top1_reducing
            train_gap_top5 = train_stats.metrics.top5_reducing - probe_metrics.top5_reducing

            log(
                f"epoch {epoch} complete | stage={stage} | train_loss={train_stats.loss:.4f} "
                f"| train_top1={train_stats.metrics.top1_reducing:.4f} | train_top5={train_stats.metrics.top5_reducing:.4f} "
                f"| train_probe_top1={train_probe_metrics.top1_reducing:.4f} | train_probe_top5={train_probe_metrics.top5_reducing:.4f} "
                f"| probe_top1={probe_metrics.top1_reducing:.4f} | probe_top5={probe_metrics.top5_reducing:.4f} "
                f"| gap_top1={train_gap_top1:+.4f} | gap_top5={train_gap_top5:+.4f} "
                f"| {monitor_name}={stage_score:.4f} | non_improvements={epochs_without_improvement}/{NON_IMPROVEMENT_LIMIT} "
                f"| epoch_time={human_seconds(train_stats.elapsed_seconds)}"
            )

            if epochs_without_improvement < NON_IMPROVEMENT_LIMIT:
                continue

            if stage == "stage1":
                if best_stage1.state is None:
                    raise RuntimeError("stage1 ended without a best proposal checkpoint")
                proposal_model.load_state_dict(best_stage1.state["proposal_model"])
                reranker, optimizer = start_stage2(proposal_model, device)
                stage = "stage2"
                epochs_without_improvement = 0
                log(
                    f"stage1 reached {NON_IMPROVEMENT_LIMIT} non-improvements; "
                    "restored best proposal and switched to stage2"
                )
                continue

            log(f"stage2 reached {NON_IMPROVEMENT_LIMIT} non-improvements; stopping")
            break

        if best_stage1.metrics is None or best_stage1.state is None:
            raise RuntimeError("training finished without a best stage1 checkpoint")
        if best_stage2.metrics is None or best_stage2.state is None:
            raise RuntimeError("training finished without a best stage2 checkpoint")

        best_stage_name = "stage2"
        best_metrics = best_stage2.metrics
        best_state = best_stage2.state
        if best_stage1.metrics.top1_reducing > best_stage2.metrics.top1_reducing:
            best_stage_name = "stage1"
            best_metrics = best_stage1.metrics
            best_state = best_stage1.state

        proposal_model.load_state_dict(best_state["proposal_model"])
        if "reranker_model" in best_state:
            if reranker is None:
                reranker, _ = start_stage2(proposal_model, device)
            reranker.load_state_dict(best_state["reranker_model"])
        else:
            reranker = None

        log(f"training finished in {human_seconds(time.time() - train_start)}")
        log(
            "best metrics | "
            f"stage={best_stage_name} | top1_reducing={best_metrics.top1_reducing:.4f} "
            f"| top5_reducing={best_metrics.top5_reducing:.4f}"
        )

        coverage = train_sampler.coverage_stats()
        for line in format_coverage_stats(coverage):
            log(line)

        per_depth_results = []
        for depth in depths:
            depth_batches = build_fixed_depth_probe(
                lut=lut,
                depth=depth,
                num_batches=config.eval_batches,
                batch_size=config.batch_size,
                seed=config.seed + 1000 + depth,
            )
            depth_metrics = evaluate_batches(
                stage=best_stage_name,
                proposal_model=proposal_model,
                reranker=reranker,
                lut=lut,
                batches=depth_batches,
                device=device,
                progress_label=f"final_d{depth}",
            )
            per_depth_results.append(
                LayerEvalResult(
                    depth=depth,
                    samples=len(depth_batches) * config.batch_size,
                    metrics=depth_metrics,
                )
            )
        for line in format_layer_eval_results(per_depth_results):
            log(line)

        if config.save_dir is not None:
            save_artifacts(Path(config.save_dir), best_state, best_metrics, best_stage_name, coverage, per_depth_results, config)
    finally:
        finish_run_io(previous_signal_handlers)


if __name__ == "__main__":
    main()
