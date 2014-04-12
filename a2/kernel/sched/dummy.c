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

#define NR_PRIO_LEVELS (5) ///< The number of different priority levels handled by this sched class.
#define DUMMY_MIN_PRIO (131) ///< The first priority of the range of handled priorities.

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
  /* The priority queue is implemented using an 
   * array of linked lists which is based in rq->queues. */
  return &(rq->queues[prio - DUMMY_MIN_PRIO]);
}

static inline struct task_struct *dummy_task_of(struct sched_dummy_entity *dummy_se)
{
  return container_of(dummy_se, struct task_struct, dummy_se);
}

static inline void _enqueue_task_dummy(struct rq *rq, struct task_struct *p)
{
  /* First of all, one get the appropriate queue and then add the process at the end of it. */
  struct sched_dummy_entity *dummy_se = &p->dummy_se;
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
  /* Here two cases are handled:
   *   - if a process of higher priority becomes runnable; and
   *   - if a process of the same priority as the running process becomes runnable
   *     in which case the running process is preempted only if it has already 
   *     exceeded its round robin quantum.
   */
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
  
  /* The queues are browsed ordered by priority. It returns as soon as a nonempty queue is found. */
  for (i = 0; i < NR_PRIO_LEVELS; ++i) {
    struct list_head *queue = &(dummy_rq->queues[i]);
    if (!list_empty(queue)) {
      next = list_first_entry(queue, struct sched_dummy_entity, run_list);
      
      /* Imagine the following case: a process of the highest priority handled by this sched class
       * is running while a process of higher priority preempts that process and is handled by another
       * sched class. Suppose this happens frequently. To make sure that the same process does not
       * run a fraction of a round robin quantum and that the other processes in the queue gets their
       * cycle we reset the round robin counter here only when a process has complete a round robin
       * tour.
      */
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

/**
 * This routine is responsible for handling the aging and is called when the
 * the tick count of the run queue exceeds the threshold.
 */
static inline void perform_aging(struct rq *rq)
{
  struct task_struct *curr = rq->curr;
  int first_prio_to_age = (curr->prio) - DUMMY_MIN_PRIO + 1;
  
  if (first_prio_to_age < NR_PRIO_LEVELS) {
    int p;
    for (p = first_prio_to_age; p < NR_PRIO_LEVELS; ++p) {
      struct list_head *dest = &(rq->dummy.queues[p - 1]);
      struct list_head *processes_to_age = &(rq->dummy.queues[p]);
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

	processes_to_age = &(rq->dummy.queues[p]);
      }
    }
  }

}

static void task_tick_dummy(struct rq *rq, struct task_struct *curr, int queued)
{
  /* Performs round robin resched if necessary. */
  rq->curr->dummy_se.rr_tick_count += 1;
  if (rq->curr->dummy_se.rr_tick_count >= get_timeslice()) {
    int curr_prio = curr->prio;
    
    dequeue_task_dummy(rq, curr, 0);
    enqueue_task_dummy(rq, curr, 0);
    resched_task(rq->curr);
  }

  /* Performs aging if necessary. */
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
  check_preempt_curr_dummy(rq, p, 0);
}

static void prio_changed_dummy(struct rq *rq, struct task_struct *p, int oldprio)
{
  /* If the new priority is less high (in linux sense) than the old one, 
   * we resched. */
  if (oldprio > p->prio) {
    dequeue_task_dummy(rq, rq->curr, 0);
    enqueue_task_dummy(rq, rq->curr, 0);
    resched_task(rq->curr);	
  }
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

