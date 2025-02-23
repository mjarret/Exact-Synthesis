#!/bin/sh

## Give your job a name to distinguish it from other jobs you run.
#SBATCH --job-name=qc_compile

## General partitions: all-HiPri, bigmem-HiPri   --   (12 hour limit)
##                     all-LoPri, bigmem-LoPri, gpuq  (5 days limit)
## Restricted: CDS_q, CS_q, STATS_q, HH_q, GA_q, ES_q, COS_q  (10 day limit)
##SBATCH --partition=bigmem
#SBATCH --partition=contrib

## Separate output and error messages into 2 files.`
## NOTE: %u=userID, %x=jobName, %N=nodeID, %j=jobID, %A=arrayID, %a=arrayTaskID`
#SBATCH --output=/scratch/mjarretb/compile/output/%j.out
#SBATCH --error=/scratch/mjarretb/compile/output/%j.err

## Slurm can send you updates via email
#SBATCH --mail-type=FAIL         # ALL,NONE,BEGIN,END,FAIL,REQUEUE,..
#SBATCH --mail-user=mjarretb@gmu.edu     # Put your GMU email address here

## Specify how much memory your job needs. (2G is the default)
##SBATCH --mem=4084000MB        # Total memory needed per task (units: K,M,G,T)
#SBATCH --mem=16G

## Specify how much time your job needs. (default: see partition above)
##SBATCH --time=5-00:00  # Total time needed for job: Days-Hours:Minutes

## Specify number CPUs
##SBATCH --cpus-per-task 32
##SBATCH --nodes=2 --exclusive
#SBATCH --array=1-3 --exclusive
#SBATCH --constraint=intel

## Load the relevant modules needed for the job
module load python 

## Run your program or script
python /scratch/mjarretb/compile/experiments.py
