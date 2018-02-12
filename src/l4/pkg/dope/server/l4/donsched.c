/*
 * \brief   DOpE donation-based scheduling module
 * \date    2004-04-28
 * \author  Norman Feske <nf2@inf.tu-dresden.de>
 *
 * This scheduler uses the scheduling extension of
 * Fiasco to perform a donation-based scheduling
 * of redraws.
 *
 * In contrast to the DOpEs simple default scheduler,
 * this scheduler supports different frame rates and
 * features a more optimistic worst-case estimation.
 */

/*
 * Copyright (C) 2002-2004  Norman Feske  <nf2@os.inf.tu-dresden.de>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the DOpE package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

/*** L4 SPECIFIC INCLUDES ***/
#include <l4/sys/syscalls.h>
#include <l4/thread/thread.h>

/*** LOCAL INCLUDES ***/
#include "dopestd.h"
#include "widget.h"
#include "thread.h"
#include "userstate.h"
#include "redraw.h"
#include "timer.h"
#include "scheduler.h"
#include "donsched-client.h"
#include "donsched-server.h"

static struct thread_services    *thread;
static struct timer_services     *timer;
static struct redraw_services    *redraw;
static struct userstate_services *userstate;

int init_don_scheduler(struct dope_services *d);

#define MAX_JOBS 64
struct job;
struct job {
	int               type;        /* type of job            */
	l4_threadid_t     don_thread;  /* time donating thread   */
	MUTEX            *sync_mutex;  /* redraw sync mutex      */
	WIDGET           *wid;         /* associated widget      */
	u32               period;      /* period in msecs        */
	u32               duration;    /* duration in usecs      */
	u32               prio;        /* priority of job        */
	CORBA_Environment env;         /* used for donation call */
	struct job       *next;        /* next pending job       */
	struct job       *prev;        /* previous pending job   */
};

static MUTEX *job_queue_mutex;
static MUTEX *pending_mutex;

static l4_threadid_t worker_thread;
static CORBA_Object  worker_tid = &worker_thread;

#define JOB_TYPE_EMPTY  0
#define JOB_TYPE_NRT    1
#define JOB_TYPE_RT     2
#define JOB_TYPE_USER   3
#define JOB_TYPE_KILLED 4
#define JOB_TYPE_UNDEF  5

static struct job jobs[MAX_JOBS];
static struct job *first;
static struct job *last;


/**********************************
 *** FUNCTIONS FOR INTERNAL USE ***
 **********************************/

//static void dump_queue(void) {
//	struct job *job = first;
//	printf("queue is ");
//	while (job) {
//		printf("[%x]", job->don_thread.id.lthread);
//		job = job->next;
//	}
//	printf("\n");
//}


/*** ALLOCATE NEW JOB SLOT ***/
static struct job *alloc_job_slot(void) {
	struct job *result = NULL;
	int i;
	thread->mutex_down(job_queue_mutex);
	CORBA_Environment env = dice_default_environment;

	/* find empty job slot */
	for (i=0; (jobs[i].type != JOB_TYPE_EMPTY) && (i<MAX_JOBS); i++);
	if ((i<MAX_JOBS) && (jobs[i].type == JOB_TYPE_EMPTY))
		result = &jobs[i];

	/* init job slot with default values */
	if (result) {
		memset(result, 0, sizeof(struct job));
		result->type = JOB_TYPE_UNDEF;
		result->next = jobs[i].prev = NULL;
		result->env  = env;
		result->prio = l4thread_get_prio(l4thread_myself());
	}
	thread->mutex_up(job_queue_mutex);
	return result;
}


/*** FIND JOB SLOT OF THE SPECIFIED WIDGET ***/
static struct job *find_job_slot(WIDGET *w) {
	int i;
	struct job *result = NULL;
	thread->mutex_down(job_queue_mutex);
	for (i=0; (!result) && (i<MAX_JOBS); i++) {
		if (jobs[i].wid == w) result = &jobs[i];
	}
	thread->mutex_up(job_queue_mutex);
	return result;
}


