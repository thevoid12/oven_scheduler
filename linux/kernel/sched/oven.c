// SPDX-License-Identifier: GPL-2.0-only
/*
 * implementation of the oven task scheduling class.
 *
 */
#include "sched.h"



static void enqueue_task_oven(struct rq *rq, struct task_struct *p, int flags)
{
    pr_info("initial boilerplate run for oven enqueue\n");
}



/*
 * Simple, special scheduling class for oven tasks:
 */
DEFINE_SCHED_CLASS(oven) = {
    .enqueue_task = enqueue_task_oven
	// .dequeue_task		= dequeue_task_oven,

	// .wakeup_preempt		= wakeup_preempt_oven,

	// .pick_task		= pick_task_oven,
	// .put_prev_task		= put_prev_task_oven,
	// .set_next_task          = set_next_task_oven,

	// .balance		= balance_oven,
	// .select_task_rq		= select_task_rq_oven,
	// .set_cpus_allowed	= set_cpus_allowed_common,

	// .task_tick		= task_tick_oven,

	// .prio_changed		= prio_changed_oven,
	// .switching_to		= switching_to_oven,
	// .update_curr		= update_curr_oven,
};

