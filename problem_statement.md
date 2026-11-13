# Programming Problems:

Having completed the first half of your operating systems training, you and your team are now well-versed OS developers, and have been hired by W4118 Inc. to work on their next-generation operating systems. In this next-gen OS, W4118 Inc. has tasked your team to improve the Linux kernel’s scheduling capabilities and optimize for workloads typical of W4118 Inc.’s customers.

Data privacy is important in modern computing, so instead of real customer programs, W4118 Inc. has provided you with a way to simulate the typical workloads submitted by end users using a Fibonacci calculator. The following links provide two task set workloads, each consisting of a list of jobs, with each line representing the N-th Fibonacci number to compute:

- TASK SET 1
- TASK SET 2

> **Note:** The Fibonacci calculator has been provided as user/fibonacci.c. It uses an inefficient algorithm by design to produce differing job lengths. You can modify the file, but do not modify the fib function.

Given these two task sets, W4118 Inc. is interested in a scheduler (let’s call it “Oven”) that can optimize for the completion time of the tasks. Specifically, the scheduler should provide the minimum average completion time for each task set across all tasks in the respective task set.

## Part 1: Measure scheduler performance with eBPF

To see how well a scheduler performs, it is necessary to first have a way to measure performance, specifically completion time of tasks. W4118 Inc. is looking for a way to trace scheduling events and determine how well a scheduler functions by measuring tasks’ run queue latency and total duration from start to finish.

Extended Berkeley Packet Filter (eBPF) is a powerful feature of the Linux kernel that allows programs to inject code into the kernel at runtime to gather metrics and observe kernel state. You will use eBPF to trace scheduler events. Tracepoints for these scheduler events are already available in the scheduler (for example, you can search in core.c for trace_sched_), so your job is to write code that will be injected into the kernel to use them. You should not need to modify any kernel source code files for this first part of the assignment.

bpftrace is a Linux tool that allows using eBPF with a high level, script-like language. Install it on your VM with:

```sh
sudo apt install bpftrace
```

Instead of searching through source code, you can run sudo bpftrace -l to see what available tracepoints there are in the kernel.

Write a bpftrace script named trace.bt to trace how much time a task spends on the run queue, and how much time has elapsed until the task completed. Your profiler should run until stopped with Ctrl-C. While tracing, your script should, at tasks’ exits, print out a table of the task name, task PID, milliseconds spent on the run queue (not including time spent running), and milliseconds until task completion. Note that task PID here is the actual pid field in a task_struct, not what is returned by getpid. The output should be comma-delimited and follows the format:

```text
COMM,PID,RUNQ_MS,TOTAL_MS
myprogram1,5000,2,452
myprogram2,5001,0,15
...
```

Ensure that the output of your eBPF script is synchronous. That is, it should print each line immediately after a task completes, and not when Ctrl-C is sent to the eBPF script. You should also only print traces from processes that have started after your script begins running. You should trace all such processes and perform no additional filtering.

Test your script by running sudo bpftrace trace.bt in one terminal. In a separate terminal, run a few commands and observe the trace metrics in your first terminal. Experiment with different task sizes. Submit your eBPF script as user/trace.bt.

Now that you have an eBPF measurement tool, use it to measure the completion times for the two task set workloads when running on the default CFS (Completely Fair Scheduler) scheduler in Linux. What is the resulting average completion time for each workload? Write the average completion times of both workloads in your README. You may find this shell script helpful for running your tasks. You should perform your measurements for two different CPU configurations of your VM: a single-CPU configuration and a four-CPU configuration. We are using the term CPU here to mean a core; the N4A (Google Axion Arm) machines used in this class do not use simultaneous multithreading, so a vCPU is a physical core. Your VM is normally provisioned as n4a-standard-4 (four vCPUs).

To measure with a single CPU, reboot with the maxcpus=1 kernel boot parameter: at the GRUB menu (see the kernel compilation guide for how to reach it), press e on the entry you want to boot, add maxcpus=1 to the end of the line beginning with linux, then boot with Ctrl-X or F10. After booting, verify that nproc outputs 1. This takes effect for that boot only; reboot normally afterward to get all four CPUs back. (If you would rather test on a genuinely smaller machine, you may instead create a second, throwaway n4a-standard-1 VM from a snapshot of your disk, but maxcpus=1 is faster and does not cost anything extra.) Specify in your README the number of CPUs used with your VM for your measurements.

