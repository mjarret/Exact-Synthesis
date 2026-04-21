# SO6 double-T objective with automatic two-stage training

This package keeps the same exact oracle / depth-10 training setup, but the training loop now **automatically switches**
from:

1. **Stage 1**: set-mass loss  
   `L = -log(sum_{a in G} p(a))`

to

2. **Stage 2**: max-valid loss  
   `L = -log(max_{a in G} p(a))`

The switch is automatic and based on **convergence of `top5_valid`**.

## Automatic switch rule

After each epoch in stage 1:

- if `top5_valid` improves by more than `stage1_top5_min_delta`, reset the patience counter
- otherwise increment the patience counter

When the patience counter reaches `stage1_top5_patience`, the script:

- restores the **best stage-1 checkpoint**
- lowers the learning rate by `stage2_lr_scale`
- switches to stage-2 loss automatically

## Default settings

- `stage1_top5_patience = 3`
- `stage1_top5_min_delta = 1e-3`
- `stage2_lr_scale = 0.2`
- stage 1 loss = `set_mass`
- stage 2 loss = `max_valid`

## Build

```bash
cd ~/Workspace/Exact-Synthesis/ml/so6_double_t_objective
source ~/Workspace/Exact-Synthesis/.venv-ml/bin/activate

export EXACT_SYNTHESIS_DIR=~/Workspace/Exact-Synthesis
unset READONLY_LUT_DEBUG READONLY_LUT_ASAN READONLY_LUT_UBSAN READONLY_LUT_DISABLE_INDICATORS

rm -rf build
rm -f readonly_lut*.so

python setup.py build_ext --inplace
```

## Run

```bash
PYTHONFAULTHANDLER=1 python train_so6_double_t_objective.py   --phase train   --max-t-depth 10   --smoke-depth 10   --skip-smoke   --threads 0   --lut-progress off   --cached-depths 2,4,6,8,10   --epochs 30   --steps-per-epoch 500   --eval-batches 100   --batch-size 512   --fault-log runs/fault.log   --traceback-every 600 |& tee runs/depth10_twostage.log
```

## What to watch

- During stage 1, watch `top5_valid` approach saturation.
- When it plateaus, the script will log a message saying it is switching to stage 2.
- In stage 2, the main target is `top1_valid`.


Updated variant: minimal terminal output with in-place epoch progress, eval rank histograms, stage-2 default hybrid_rank loss, and stage-1 to stage-2 switch gated on top5_valid reaching a target then plateauing.
