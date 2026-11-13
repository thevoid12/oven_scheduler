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

	/*
	 * Sorted by weight for shortest job first;
	 * equal weights stay first come, first served.
	 */
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
	/*
	 * A new task with a smaller weight than the running one
	 * preempts it, so the shortest job runs first.
	 */
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

	/*
	 * Weighted tasks (shortest job first) are not time sliced,
	 * so they keep running.
	 */
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
 * Idle balancing: pick the online CPU with the most oven tasks,
 * as long as it has at least two. Lockless read, so it is only a hint;
 * the count is checked again under the locks.
 */
static struct rq *find_busiest_oven_rq(struct rq *this_rq)
{
	struct rq *busiest = NULL;
	unsigned int max = 1;
	int cpu;

	for_each_online_cpu(cpu) {
		struct rq *src_rq = cpu_rq(cpu);
		unsigned int nr = READ_ONCE(src_rq->ov.nr_running);

		if (src_rq == this_rq)
			continue;
		if (nr > max) {
			max = nr;
			busiest = src_rq;
		}
	}
	return busiest;
}

/*
 * The queue is sorted by weight, so the first task that is not running,
 * may move to this cpu and is not migration disabled is the shortest
 * waiting job. Pulling it keeps shortest job first across cpus.
 */
static struct task_struct *pick_pullable_oven_task(struct rq *src_rq, int this_cpu)
{
	struct sched_oven_entity *e;

	list_for_each_entry(e, &src_rq->ov.queue, run_list) {
		struct task_struct *p = container_of(e, struct task_struct, ov);

		if (p == src_rq->donor || is_migration_disabled(p))
			continue;
		if (task_is_pushable(src_rq, p, this_cpu))
			return p;
	}
	return NULL;
}

/* Pull one oven task to this_rq, which has no oven tasks. */
static void pull_oven_task(struct rq *this_rq)
{
	struct rq *src_rq = find_busiest_oven_rq(this_rq);
	struct task_struct *p;

	if (!src_rq)
		return;

	/*
	 * double_lock_balance() takes both locks in a fixed order to avoid
	 * deadlock, and may drop this_rq's lock to do so. Recheck both
	 * counts after it returns.
	 */
	double_lock_balance(this_rq, src_rq);

	if (this_rq->ov.nr_running == 0 && src_rq->ov.nr_running >= 2) {
		p = pick_pullable_oven_task(src_rq, this_rq->cpu);
		if (p)
			move_queued_task_locked(src_rq, this_rq, p);
	}

	double_unlock_balance(this_rq, src_rq);
}

/*
 * Called by prev_balance() in core.c before picking the next task.
 * Returns true if a task of oven or higher class is runnable, which
 * stops the balance pass of the lower classes.
 */
static int balance_oven(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	if (rq->ov.nr_running == 0) {
		rq_unpin_lock(rq, rf);
		pull_oven_task(rq);
		rq_repin_lock(rq, rf);
	}

	return sched_stop_runnable(rq) || sched_dl_runnable(rq) ||
	       sched_rt_runnable(rq) || rq->ov.nr_running > 0;
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
	.update_curr		= update_curr_oven,
	.yield_task		= yield_task_oven,
	.get_rr_interval	= get_rr_interval_oven,
	.switched_to		= switched_to_oven,
};