> **Hint:** You may find it useful to reference the bpftrace GitHub project, which contains a manual and a list of sample scripts. A useful example script to start with is runqlat.bt.

> **Note:** Since your profiler will run on the same machine as the test workloads, you will need to ensure that the profiler gets sufficient CPU time to record data. What could you do to the profiler’s scheduling class and priority to ensure it can run over the test workloads? You may find chrt helpful.

> **Note:** It is important to ensure that your trace.bt script is correct so that you are able to rely on the measurements produced by the script when comparing your custom scheduler to the default CFS scheduler.

### Deliverables

- trace.bt script in user/trace.bt
- Average completion times for both tasksets on a single-CPU VM and on a VM configured with four CPUs, using the default CFS scheduling policy, in your README
- Make sure to specify the CPU configuration used for each measurement in your README

## Oven Overview

W4118 Inc. has tasked you with creating a new scheduling policy, Oven, that provides better average completion time than the default Linux scheduling policy for the fibonacci workload. Oven should work as follows:

- Only tasks whose policy is set to SCHED_OVEN should be considered for selection by your new scheduler.
- Every task will have an assigned weight. Weights should be configured when a process calls the sched_setscheduler system call, and passed as the sched_priority field of struct sched_param.
- By default, tasks have a weight set to a MAX_WEIGHT of 10000. Tasks with a weight equal to the MAX_WEIGHT of 10000 should use an unweighted round-robin scheduling algorithm. Each such task should get a time quantum of 1 tick.
- Tasks with a weight less than 10000 should be prioritized over weights equal to MAX_WEIGHT, and use your choice of scheduling algorithm and time quantum to optimize the overall average completion time metric for running the Fibonacci workload. Essentially, your scheduling class defaults to round-robin but should have a special mode that is optimized for running the Fibonacci workload, where the optimized scheduling algorithm is up to you to decide.
- Tasks using the SCHED_OVEN policy should take priority over tasks using the SCHED_NORMAL policy, but not over tasks using the SCHED_RR or SCHED_FIFO policies.
- The new scheduling policy should operate alongside the existing Linux schedulers. The value of SCHED_OVEN should be 8.
- You should keep track of runtime statistics so commands like top work with your scheduling class.
- A CPU should attempt to steal a task from another CPU when it doesn’t have any tasks to run. The specific conditions in which it should be able to steal a task are detailed in Part 5.

## Part 2: Prepare the Oven

Background readings:

- Sched class methods
- Medium Article
- Scheduler deep dive

> **Note:** some of the information in the links may be outdated (e.g. the second source claims the scheduling classes have “.next” pointers to the scheduling class of next highest precedence, which is no longer accurate), so while they are helpful as a reference, your best “source of truth” is still the 7.0 kernel.

### Part 2.1: Oven data structures

The Linux scheduler implements individual scheduling classes corresponding to different scheduling policies. For this assignment, you need to create a new scheduling class, oven_sched_class, for the OVEN policy, and implement the necessary functions in kernel/sched/oven.c. The goal of this section is to set up the Oven data structures and a skeleton scheduling class. Initially, you will not use the Oven scheduler to run anything, but rather you just want to ensure that your modifications do not crash the kernel. In other words, you should define the following and ensure that your updated kernel boots:

```c
#define  SCHED_OVEN  8

struct oven_rq;

struct sched_oven_entity;

struct sched_class oven_sched_class;
```

You will need to examine the following files and make specific modifications: kernel/sched/core.c, kernel/sched/sched.h, include/linux/sched.h, and include/asm-generic/vmlinux.lds.h. While there is a fair amount of code in these files, a key goal of this assignment is for you to understand how to abstract the scheduler code so that you learn in detail the parts of the scheduler that are crucial for this assignment and ignore the parts that are not.

In particular, at this point you should have SCHED_NORMAL take priority over your SCHED_OVEN policy so that bugs in your scheduling class are less likely to prevent other tasks from running. Once you have your scheduling class working, you can then switch the priority of these policies in the later parts of the homework.

In order to set up a minimal oven_sched_class, a good place to start is the simple scheduling class for idle tasks, listed in kernel/sched/idle.c. This is the simplest of the scheduling classes. In particular, the functions implemented by this scheduling class are a good indication of the minimum set of functions you need to implement to have an operational scheduling class. You can return placeholder values in your scheduler class functions for now; the following section will revisit these functions. As an alternative, you can also look at the simple scheduling class for stop tasks, listed in kernel/sched/stop_task.c.

