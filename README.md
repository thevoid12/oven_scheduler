# CS 4118 — Homework 4

Some files and the Part 1 measurements come from teammates and the course
starter code. See [CREDITS.md](CREDITS.md).

---

# Part 1 — eBPF Scheduler Tracing and CFS Measurements

**Status: Working**

### Files added/modified

- `user/trace.bt` — bpftrace script for measuring runqueue latency and total completion time.
- `user/Makefile` — updated to build `fibonacci`.
- `user/run_tasks_cfs.sh` — launcher used to run the tasksets under the default CFS scheduling policy.
- `user/taskset1.txt` and `user/taskset2.txt` — taskset input files used for the CFS measurements.

### CFS measurements

The workloads were executed with 100 Fibonacci processes per taskset.

| Taskset | CPU configuration | Average completion time |
| --- | --- | ---: |
| Taskset 1 | 1 CPU | 310.51 ms |
| Taskset 2 | 1 CPU | 60.11 ms |
| Taskset 1 | 4 CPUs | 79.31 ms |
| Taskset 2 | 4 CPUs | 24.26 ms |

The four-CPU measurements were performed using the VM's normal four-CPU configuration. The single-CPU measurements were performed by booting the VM with the `maxcpus=1` kernel boot parameter. The CPU configuration was verified using `nproc`.

### Testing

The `trace.bt` script was run with `bpftrace` before launching each taskset. Each taskset contains 100 Fibonacci processes, and the trace produced 100 corresponding Fibonacci measurements for each experiment.

The average completion times above were calculated from the `TOTAL_MS` field of the 100 recorded task completions for each configuration.

---

# Part 2 — Oven Scheduler

**Status: Working**

The Oven scheduler was implemented as a new Linux scheduling class in:

`linux/kernel/sched/oven.c`

The scheduler maintains an Oven-specific runqueue and orders runnable tasks by their normalized Oven weight.

The implementation includes the required scheduling-class callbacks, including:

- enqueueing and dequeueing tasks
- selecting the next task
- updating the currently running task
- preemption checks
- task ticking
- yielding
- CPU selection
- handling transitions into the Oven scheduling class

The Oven runqueue maintains tasks in weight order so that lower-weight tasks are selected before higher-weight tasks. Equal-weight tasks retain FIFO ordering.

---

# Part 3 — Oven Integration and Scheduling Policy

**Status: Working**

The Oven scheduling class was integrated into the kernel scheduler and made available through the scheduler policy interface.

The relevant scheduler implementation is:

`linux/kernel/sched/oven.c`

The Oven scheduler was also made the default scheduler for the assignment workload.

The implementation was tested using the provided Fibonacci workloads on the custom kernel.

---

# Part 4 — Oven Priorities, Weights, and Time Slices

**Status: Working**

The Oven scheduler was extended to support the required priority/weight behavior.

The implementation defines:

```c
#define OVEN_MAX_WEIGHT 10000
#define OVEN_MIN_WEIGHT 2064
#define OVEN_TIMESLICE 1
```

Oven priorities outside the supported range are normalized to the maximum weight.

The scheduler also implements time-slice handling for the maximum-weight workload. Lower-weight tasks are selected according to the Oven ordering policy, while maximum-weight tasks are subject to the configured time slice.

The relevant changes are in:

- `linux/kernel/sched/oven.c`
- the scheduler policy handling in the kernel scheduler code
- `user/fibonacci.c`

### Oven measurements

The following files contain the Part 4 measurements:

- `user/taskset1_average.txt` — Oven, 1 CPU
- `user/taskset2_average.txt` — Oven, 1 CPU
- `user/taskset1_average_smp.txt` — Oven, 4 CPUs
- `user/taskset2_average_smp.txt` — Oven, 4 CPUs

The exact average values from these traces will be included in the final results table after extracting them from the trace files.

---

# Results

## CFS Baseline — Part 1

| Taskset | CPU configuration | Scheduler | Average completion time |
| --- | --- | --- | ---: |
| Taskset 1 | 1 CPU | CFS | 310.51 ms |
| Taskset 2 | 1 CPU | CFS | 60.11 ms |
| Taskset 1 | 4 CPUs | CFS | 79.31 ms |
| Taskset 2 | 4 CPUs | CFS | 24.26 ms |

## Oven — Part 4