/*** INSERT NEW JOB INTO JOB QUEUE ***
 *
 * NOTE: We should consider job priorities here.
 */
static void queue_job(struct job *job) {

	if (job->next || job->prev)
		ERROR(printf("DonSched(queue_job): Damn! You try to queue the element twice!\n"));

	/* put job at the beginning of the fifo queue */
	thread->mutex_down(job_queue_mutex);
	job->next = first;
	job->prev = NULL;
	if (job->next) job->next->prev = job;
	first = job;
	if (!last) last = job;
	thread->mutex_up(job_queue_mutex);
}


/*** QUEUE JOB INTO JOB QUEUE AND HIRE WORKER THREAD TO DO THE DIRTY WORK ***
 *
 * This function is used by donation threads to queues a job into
 * the job queue, wake up the worker thread and donate processing
 * time to the worker thread via an ipc call.
 */
static inline void hire_worker(struct job *job) {

	/* queue job into jobqueue */
	queue_job(job);

	/* signal pending job */
	thread->mutex_up(pending_mutex);

	/* donate time to worker thread (blocking) */
	donsched_donate_call(worker_tid, &job->env);
}


/*** GET NEXT PENDING JOB ***
 *
 * \return  job structure of next pending job
 *          or NULL if there is no pending job
 */
static struct job *get_pending_job(void) {
	struct job *result = NULL;

	if (!last) {
		ERROR(printf("DonSched(get_pending_job): Damn! Queue is empty!\n"));
	}

	/* lock queue during modifications */
	thread->mutex_down(job_queue_mutex);

	/* unqueue last queue element */
	result = last;
	if (last->prev) {
		last->prev->next = NULL;
	} else {
		first = NULL;
	}
	last = last->prev;

	/* isolate result queue element */
	result->prev = result->next = NULL;

	thread->mutex_up(job_queue_mutex);
	return result;
}


/*** EXECUTE JOB ***/
static void exec_job(struct job *job) {
	WIDGET *cw;
	if (!job) return;
	
	switch (job->type) {
		case JOB_TYPE_USER:
			userstate->handle();
			break;

		case JOB_TYPE_RT:
			if ((cw = job->wid))
				cw->gen->drawarea(cw, cw, 0, 0, cw->gen->get_w(cw), cw->gen->get_h(cw));

			if (job->sync_mutex)
				thread->mutex_up(job->sync_mutex);
			break;

		case JOB_TYPE_NRT:
			redraw->exec_redraw(job->duration);
			break;
	}
}


/*** DONATION THREAD FOR PERIODIC JOBS ***
 *
 * Every job has an associated donation thread that provides the
 * processing time for the execution of the job.
 */
static void don_thread(void *arg) {
	struct job *job = (struct job *)arg;
	job->don_thread = l4_myself();
	l4thread_set_prio(l4thread_myself(), job->prio);

	printf("DonSched(don_thread): new don_thread %x, type=%d\n", job->don_thread.id.lthread, job->type);

	while (job->type != JOB_TYPE_KILLED) {
		l4thread_usleep(job->period * 1000);
		hire_worker(job);
	}
	job->type = JOB_TYPE_EMPTY;
}


/*** DONATION THREAD FOR BEST EFFORT NRT PROCESSING ***
 *
 * This thread is started to queue non-real-time redraw jobs into
 * the job queue. It is not periodic queues jobs as long as there
 * are pending redraw requests. If there are no pending requests,
 * the thread sleeps for a while. This thread is meant to run on
 * a low priority such that it is considered to be non-realtime.
 */
static void don_nrt_thread(void *arg) {
	struct job *job = (struct job *)arg;
	job->don_thread = l4_myself();
	l4thread_set_prio(l4thread_myself(), job->prio);
	while (1) {
		if (redraw->get_noque() == 0) {
			l4thread_usleep(job->period * 1000);
		} else {
			hire_worker(job);
		}
	}
}