#### Hints

- Pay careful attention to how other scheduling classes initialize their class-specific run queues and scheduling entities.
- Avoid using complex data structures that may provide better theoretical runtime complexity but are harder to implement and debug. In general, lists are fine to use.
- Check out the debugging tips provided below.

#### Checkpoints

Ensure that everything compiles and no previous functionality has been affected:

- When you run fibonacci, your output should still look like this:

  ```text
  $ ./fibonacci 10
  OVEN scheduling policy does not exist
  ```

- Inspect the output of the following command to ensure that it matches what you would observe before making your changes. You may find the man page of the ps command helpful for understanding the output.

  ```sh
  ps -e --forest -o sched,policy,psr,pcpu,c,pid,user,cmd
  ```

### Part 2.2: Scheduling 1 task

The rest of Part 2 will build toward enabling individual tasks, like fibonacci, to set their scheduling class to Oven. You will first implement the default case of Oven, which is round robin, then optimize it in Part 4. It will be helpful to reference the way other scheduling classes implement their core functions: kernel/sched/rt.c and kernel/sched/fair.c. You may find the former particularly useful because it has its own version of a round-robin scheduler, SCHED_RR, though you will likely find many parts of the code too complex to use directly for your own scheduling class.

This section focuses on enabling 1 task to set its scheduling class to Oven. There are two main components to this:

- Add basic functionality to your oven scheduling class so that it is able to add 1 task to its run queue and pick that task to run. You should keep your run queue simple for this part.
- Trace through the sched_setscheduler system call that fibonacci uses to set its scheduling class. Modify the related functions to enable setting to the Oven scheduling class.

You should add printk or pr_info logs to the Oven function(s) you implemented to ensure that they are properly called when you run fibonacci.

### Part 2.3: Scheduling statistics

Although you can run a single task using your Oven scheduling class, you may notice that standard user tools may not correctly show that the task is running and using up CPU time. To fix this, you should implement functionality in your scheduling class to update the execution statistics for the task– this can help with debugging in the future through commands like top or htop. Reference other schedulers to see how they update the execution statistics for their tasks.

### Part 2.4: Scheduling multiple tasks

You will now add support for running multiple tasks in a round robin fashion. As mentioned before, each such task should get a time quantum of 1 tick. After this time quantum, another task should run if there are multiple tasks on the run queue.

Make the relevant changes to your Oven scheduling class. After you make your updates, you should be able to launch multiple fibonacci tasks using the shell script you wrote in Part 1. It is important before moving on to ensure that multiple tasks have the opportunity to run and are picked to run in a round robin fashion.

### Part 2.5: Scheduling multiple tasks that sleep

In this section, you will ensure that fibonacci tasks can consistently run to completion and sleep. One hint is to think about what happens to a task with respect to the run queue when it sleeps and support this functionality in your Oven scheduling class.

You should add a sleep(1) call in your fibonacci.c file and ensure that the updated program consistently runs to completion. Again, you should use printk or pr_info logs to verify that your Oven scheduling class functions are actually invoked and used in the manner that you intend. The sleep and debugging logs should only be added for testing purposes and should not be in your submission. In particular, debugging statements can cause issues if invoked too frequently.

#### Checkpoints

- Test your scheduler on a few runs of fibonacci. Your fibonacci output should now look like this:

  ```text
  $ ./fibonacci 10
  $ echo $?
  55
  ```

- Inspect your fibonacci program using the previously mentioned ps command– you may find it helpful to add an infinite loop at the end of your program to give you time to inspect it.
- Recall how we previously had SCHED_NORMAL take priority over your SCHED_OVEN policy. Now modify the kernel so that SCHED_OVEN takes precedence over SCHED_NORMAL, but remains below the SCHED_RR and SCHED_FIFO policies. Verify that your Oven scheduling class is correct by launching multiple fibonacci tasks using the shell script you wrote in Part 1 and conducting a stress test. You should also try running the program from homework 3 that tests state changes. This will exercise your scheduling code more thoroughly as it will involve scheduling processes that have a greater variety of state changes.

### Part 2.6: Scheduling all tasks from a shell

