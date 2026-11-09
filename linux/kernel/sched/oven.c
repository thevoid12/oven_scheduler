// SPDX-License-Identifier: GPL-2.0-only
/*
 * implementation of the oven task scheduling class.
 *
 */
#include "sched.h"

void __setparam_oven(struct task_struct *p, const struct sched_attr *attr)
{
	/* pr_info("setparam oven %s (pid %d)\n", p->comm, p->pid);*/
	p->ov.weight = oven_normalize_weight(attr->sched_priority);
}


void init_oven_rq(struct oven_rq *oven_rq)
{
	INIT_LIST_HEAD(&oven_rq->queue);
	oven_rq->nr_running = 0;
}

static void update_curr_oven(struct rq *rq)
{
	struct task_struct *donor = rq->donor;

	if (donor->sched_class != &oven_sched_class)
		return;

	update_curr_common(rq);
}

static void enqueue_task_oven(struct rq *rq, struct task_struct *p, int flags)
{
	struct list_head *pos = &rq->ov.queue;
	struct sched_oven_entity *e;

	/* Sorted by weight for shortest job first
	 equal weights stay first come, first served. */
	list_for_each_entry(e, &rq->ov.queue, run_list) {
		if (e->weight > p->ov.weight) {
			pos = &e->run_list;
			break;
		}
	}
	list_add_tail(&p->ov.run_list, pos);
	p->ov.on_rq = 1;
	rq->ov.nr_running++;
	add_nr_running(rq, 1);  /* this is needed for kernel wide count across all schedulers*/
	/* pr_info("enqueue task oven %s (pid %d)\n", p->comm, p->pid); */
}

static bool dequeue_task_oven(struct rq *rq, struct task_struct *p, int flags)
{
	update_curr_oven(rq);
	list_del_init(&p->ov.run_list);
	p->ov.on_rq = 0;
	rq->ov.nr_running--;
	sub_nr_running(rq, 1);
	/*pr_info("dequeue task oven %s (pid %d)\n", p->comm, p->pid);*/

	return true;
}


static struct task_struct *pick_task_oven(struct rq *rq, struct rq_flags *rf)
{
	if (list_empty(&rq->ov.queue))
	return NULL;

	return list_first_entry(&rq->ov.queue, struct task_struct, ov.run_list);
}

static void set_next_task_oven(struct rq *rq, struct task_struct *p, bool first)
{
	p->se.exec_start = rq_clock_task(rq);
}

static void put_prev_task_oven(struct rq *rq, struct task_struct *prev,
			       struct task_struct *next)
{
		update_curr_oven(rq);
}


static void wakeup_preempt_oven(struct rq *rq, struct task_struct *p, int flags)
{
	/* when a new task comes in and we see its weight lesser than
	whats currently running on the run queue we just reschedule the rq
	so that the new task with less weigt gets picked up*/
	if (p->ov.weight < rq->donor->ov.weight)
		resched_curr(rq);
}

static void prio_changed_oven(struct rq *rq, struct task_struct *p, u64 oldprio)
{
}

static int select_task_rq_oven(struct task_struct *p, int cpu, int flags)
{
	return task_cpu(p);
}


static void requeue_task_oven(struct rq *rq, struct task_struct *p, int head)
{
	if (head)
		list_move(&p->ov.run_list, &rq->ov.queue);
	else
		list_move_tail(&p->ov.run_list, &rq->ov.queue);
}


static void task_tick_oven(struct rq *rq, struct task_struct *p, int queued)
{
	update_curr_oven(rq);

	/* Weighted tasks (shortest job first) are not time sliced. 
	so it will keep running*/
	if (p->ov.weight < OVEN_MAX_WEIGHT)
		return;

	if (--p->ov.time_slice > 0)
		return;

	p->ov.time_slice = OVEN_TIMESLICE;

	if (!list_is_singular(&rq->ov.queue)) {
		requeue_task_oven(rq, p, 0);
		resched_curr(rq);
	}
}

static unsigned int get_rr_interval_oven(struct rq *rq, struct task_struct *task)
{
	/*
	 * Time slice is 0 for SCHED_FIFO tasks
	 */
	if (task->policy == SCHED_OVEN)
		return OVEN_TIMESLICE;
	else
		return 0;
}


static void yield_task_oven(struct rq *rq)
{
	requeue_task_oven(rq, rq->donor, 0);
}

static void switched_to_oven(struct rq *rq, struct task_struct *p)
{
	if (task_on_rq_queued(p) && !task_current(rq, p))
		resched_curr(rq);   /* let schedule() re-evaluate */
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

	.select_task_rq		= select_task_rq_oven,
	.set_cpus_allowed	= set_cpus_allowed_common,

	.task_tick		= task_tick_oven,

	.prio_changed		= prio_changed_oven,
	.update_curr		= update_curr_oven,
	.yield_task		= yield_task_oven,
	.get_rr_interval 	= get_rr_interval_oven,
	.switched_to		= switched_to_oven,
};