/*************************
 *** SERVICE FUNCTIONS ***
 *************************/

/*** REGISTER NEW REAL-TIME WIDGET ***/
static s32 rt_add_widget(WIDGET *w, u32 period) {
	struct job *new_job = alloc_job_slot();
	if (!new_job) return -1;
	new_job->type   = JOB_TYPE_RT;
	new_job->period = period;
	new_job->wid    = w;
	l4thread_create(don_thread, new_job, L4THREAD_CREATE_ASYNC);
	return 0;
}


/*** UNREGISTER A REAL-TIME WIDGET ***/
static void rt_remove_widget(WIDGET *w) {
	struct job *job = find_job_slot(w);
	if (!job) return;
	job->type = JOB_TYPE_KILLED;
}


/*** UNREGISTER ALL REAL-TIME WIDGETS OF AN APPLICATION ***/
static void rt_remove_app(int app_id) {

	printf("DonScheduler(rt_remove_app): not implemented\n");
}


/*** SET MUTEX THAT SHOULD BE UNLOCKED AFTER DRAWING OPERATIONS ***/
static void rt_set_sync_mutex(WIDGET *w, MUTEX *m) {
	struct job *job = find_job_slot(w);
	if (!job) return;
	job->sync_mutex = m;
}


/*** MAINLOOP OF DOpE ***
 *
 * Within the mainloop we must do the following things:
 *
 * * Perform the redraw of real-time and non-real-time widgets.
 * * Call the userstate manager periodically.
 */
static void process_mainloop(void) {
	CORBA_Object don_tid;
	CORBA_Server_Environment env = dice_default_server_environment;
	donsched_msg_buffer_t msg_buf;
	l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);
	struct job *curr_job = NULL;

	job_queue_mutex = thread->create_mutex(0);
	pending_mutex   = thread->create_mutex(1);

	worker_thread = l4_myself();

	/* create periodic donation thread for user activity */
	{
		struct job *new_job = alloc_job_slot();
		if (!new_job) return;
		new_job->period   = 10;
		new_job->type     = JOB_TYPE_USER;
		l4thread_create(don_thread, new_job, L4THREAD_CREATE_ASYNC);
	}
	
	/* create best-effort donation thread for non-realtime redraws */
	{
		struct job *new_job = alloc_job_slot();
		if (!new_job) return;
		new_job->period   = 1;
		new_job->duration = 1*1000;
		new_job->type     = JOB_TYPE_NRT;
		new_job->prio     = 0x10;
		l4thread_create(don_nrt_thread, new_job, L4THREAD_CREATE_ASYNC);
	}

	/* make the priority of the worker thread lower than donation threads */
	l4thread_set_prio(l4thread_myself(), l4thread_get_prio(l4thread_myself()) - 1);

	while (1) {

		/* wait for a pending job */
		thread->mutex_down(pending_mutex);

		curr_job = get_pending_job();
		don_tid  = &(curr_job->don_thread);

		/* receive some time from donating thread */
		donsched_srv_recv_any(don_tid, &tag, &msg_buf, &env);

		/* use precious time to do something useful */
		exec_job(curr_job);

		/* confirm finished job to donation thread and wait for new job */
		donsched_donate_reply(don_tid, &env);
	}
}


/****************************************
 *** SERVICE STRUCTURE OF THIS MODULE ***
 ****************************************/

static struct scheduler_services services = {
	rt_add_widget,
	rt_remove_widget,
	rt_remove_app,
	rt_set_sync_mutex,
	process_mainloop,
};


/**************************
 *** MODULE ENTRY POINT ***
 **************************/

int init_don_scheduler(struct dope_services *d) {
	thread    = d->get_module("Thread 1.0");
	userstate = d->get_module("UserState 1.0");
	redraw    = d->get_module("RedrawManager 1.0");
	timer     = d->get_module("Timer 1.0");

	d->register_module("Scheduler 1.0", &services);
	return 1;
}