To more rigorously test your scheduler, you should launch a shell in the Oven scheduling class and run various programs from that shell. Programs launched from that shell should inherit its scheduling parameters and therefore run using the Oven scheduling class. For example, as part of your kernel development, the actual kernel compilation using make -j should be able to run from that shell as well. This will exercise your scheduling code even more thoroughly as it will involve using the scheduler for more complex programs as part of your development process.

> **Hint:** At this point, excessive printk() or pr_info() logging may cause problems because of how often the scheduling functions are executed if they are being used to run many tasks. Hence, you may want to limit the logs in your oven_sched_class functions by only printing for a limited selection of tasks using if-statements.

### Deliverables

- Implementation of your scheduler in linux/kernel/sched/oven.c
- Modifications to the Linux source code

## Part 3: Oven as the Default Scheduler

The goal of this part is to set Oven to be the default scheduler of your system. You may also choose to first focus on optimizing Oven, as described in Part 4, then return to this section, or work on both at the same time by having different team members work on different parts.

You will need to change different parts of the kernel code to allow your kernel to boot with your scheduling class as the default.

- Consider how the scheduling policy for the init_task is assigned and how new tasks are assigned to a scheduling class by default.
- For a more responsive system, you may want to set the scheduler of kernel threads to be SCHED_OVEN as well (otherwise, SCHED_OVEN tasks can starve the SCHED_NORMAL tasks to a degree). To do this, you can modify kernel/kthread.c and replace SCHED_NORMAL with SCHED_OVEN. It is strongly suggested that you do this to ensure that your VM is responsive enough for the test cases, but you should not do this until you are certain your scheduler works properly.

#### Checkpoints

- You may want to write a test program that forks and examine the program’s details through the ps command:

  ```text
  :~$ ps -e --forest -o sched,policy,psr,pcpu,c,pid,user,cmd
  SCH POL PSR %CPU  C     PID USER     CMD
    8 #8    0  0.0  0       2 root     [kthreadd]
    8 #8    0  0.0  0       3 root      \_ [pool_workqueue_release]
    8 #8    0  0.0  0       4 root      \_ [kworker/R-rcu_gp]
    8 #8    0  0.0  0       5 root      \_ [kworker/R-sync_wq]
    8 #8    0  0.0  0       6 root      \_ [kworker/R-kvfree_rcu_reclaim]
    8 #8    0  0.0  0       7 root      \_ [kworker/R-slub_flushwq]
    8 #8    0  0.0  0       8 root      \_ [kworker/R-netns]
    8 #8    0  0.0  0      10 root      \_ [kworker/0:0H-events_highpri]
    8 #8    0  0.0  0      11 root      \_ [kworker/u16:0-flush-259:0]
  ...
    8 #8    0  1.9  1    6567 user      \_ sshd: user@pts/0
    8 #8    0  0.1  0    6574 user          \_ bash
    8 #8    0  0.0  0    6601 user              \_ ./mytest
    8 #8    0  0.0  0    6602 user                  \_ ./mytest
    8 #8    0  0.0  0    6603 user                  |   \_ ./mytest
    8 #8    0  0.0  0    6604 user                  |       \_ ./mytest
    8 #8    0  0.0  0    6605 user                  \_ ./mytest
    8 #8    0  0.0  0    6606 user                  |   \_ ./mytest
    8 #8    0  0.0  0    6607 user                  |       \_ ./mytest
    8 #8    0  0.0  0    6608 user                  \_ ./mytest
    8 #8    0  0.0  0    6609 user                  |   \_ ./mytest
    8 #8    0  0.0  0    6610 user                  |       \_ ./mytest
  ...
  ```

- You may want to write your own stress test program and run it with Oven as the default scheduling policy.

### Deliverables

- Modifications to the Linux source code

## Part 4: Time to Cook - Optimize Oven

Recall your average completion time measurements from Part 1, which used the CFS scheduler. Can you do better? In this section, you will optimize Oven, with the goal of beating CFS.

### Part 4.1: Enable custom weights

Trace through how the parameter(s) of the sched_setscheduler system call are ultimately used to set the weight of a task. Add support for dynamically setting a task’s weight in Oven.

You may want your scheduler to have a minimum valid weight well above the priority range normally used for sched_priority such that any weight assignment less than the minimum valid weight is instead forced to be the MAX_WEIGHT. The reason for this is that there are system programs that use sched_setscheduler with lower values for sched_priority. If they are scheduled using your scheduling class, you want to detect their invalid weight assignments and schedule them using your round-robin algorithm.W4118 Inc.’s convention is that the minimum valid weight ends in digits 64.

