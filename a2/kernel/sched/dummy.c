/*
 * Dummy scheduling class, mapped to range of 5 levels of SCHED_NORMAL policy
 */

#include "sched.h"
#include <linux/kernel.h>
/*
 * Timeslice and age threshold are represented in jiffies. Default timeslice
 * is 100ms. Both parameters can be tuned from /proc/sys/kernel.
 */

#define DUMMY_TIMESLICE		(100 * HZ / 1000)
#define DUMMY_AGE_THRESHOLD	(3 * DUMMY_TIMESLICE)

#define NR_PRIO_LEVELS (5)
#define DUMMY_MIN_PRIO (131)

unsigned int sysctl_sched_dummy_timeslice = DUMMY_TIMESLICE;
static inline unsigned int get_timeslice()
{
  return sysctl_sched_dummy_timeslice;
}

unsigned int sysctl_sched_dummy_age_threshold = DUMMY_AGE_THRESHOLD;
static inline unsigned int get_age_threshold()
{
  return sysctl_sched_dummy_age_threshold;
}

/*
 * Init
 */

void init_dummy_rq(struct dummy_rq *dummy_rq, struct rq *rq)
{
  int i;
  for (i = 0; i < NR_PRIO_LEVELS; ++i) {
    INIT_LIST_HEAD(&(dummy_rq->queues[i]));
  }

  dummy_rq->aging_tick_count = 0;
}

/*
 * Helper functions
 */

static inline struct list_head *get_queue_for_priority(struct dummy_rq *rq, int prio)
{
  return &(rq->queues[prio - DUMMY_MIN_PRIO]);
}

static inline struct task_struct *dummy_task_of(struct sched_dummy_entity *dummy_se)
{
  return container_of(dummy_se, struct task_struct, dummy_se);
}

static inline void _enqueue_task_dummy(struct rq *rq, struct task_struct *p)
{
  struct sched_dummy_entity *dummy_se = &p->dummy_se;
  
  printk(KERN_ALERT "enqueue prio %d\n", p->prio);

  struct list_head *queue = get_queue_for_priority(&(rq->dummy), p->prio);
  list_add_tail(&dummy_se->run_list, queue);
}

static inline void _dequeue_task_dummy(struct task_struct *p)
{
  struct sched_dummy_entity *dummy_se = &p->dummy_se;
  list_del_init(&dummy_se->run_list);
}

/*
 * Scheduling class functions to implement
 */

static void enqueue_task_dummy(struct rq *rq, struct task_struct *p, int flags)
{
  printk(KERN_ALERT "enqueue called %d\n", p->prio);
  _enqueue_task_dummy(rq, p);
  inc_nr_running(rq);
}

static void dequeue_task_dummy(struct rq *rq, struct task_struct *p, int flags)
{
  _dequeue_task_dummy(p);
  dec_nr_running(rq);
}

static void yield_task_dummy(struct rq *rq)
{
  dequeue_task_dummy(rq, rq->curr, 0);
  enqueue_task_dummy(rq, rq->curr, 0);
  resched_task(rq->curr);
}

static void check_preempt_curr_dummy(struct rq *rq, struct task_struct *p, int flags)
{
  if (rq->curr->prio > p->prio) {
    resched_task(rq->curr);
  } else if (rq->curr->prio == p->prio) {

    if (p->dummy_se.rr_tick_count >= get_timeslice()) {    
      dequeue_task_dummy(rq, rq->curr, 0);
      enqueue_task_dummy(rq, rq->curr, 0);
      resched_task(rq->curr);
    }
  }
}

static struct task_struct *pick_next_task_dummy(struct rq *rq)
{
  
  struct dummy_rq *dummy_rq = &rq->dummy;
  struct sched_dummy_entity *next;
  int i = 0;

  for (i = 0; i < NR_PRIO_LEVELS; ++i) {
    struct list_head *queue = &(dummy_rq->queues[i]);
    if (!list_empty(queue)) {
      next = list_first_entry(queue, struct sched_dummy_entity, run_list);
      
      if (rq->curr->dummy_se.rr_tick_count >= get_timeslice()) {
	rq->curr->dummy_se.rr_tick_count = 0;
      }

      return dummy_task_of(next);
    }
  
  }
  
  return NULL;
}

static void put_prev_task_dummy(struct rq *rq, struct task_struct *prev)
{
}

static void set_curr_task_dummy(struct rq *rq)
{
}

static inline void perform_aging(struct rq *rq)
{
  struct task_struct *curr = rq->curr;
  int first_prio_to_age = (curr->prio) - DUMMY_MIN_PRIO + 1;
  
  if (first_prio_to_age < NR_PRIO_LEVELS) {
    int p;
    for (p = first_prio_to_age; p < NR_PRIO_LEVELS; ++p) {
      struct list_head *dest = get_queue_for_priority(&(rq->dummy), p-1);
      struct list_head *processes_to_age = get_queue_for_priority(&(rq->dummy), p);
      /* Take processes from one queue to put in the higher
       * priority queue. */
      while (!list_empty(processes_to_age)) {
	struct sched_dummy_entity* to_age;
	to_age = list_first_entry(processes_to_age, 
				  struct sched_dummy_entity, 
				  run_list); 
	dequeue_task_dummy(rq, dummy_task_of(to_age), 0);
	
	list_add_tail(&to_age->run_list, dest);
	inc_nr_running(rq);

	processes_to_age = get_queue_for_priority(&(rq->dummy), p);
      }
    }
  }

}

static void task_tick_dummy(struct rq *rq, struct task_struct *curr, int queued)
{
  rq->curr->dummy_se.rr_tick_count += 1;
  if (rq->curr->dummy_se.rr_tick_count >= get_timeslice()) {
    int curr_prio = curr->prio;
    
    dequeue_task_dummy(rq, curr, 0);
    enqueue_task_dummy(rq, curr, 0);
    resched_task(rq->curr);
  }

  rq->dummy.aging_tick_count += 1;
  if (rq->dummy.aging_tick_count >= get_age_threshold()) {
    perform_aging(rq);
    rq->dummy.aging_tick_count = 0;
  }
}

static void switched_from_dummy(struct rq *rq, struct task_struct *p)
{
}

static void switched_to_dummy(struct rq *rq, struct task_struct *p)
{
}

static void prio_changed_dummy(struct rq *rq, struct task_struct *p, int oldprio)
{
}

static unsigned int get_rr_interval_dummy(struct rq *rq, struct task_struct *p)
{
	return get_timeslice();
}

/*
 * Scheduling class
 */

const struct sched_class dummy_sched_class = {
  .next			= &idle_sched_class,
  .enqueue_task		= enqueue_task_dummy,
  .dequeue_task		= dequeue_task_dummy,
  .yield_task		= yield_task_dummy,
  
  .check_preempt_curr 	= check_preempt_curr_dummy,
  
  .pick_next_task	= pick_next_task_dummy,
  .put_prev_task	= put_prev_task_dummy,
  
  .set_curr_task	= set_curr_task_dummy,
  .task_tick		= task_tick_dummy,
  
  .switched_from	= switched_from_dummy,
  .switched_to		= switched_to_dummy,
  .prio_changed		= prio_changed_dummy,
  
  .get_rr_interval	= get_rr_interval_dummy,
};

