// SPDX-License-Identifier: GPL-2.0-only
/*
 * implementation of the oven task scheduling class.
 *
 */
#include "sched.h"

void __setparam_oven(struct task_struct *p, const struct sched_attr *attr)
{
	p->ov.weight = attr->sched_priority;
	pr_info("setparam oven %s (pid %d)\n", p->comm, p->pid);
}


void init_oven_rq(struct oven_rq *oven_rq)
{
	INIT_LIST_HEAD(&oven_rq->queue);
	oven_rq->nr_running = 0;
}


static void enqueue_task_oven(struct rq *rq, struct task_struct *p, int flags)
{
    list_add_tail(&p->ov.run_list, &rq->ov.queue);
	p->ov.on_rq = 1;
	rq->ov.nr_running++;
	add_nr_running(rq, 1);  /* this is needed for kernel wide count across all schedulers*/
	pr_info("enqueue task oven %s (pid %d)\n", p->comm, p->pid);

}

static bool dequeue_task_oven(struct rq *rq, struct task_struct *p, int flags)
{
	list_del_init(&p->ov.run_list);
	p->ov.on_rq = 0;
	rq->ov.nr_running--;
	sub_nr_running(rq, 1);
	pr_info("dequeue task oven %s (pid %d)\n", p->comm, p->pid);

	return true;
}


/*
 * Called on every pass over the scheduling classes, whether or not oven has
 * tasks. No oven runqueue yet, so there is never a task to pick: return NULL
 * and let the scheduler fall through to the idle class.
 */
static struct task_struct *pick_task_oven(struct rq *rq, struct rq_flags *rf)
{
	if (list_empty(&rq->ov.queue))
	return NULL;

	return list_first_entry(&rq->ov.queue, struct task_struct, ov.run_list);
}

static void set_next_task_oven(struct rq *rq, struct task_struct *next, bool first)
{
}

static void put_prev_task_oven(struct rq *rq, struct task_struct *prev,
			       struct task_struct *next)
{
}

static void task_tick_oven(struct rq *rq, struct task_struct *curr, int queued)
{
}

static void update_curr_oven(struct rq *rq)
{
}

static void wakeup_preempt_oven(struct rq *rq, struct task_struct *p, int flags)
{
}

static void prio_changed_oven(struct rq *rq, struct task_struct *p, u64 oldprio)
{
}

static int select_task_rq_oven(struct task_struct *p, int cpu, int flags)
{
	return task_cpu(p);
}

static void yield_task_oven(struct rq *rq)
{
}

/*
 * Simple, special scheduling class for oven tasks:
 */
DEFINE_SCHED_CLASS(oven) = {
	.enqueue_task		= enqueue_task_oven,
	.dequeue_task		= dequeue_task_oven,

	.wakeup_preempt		= wakeup_preempt_oven,

	.pick_task		= pick_task_oven,
	.put_prev_task		= put_prev_task_oven,
	.set_next_task          = set_next_task_oven,

	.balance		= balance_oven,
	.select_task_rq		= select_task_rq_oven,
	.set_cpus_allowed	= set_cpus_allowed_common,

	.task_tick		= task_tick_oven,

	.prio_changed		= prio_changed_oven,
	.switching_to		= switching_to_oven,
	.update_curr		= update_curr_oven,
	.yield_task		= yield_task_oven,
};

