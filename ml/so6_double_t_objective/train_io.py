from __future__ import annotations

import json
import os
import resource
import signal
import sys
import time
from dataclasses import asdict
from pathlib import Path
from typing import Any, Optional, Sequence, Tuple

import torch


CURSOR_SHOW_ESCAPE = "\033[?25h"


def rss_gb() -> float:
    """Return the current process peak RSS in GiB on Linux."""
    return float(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss) / (1024.0 * 1024.0)


def log(msg: str) -> None:
    """Print a timestamped log line with the current RSS."""
    stamp = time.strftime("%H:%M:%S")
    print(f"[{stamp} rss={rss_gb():.2f}GiB] {msg}", flush=True)


def clear_progress_line() -> None:
    """Erase the terminal progress line emitted by render_epoch_progress."""
    sys.stdout.write("\r" + " " * 180 + "\r")
    sys.stdout.flush()


def restore_terminal_state() -> None:
    """Restore the interactive terminal state on process exit."""
    sys.stdout.flush()
    sys.stderr.flush()
    if sys.stdout.isatty():
        clear_progress_line()
        os.write(sys.stdout.fileno(), CURSOR_SHOW_ESCAPE.encode("ascii"))
    elif sys.stderr.isatty():
        os.write(sys.stderr.fileno(), CURSOR_SHOW_ESCAPE.encode("ascii"))


def install_exit_signal_handlers() -> list[Tuple[int, object]]:
    """Install signal handlers that restore the cursor before exiting."""
    previous_handlers: list[Tuple[int, object]] = []

    def _handle_exit_signal(signum: int, _frame: object) -> None:
        restore_terminal_state()
        raise SystemExit(128 + signum)

    for sig_name in ("SIGINT", "SIGTERM"):
        if not hasattr(signal, sig_name):
            continue
        sig = getattr(signal, sig_name)
        previous_handlers.append((sig, signal.getsignal(sig)))
        signal.signal(sig, _handle_exit_signal)
    return previous_handlers


def restore_exit_signal_handlers(previous_handlers: Sequence[Tuple[int, object]]) -> None:
    """Restore the signal handlers replaced by install_exit_signal_handlers."""
    for sig, handler in previous_handlers:
        signal.signal(sig, handler)


def finish_run_io(previous_signal_handlers: Sequence[Tuple[int, object]]) -> None:
    """Restore terminal and signal-handler state at process exit."""
    restore_terminal_state()
    restore_exit_signal_handlers(previous_signal_handlers)


def human_seconds(seconds: float) -> str:
    """Format a duration in a compact human-readable form."""
    if seconds < 60:
        return f"{seconds:.1f}s"
    minutes, sec = divmod(seconds, 60.0)
    if minutes < 60:
        return f"{int(minutes)}m{sec:04.1f}s"
    hours, minutes = divmod(minutes, 60.0)
    return f"{int(hours)}h{int(minutes):02d}m{sec:04.1f}s"


def _clone_state_dict_cpu(state_dict: dict[str, torch.Tensor]) -> dict[str, torch.Tensor]:
    """Clone a model state dict onto CPU tensors for checkpointing."""
    return {k: v.detach().cpu().clone() for k, v in state_dict.items()}


def save_training_checkpoint(
    path: Path,
    *,
    args: Any,
    epoch: int,
    proposal_model: torch.nn.Module,
    reranker_model: Optional[torch.nn.Module],
    optimizer: torch.optim.Optimizer,
    stage: Any,
    best: Optional[Any],
) -> None:
    """Persist a resumable training checkpoint to disk."""
    payload = {
        "epoch": int(epoch),
        "proposal_model": _clone_state_dict_cpu(proposal_model.state_dict()),
        "reranker_model": _clone_state_dict_cpu(reranker_model.state_dict()) if reranker_model is not None else None,
        "optimizer_state": optimizer.state_dict(),
        "stage": asdict(stage),
        "best": {
            "metrics": asdict(best.metrics) if best is not None and best.metrics is not None else None,
            "global_state": best.global_state if best is not None else None,
            "stage1_state": best.stage1_state if best is not None else None,
            "stage2_state": best.stage2_state if best is not None else None,
        },
        "args": vars(args),
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    torch.save(payload, path)


def load_training_checkpoint(path: Path, device: torch.device) -> dict:
    """Load a resumable training checkpoint."""
    return torch.load(path, map_location=device, weights_only=False)


def load_model_artifact(path: Path, device: torch.device) -> dict:
    """Load a saved model artifact or raw state dict."""
    return torch.load(path, map_location=device, weights_only=False)


def render_epoch_progress(epoch: int, epochs: int, stage_name: str, step: int, total_steps: int, elapsed_seconds: float) -> None:
    """Render a single-line terminal progress bar for training or evaluation."""
    width = 30
    frac = step / max(total_steps, 1)
    filled = int(round(frac * width))
    bar = "█" * filled + "·" * (width - filled)
    elapsed = human_seconds(elapsed_seconds)
    msg = f"epoch {epoch}/{epochs} [{bar}] {step}/{total_steps} stage={stage_name} elapsed={elapsed}"
    sys.stdout.write("\r" + msg[:180])
    sys.stdout.flush()