You can verify that the correct value is set by adding selective printk or pr_info logs and running fibonacci with different weights.

### Part 4.2: Beat CFS

In order to beat CFS, a good starting point is considering what makes the Fibonacci workload unique– those characteristics should guide your scheduler design because this assignment asks you to optimize your scheduler specifically for the Fibonacci workload. You should consider how you might modify the fibonacci program to set its OVEN weight. What weights and scheduling algorithm will optimize for average completion time?

Make any changes to fibonacci and your scheduler. As you iterate on your scheduler, you can use your eBPF script from Part 1 to measure the performance of your optimized scheduler and compare it against the default CFS scheduler.

After you have optimized your scheduler, submit eBPF traces of the two tasksets running on your scheduler. You should use the same two VM CPU configurations you used earlier to measure performance using CFS. Submit your eBPF traces for the two task sets and CPU configurations as user/taskset1_average.txt, user/taskset1_average_smp.txt, user/taskset2_average.txt, and user/taskset2_average_smp.txt. Write the average completion times for each workload in your README and briefly describe how your Oven scheduler compares against CFS.

> **Hint:** Configuring the scheduling class and priority within the Fibonacci program will be too late (why is this the case?). You may want to use chrt while launching your Fibonacci tasks or write a small program to set the scheduler and weight before calling exec on Fibonacci.

> **Note:** When launching jobs from the task list, start tasks in the order they are listed, but do not wait for tasks to finish before launching the next one. For all trace submissions in this section, filter out any process that is not fibonacci.

### Deliverables

- Changes to fibonacci.c
- Modifications to the Linux source code
- user/taskset1_average.txt, user/taskset1_average_smp.txt, user/taskset2_average.txt, and user/taskset2_average_smp.txt
- Average completion times for both tasksets on a single-CPU VM and on a VM configured with four CPUs, using the Oven scheduling policy, in your README
- Comparison between the Oven average completion time and CFS average completion time in your README

## Part 5: Add load-balancing features

So far, your scheduler will run a task only on the CPU that it was originally assigned, which may also limit its performance especially compared to CFS. For this part you will be implementing idle balancing, which is when a CPU will attempt to steal a task from another CPU when it doesn’t have any tasks to run (i.e. when its runqueue is empty).

Load balancing is a key feature of the default Linux CFS scheduler. While CFS follows a rather complicated policy to balance tasks and works to move more than one task in a single go, your scheduler can have a simpler policy in this regard, though you will want to consider what is the best algorithm to use for load balancing to optimize for the Fibonacci workload.

Idle balancing works as follows:

- Idle balance is when an idle CPU (i.e. a CPU with an empty run queue) pulls one task from another CPU.
- Take a look at the CFS, RT, and DL implementations to figure out where this is taking place and how it is called from kernel/sched/core.c. In particular, you may want to reference the way RT does idle balancing as it is simpler than CFS.
- The CPU that you pull a task from should have at least two tasks on its run queue. Which task you pull off is up to your scheduling algorithm.
- You should again ensure that the task you are stealing is eligible to be moved and allowed to run on the idle CPU.
- If there is no eligible task, it is fine to not idle balance.

You may need to manually grab run queue lock(s) in order to move a task from one run queue to another. Make sure your implementation works in a deadlock free manner.

Once you add idle load balancing, repeat the performance tests you conducted in the previous part and make notes on any observations. Again, be sure to include actual data. Submit the trace in user/taskset1_smp_balance.txt and user/taskset2_smp_balance.txt and note any differences to your previous scheduler in your README. You only need to submit results in this case for the four-CPU VM configuration, and do not need to submit single-CPU VM results for this case. Write the updated average completion times for each workload in your README and compare against CFS.

### Deliverables

- Modifications to the Linux source code/your scheduler
- user/taskset1_smp_balance.txt and user/taskset2_smp_balance.txt
- Average completion times for both tasksets on a four-CPU VM, using the Oven scheduling policy with idle balancing in your README
- Comparison between Oven average completion time (with idle balancing) and CFS average completion time in your README

## Part 6: Tail Completion Time