| Taskset | CPU configuration | Scheduler | Average completion time |
| --- | --- | --- | ---: |
| Taskset 1 | 1 CPU | Oven | To be extracted |
| Taskset 2 | 1 CPU | Oven | To be extracted |
| Taskset 1 | 4 CPUs | Oven | To be extracted |
| Taskset 2 | 4 CPUs | Oven | To be extracted |

The corresponding raw measurements are included in the submitted trace files.

---

# Part 5 — Oven Idle Load Balancing

**Status: Implemented; final runtime validation pending**

The Oven scheduler was extended with an idle-load-balancing mechanism in:

`linux/kernel/sched/oven.c`

The implementation adds a `balance_oven()` callback to the Oven scheduling class.

When an Oven runqueue is empty, the balancing code searches other online CPUs for an Oven runqueue containing at least two runnable tasks. If an eligible task can run on the idle CPU, one task is moved from the source runqueue to the idle CPU's runqueue.

The implementation uses the scheduler's runqueue locking mechanisms, including `double_lock_balance()`, and moves the task using `move_queued_task_locked()`.

The relevant scheduler-class entry is:

```c
.balance = balance_oven,
```

### Requirements handled

- Balancing is attempted only when the current Oven runqueue is empty.
- A source CPU must have at least two Oven tasks before a task is stolen.
- The selected task must be allowed to execute on the destination CPU.
- The source and destination runqueues are locked while the task is moved.
- If no eligible task exists, the balancing pass does nothing.

### Part 5 measurements

The final four-CPU measurements still need to be generated after the custom kernel successfully boots.

| Taskset | CPU configuration | Scheduler | Average completion time |
| --- | --- | --- | ---: |
| Taskset 1 | 4 CPUs | Oven + idle balancing | Pending |
| Taskset 2 | 4 CPUs | Oven + idle balancing | Pending |

Required output files:

- `user/taskset1_smp_balance.txt`
- `user/taskset2_smp_balance.txt`

---

# Part 6 — Tail Completion Time

**Status: Implementation/testing pending**

Part 6 measures tail completion time rather than average completion time.

The required metric is the maximum completion time among 99% of the jobs in the workload.

The recursive `fib()` function is not modified.

The required files are:

- `user/fibonacci_tail.c`
- `user/taskset1_tail.txt`
- `user/taskset2_tail.txt`

### Tail completion results

| Taskset | CPU configuration | Scheduler | Tail completion time |
| --- | --- | --- | ---: |
| Taskset 1 | 4 CPUs | CFS | Pending |
| Taskset 1 | 4 CPUs | Optimized Oven | Pending |
| Taskset 2 | 4 CPUs | CFS | Pending |
| Taskset 2 | 4 CPUs | Optimized Oven | Pending |

The final Part 6 measurements will be generated after the custom kernel is successfully booted and the workload is tested.

---

# Part 7 — Kernel Scheduling Questions

**Status: Completed**

The answers to the Part 7 questions are provided in:

- `user/part7.txt`

The required Bootlin v7.0 source references are provided in:

- `references.txt`

---

# Files Submitted

The final submission contains the following required files:

- `README`
- `references.txt`
- `linux/kernel/sched/oven.c`
- `fibonacci.c`
- `fibonacci_tail.c`
- `user/trace.bt`
- `user/taskset1_average.txt`
- `user/taskset2_average.txt`
- `user/taskset1_average_smp.txt`
- `user/taskset2_average_smp.txt`
- `user/taskset1_smp_balance.txt`
- `user/taskset2_smp_balance.txt`
- `user/taskset1_tail.txt`
- `user/taskset2_tail.txt`
- `user/part7.txt`


---

# Testing Environment

The custom scheduler is implemented in the Linux v7.0 kernel source supplied with the assignment.

Testing is performed on a four-vCPU ARM64 GCP VM.

The assignment workloads use the provided Fibonacci program and taskset input files. CFS measurements are collected using the default Linux scheduling policy, while Oven measurements use the custom `SCHED_OVEN` scheduler.

The single-CPU CFS measurements were obtained using the `maxcpus=1` kernel boot parameter, while the four-CPU measurements were obtained using the normal four-vCPU VM configuration.

# Additional Notes

The custom kernel build and boot process required additional debugging because kernel installation and initramfs generation can consume substantial space in `/boot`.

The Part 5 idle-load-balancing implementation has been committed and pushed to the repository. Final Part 5 and Part 6 performance results remain to be collected after successfully booting the custom kernel.