Thus far you have focused on optimizing the average completion time across all jobs. Another important metric to consider is tail completion time, which is the maximum completion time across 99% of all jobs. Tail completion time focuses on ensuring that the completion time of most jobs is no worse than some amount. Using tail completion time as the performance metric of interest, compare your optimized scheduling class versus the default Linux scheduler for the W4118 Inc. workloads. Which one does better? If the default Linux scheduler has better tail completion time, can you change how you use your OVEN scheduler (without modifying the OVEN scheduler code) to provide tail completion time comparable to the default Linux scheduler?

Modify fibonacci.c as needed and save the updated version in a new file named fibonacci_tail.c. As before, do not make any changes to the fib function. Submit eBPF traces of the two task sets running on your scheduler as user/taskset1_tail.txt and user/taskset2_tail.txt. You only need to submit results in this case for the four-CPU VM configuration, and do not need to submit single-CPU VM results for this case. Write the tail completion times for each workload in your README and compare against CFS.

### Deliverables

- Your fibonacci code optimized for tail completion in fibonacci_tail.c
- user/taskset1_tail.txt and user/taskset2_tail.txt
- Tail completion times for both tasksets on a four-CPU VM, using the Oven scheduling policy in your README
- Comparison between your tail completion time and CFS tail completion time in your README

## Part 7: Analysis and Investigation of Kernel Source Code

Write the answers to the following questions in the user/part7.txt text file, following the provided template exactly. Make sure to include any references you use in your references.txt file.

- Give the exact URL on elixir.bootlin.com pointing to the file and line number of the function that initializes the idle tasks on CPUs other than the boot CPU for a multi-CPU system. What is the PID of the task that calls this function? Note: make sure you use v7.0.
- Give the exact URL on elixir.bootlin.com pointing to the file and line number at which the TIF_NEED_RESCHED flag is set for the currently running task as a result of its time quantum expiring if the task is scheduled using SCHED_RR. Select the file and line number most related to the SCHED_RR (i.e. do not select a generic helper function that may be used outside of SCHED_RR). Note: make sure you use v7.0.
- Give the exact URL on elixir.bootlin.com pointing to the file and line number at which a timer interrupt occurring for a process running in user mode, whose time quantum has expired, results in schedule being called. Select the line of the call to schedule. Note: make sure you use v7.0.
- What is the default time period for a tick? Write your answer in milliseconds. Hint: You may need to look in the kernel configuration files to find this answer.

## Debugging Tips

The following are some debugging tips you may find useful as you create your new scheduling class.

- Before testing, take a snapshot of your VM’s boot disk if you have not done so already (see Booting to the new kernel in the kernel compilation guide). That way, if your kernel crashes or is unresponsive because of scheduler malfunction, you can restore the disk to its state before the problem.
- It is possible that your kernel gets stuck at boot time or becomes unresponsive over SSH, preventing you from reading debug messages via dmesg. In this scenario, use GCP’s interactive serial console (also covered in the kernel compilation guide) rather than SSH: it is a separate, out-of-band console that stays usable even when the VM cannot be reached over the network. Reset the VM from the GCP console if it is fully hung, then use the serial console at the GRUB menu to boot back into your stock -gcp kernel and inspect what happened, or to add console=ttyS0 (or console=ttyS1) to the kernel command line for a boot if you need more verbose early console output.
- You may find it helpful to use ps, top, or htop to view information such as the CPU usage and scheduling policies of your tasks. These commands rely on execution time statistics reported by certain scheduler functions in the kernel. As a result, bugs in your scheduling class could cause these commands to report inaccurate information. This cuts two ways. First, it is possible that your scheduler is working properly in terms of selecting the right tasks to execute for the right amount of time, but your calculation of execution time statistics in the kernel is wrong, so these commands appear to report that your tasks are not running at all when in fact they are. Second, it is possible that your scheduler is not working properly such that these tools report that tasks are using your scheduling class when in fact they are not.

  As a result, you should not exclusively rely on these tools to determine if your scheduling class is working properly. For example, when you make your scheduling class the default scheduling class, the fact that user-level tools claim that all tasks are using your scheduling class may not necessarily mean that this is the case. Instead, to ensure that your scheduling class functions are actually being used, you might add a printk to a function like your class’s enqueue_task() and verify that it appears in dmesg output. Make sure that you do not submit your code with such debugging printk statements as they can cause issues if invoked too frequently.

- Check out the QEMU and GDB debugging guide.
