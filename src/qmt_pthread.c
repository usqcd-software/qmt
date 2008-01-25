/*----------------------------------------------------------------------------
 * Copyright (c) 2007      Jefferson Science Associates, LLC 
 *                         Under U.S. DOE Contract No. DE-AC05-06OR23177
 *                         
 *                         Thomas Jefferson National Accelerator Facility
 *
 *
 *                         Jefferson Lab 
 *                         Scientific Computing Group, 
 *                         12000 Jefferson Ave., 
 *                         Newport News, VA 23606
 *----------------------------------------------------------------------------
 *
 * Description:
 *     QCD Multi-Threading Pthread Implementation
 *
 * Author:  
 *      Jie Chen
 *      Jefferson Lab Scientific Computing Group
 *
 * Revision History:
 *   $Log: qmt_pthread.c,v $
 *   Revision 1.3  2008-01-25 18:53:54  chen
 *   Remove unneccessary cache write for queue-based barrier
 *
 *   Revision 1.2  2007/05/08 16:57:25  chen
 *   add configuration flag to do affinity
 *
 *   Revision 1.1.1.1  2007/03/02 19:44:55  chen
 *   Initial import qmt source
 *
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#ifdef __linux

#ifndef __USE_GNU
#define __USE_GNU
#include <sched.h>
#endif

#endif

#include <pthread.h>

#include "qmt.h"
#include "qmt_pthread.h" 
#include "qmt_lock.h"

#ifdef _USE_PAPI
#include <papi.h>
#include <errno.h>
#endif

 
/**
 * The following are some static variables
 * The order of these variables are actually important
 * for performance. 
 *
 * This is caused by cache line issue
 */
static qmt_spinlock_t       qmt_slock;
static int                  dumb_pad[W_CACHE_SIZE];
static int volatile         qmt_idle_release_flag = 0; 

static qmt_userfunc_t       user_func;
static void                 *user_arg;
static unsigned int         lqcd_count = 0;


static int                  qmt_threads_are_init = 0;
static int volatile         qmt_threads_are_parallel = 0;

static qmt_cell_barrier_t   my_barrier;


/* thread private specific data: master thread only*/ 
static qmt_thread_p master_thread_ptr = 0;


/* thread information data shared by every threads */
static qmt_thread_info_t my_thread_info;

/**
 * How do thread number mapped to core number
 * master thread always to core 0
 */
static int
qmt_thread_num_to_core (int tnum, int numcores)
{
  return tnum;
}

/**
 * Check whether a line is an empty line
 */
static int
qmt_empty_line (char line[], int size)
{
  char* p = 0;

  p = line;

  while (p && *p) {
    if (isprint(*p))
      return 0;
    p++;
  }
  return 1;
}



/**
 * Simple hack to get number of cpus (cores) for a machine
 */
static int
qmt_get_num_cpus (void)
{
#ifdef __linux
  FILE* fd;
  char  line[80];
  int   numcpu = 0;

  fd = fopen ("/proc/cpuinfo", "r");
  if (!fd) {
    fprintf (stderr, "cannot open /proc/cpuinfo file.");
    return EPERM;
  }

#if defined (__i386) || defined (__x86_64)
  while (!feof (fd)) {
    memset (line, 0, sizeof (line));
    if (fgets (line, sizeof (line) - 1, fd) &&
	!qmt_empty_line (line, sizeof (line) - 1)) {
      if (strstr (line, "processor"))
	numcpu++;
    }
  }
#endif

    fclose (fd);
    return numcpu;

#else

# error qmt_get_num_cpus () is not implemented for this archtecture.

#endif
}

/**
 * qmt_thread_id
 *   Return thread spawn thread id. This will be in the range 
 *   of 0 to N-1, where N is the number of threads in the 
 *   process.
 * arguments:
 *   regular pthread id
 *
 * return
 *   spawn thread id
 */
 
int
qmt_thread_number (pthread_t tid)
{
  int i;

  for (i = 0; i < my_thread_info.nsthreads; i++) 
    if (pthread_equal(tid, my_thread_info.tid[i]) != 0)
      return i;
  fprintf (stderr, "Cannot find thread id for this thread, error\n");
  return -1;
}

/**
 * Return which cores is bound to a particualr thread
 * 
 * This part of code is not really portable
 * It is available inside NPTL
 *
 */
static int
qmt_core_for_thread (pthread_t tid)
{
#if defined(__linux) 

  int i, ret, thread_num;
  cpu_set_t cpu_mask;

  /**
   * Find thread number for this thread id
   */
  thread_num = qmt_thread_number (tid);

  CPU_ZERO (&cpu_mask);

  ret = pthread_getaffinity_np (tid, sizeof(cpu_mask), &cpu_mask);
  if (ret != 0) {
    fprintf (stderr, "Thread %d is not bounded to a core .\n",
	     thread_num);
  }
  for (i = 0; i < my_thread_info.ncores; i++) {
    if (CPU_ISSET (i, &cpu_mask))
      return i;
  }
  return ret;
#else
# error qmt_core_for_thread is not implemented for this OS
#endif
}

/**
 * Bind a thread to a particular core
 * 
 * This part of code is not really portable
 * It is available inside NPTL
 *
 */
static int
qmt_bind_thread_to_core (pthread_t tid)
{
#if defined(__linux) 

  int ret, thread_num, core_num;
  cpu_set_t cpu_mask;

  /**
   * Find thread number for this thread id
   */
  thread_num = qmt_thread_number (tid);
  
  /**
   * Calculate which core this thread should be bound to
   */
  core_num = qmt_thread_num_to_core (thread_num, my_thread_info.ncores);

  CPU_ZERO (&cpu_mask);
  CPU_SET (core_num, &cpu_mask);

  ret = pthread_setaffinity_np (tid, sizeof(cpu_mask), &cpu_mask);
  if (ret != 0) {
    fprintf (stderr, "Thread %d cannot be bounded to core %d.\n",
	     qmt_thread_number (tid), core_num);
  }
#ifdef _QMT_DEBUG 
  fprintf (stderr, "Thread %d is bound to core %d\n",
	   thread_num, qmt_core_for_thread (tid));
#endif
  return ret;

#else
#error qmt_bind_thread_to_core is not implemented for this OS
#endif
}

#if !defined (_USE_SPIN_BARRIER) && !defined (_USE_LOCK_BARRIER)
/**
 * qmt_barrier_alloc
 *   Allocate cell barrier data structure based upon the 
 *   number of threads that are in the current process.
 *
 * arguments
 *   brc  - pointer pointer to the user cell barrier
 *   n    - number of threads that will use this barrier
 *
 * return
 *   0- success
 *   -1- failed to allocate cell barrier
 */
static int
qmt_barrier_alloc(qmt_cell_barrier_t *brc, int n)
{
  qmt_cell_barrier_t b;
  char *p;
  int i;
 
  /**
   * Allocate cell barrier for "n" threads
   */
  if ((p = (char *) malloc(BR_CELL_T_SIZE(n) + B_CACHE_SIZE - 1)) == 0 )
    return -1;
 
  /**
   * Align the barrier on a cache line for maximum 
   * performance.
   */
   
   b = (qmt_cell_barrier_t) ALIGN(p, B_CACHE_SIZE);
   b->br_c_magic = 0x4200beef;
   b->br_c_cell_cnt = n;   /* keep track of the # of threads */
   b->br_c_release = 0;    /* initialize release flag */
   b->br_c_free_ptr = p;   /* keep track of orginal malloc ptr */

   for(i = 0; i < n; i++ ) 
     b->br_c_cells[i].c_cell = 0;/* zero the cell flags */

   /**
    * allocate local array to store cell flag for master thread
    */
   b->br_lflags_free_ptr = (int *)malloc(n * sizeof(int) + B_CACHE_SIZE - 1);
   if (!b->br_lflags_free_ptr)
     return -1;

   /**
    * Align the array on a cache line
    */
   b->br_lflags = (int *)ALIGN(b->br_lflags_free_ptr, B_CACHE_SIZE);

   /**
    * Make the local flags the same value as the cell flag
    */
   for (i = 0; i < n; i++)
     b->br_lflags[i] = b->br_c_cells[i].c_cell;

   *brc = b;
 
   return 0;
}

/**
 * qmt_barrier_sync
 *   Provide a barrier for all threads to sync up at, after 
 *   they have finished performing parallel work.
 *
 * arguments
 *    b       - pointer to cell barrier
 *    id      - id of the thread (need to be in the 
 *              range of 0 - (N-1), where N is the number of threads).
 *
 * return: none
 */
 
void
qmt_barrier_sync (qmt_cell_barrier_t b, int id)
{
  int i, key;
 
  /*
   * Get the release flag value, before we signal that we 
   * are at the barrier.
   */
  key = b->br_c_release;
 
  if ( id == 0 ) {
    /*
     * make thread 0 (i.e. parent thread) wait for the child
     * threads to show up.
     */
    for( i = 1; i < b->br_c_cell_cnt; i++ ) {
      /**
       * wait on the Nth cell
       */
      while (b->br_c_cells[i].c_cell == b->br_lflags[i])
	/* spin */;

       /**
	* For the master thread, this is just a 
	* simple cache read. No cache write involved
	*/
       b->br_lflags[i] = b->br_c_cells[i].c_cell;
    }
 
    /**
     * signal all of the child threads to leave the barrier.
     */
    ++b->br_c_release;
  } 
  else {
    /*
     * signal that the Nth thread has arrived at the barrier.
     */
    ++(b->br_c_cells[id].c_cell);

    while ( key == b->br_c_release ) 
      /* spin */;
  }
}

static void
qmt_barrier_destroy (qmt_cell_barrier_t br)
{
  free (br->br_lflags_free_ptr);
  free (br->br_c_free_ptr);
}
#elif defined (_USE_SPIN_BARRIER)

#include "qmt_atomic.h"

static int
qmt_barrier_alloc(qmt_cell_barrier_t *brc, int num)
{
  char *p;
  qmt_barrier_t b, n;
 
  b = (qmt_cell_barrier_t) *brc;
 
  if ( b != NULL ) 
    return EINVAL;
  
  p = (char *)malloc(sizeof(struct qmt_spin_barrier) + B_CACHE_SIZE - 1);
  if (!p) {
    fprintf (stderr, "Cannot allocate space to spin barrier.\n");
    return ENOMEM;
  }

  n = (qmt_barrier_t)ALIGN(p, B_CACHE_SIZE);
  n->optr = p;
  n->magic = VALID_BARRIER;
  n->counter = num;
  n->num = num;
  n->release = 0;

  *brc = n;
 
  return 0;
}

static int
qmt_barrier_sync (qmt_cell_barrier_t br, int id)
{
  int rv;
  int key;
 
  if ( br == NULL || br->magic != VALID_BARRIER )
    return EINVAL;
 
  /**
   * Get release flag value. This value cannot be changed 
   * since the counter will not be zero until this thread
   * decrease the counter value
   */
  key = br->release;  /* get release flag */

  /* Issue atomic dec instruction */
  qmt_atomic_int_dec (&br->counter);

  /* Get current counter value */
  /* if this counter is update by another thread
   * we are still ok, since we are just executing
   * the same instructions twice
   */
  rv = qmt_atomic_int_get (&br->counter);

 
  /*
   * See if we are the last thread into the barrier
   */
  if ( rv == 0 ) {
    /*
     * We are the last thread, so clear the counter
     * and signal the other threads by changing the
     * release flag.
     */
    qmt_atomic_int_set (&br->counter, br->num);
    qmt_atomic_int_inc (&br->release); 

  } else {
 
    /*
     * We are not the last thread, so wait 
     * until the release flag changes.
     */
    while( key == br->release ) 
      ;
  }
  return 0;
}


static void
qmt_barrier_destroy (qmt_cell_barrier_t br)
{
  qmt_spin_destroy (&br->lock);
  free (br->optr);
}

#else
static int
qmt_barrier_alloc(qmt_cell_barrier_t *brc, int num)
{
  char *p;
  qmt_barrier_t b, n;
  int rv;
 
  b = (qmt_cell_barrier_t) *brc;
 
  if ( b != NULL ) 
    return EINVAL;
  
  p = (char *)malloc(sizeof(struct qmt_spin_barrier) + B_CACHE_SIZE - 1);
  if (!p) {
    fprintf (stderr, "Cannot allocate space to spin barrier.\n");
    return ENOMEM;
  }

  n = (qmt_barrier_t)ALIGN(p, B_CACHE_SIZE);
  n->optr = p;
  n->magic = VALID_BARRIER;
  n->counter = num;
  n->num = num;
  n->release = 0;

  if ( (rv = qmt_spin_init(&n->lock)) != 0 )
    return rv;
 
  *brc = n;
 
  return 0;
}

static int
qmt_barrier_sync (qmt_cell_barrier_t br, int id)
{
  int rv;
  int key;
 
  if ( br == NULL || br->magic != VALID_BARRIER )
    return EINVAL;
 
  qmt_spin_lock (&br->lock); 
  key = br->release;  /* get release flag */

  /* fetch and dec shared counter */  
  rv = --br->counter; 

  /*
   * See if we are the last thread into the barrier
   */
  if ( rv == 0 ) {
    /*
     * We are the last thread, so clear the counter
     * and signal the other threads by changing the
     * release flag.
     */
    br->counter = br->num;
    ++br->release; 
    qmt_spin_unlock(&br->lock);

  } else {
    qmt_spin_unlock(&br->lock); 
 
    /*
     * We are not the last thread, so wait 
     * until the release flag changes.
     */
    while( key == br->release ) 
      ;
  }
  return 0;
}


static void
qmt_barrier_destroy (qmt_cell_barrier_t br)
{
  qmt_spin_destroy (&br->lock);
  free (br->optr);
}
#endif


#ifdef _QMT_PUBLIC
/**
 * Thread exit function
 */
static void qmt_thread_exit (void* arg, int stid)
{
  qmt_barrier_sync (my_barrier, stid);
  pthread_exit (arg);
}

#else

static void qmt_thread_exit (unsigned int lo, unsigned int hi, int stid, 
			     void* arg)
{
  qmt_barrier_sync (my_barrier, stid);
  pthread_exit (arg);
}

#endif 
/*
 * qmt_idle_threads
 *   All of the process child threads will execute this 
 *   code. It is the idle loop where the child threads spin wait 
 *   for parallel work.
 * arguments
 *   thr- thread pointer
 *
 * algorithm:
 *   Initialize some thread specific data structures.
 *   Loop forever on the following:
 *      Wait until we have work.
 *      Get global values on what work needs to be done.
 *      Call user specified function with argument.
 *      Call barrier code to sync up all threads.
 */
static void
qmt_idle_threads (qmt_thread_p thr)
{
#ifdef _BIND_CORE
  qmt_bind_thread_to_core (pthread_self () );
#endif

#ifdef _USE_PAPI
  if (PAPI_register_thread () != PAPI_OK) {
    fprintf (stderr, "Cannot register thread %d\n", thr->stid);
  }

#if 0
  fprintf (stderr, "Thread %d has papi thread id %u\n",
	   thr->stid, PAPI_thread_id());
#endif

#endif

  for(;;) {
    /*
     * threads spin here waiting for work to be assign
     * to them.
     */
    while(thr->release_flag == qmt_idle_release_flag ) 
      /* spin until idle_release_flag changes */
      ;

    thr->release_flag = qmt_idle_release_flag;
 
    /*
     * call user function with their specified argument.
     */
#ifdef _QMT_PUBLIC
    (*user_func)(user_arg, thr->stid);
#else
    (*user_func)(lqcd_count * thr->stid/thr->nsthreads,
		 lqcd_count * (thr->stid + 1)/thr->nsthreads,
		 thr->stid, user_arg);
#endif

    /*
     * make all threads join before they were to the idle
     * loop.
     */
    qmt_barrier_sync (my_barrier, thr->stid); 
  }
}

/** create_threads
 *   This routine creates all of the MY THREADS package data 
 *   structures and child threads.
 *
 *   arguments:
 *     none
 *
 *   return:
 *     none
 *
 * algorithm:
 *   Allocate data structures for a thread
 *   Create the thread via the pthread_create call.
 *   If the create call is successful, repeat until the 
 *   number of threads equal the number of processors.
 *
 */
 
static void
qmt_create_threads (void)
{
  char *env_val;
  int i, rv, nthreads;
  qmt_thread_p thr;
 
  /**
   * First get number of core of the system
   */
  my_thread_info.ncores = qmt_get_num_cpus ();

  /* Get number of threads from an environment variable */
  if ((env_val = getenv("OMP_NUM_THREADS")) != NULL  ||
      (env_val = getenv("QMT_NUM_THREADS")) != NULL) {
    int val;
    val = atoi(env_val);
    if ( val >= 1 && val <= MAX_THREADS)
      nthreads = val;
    else {
      fprintf (stderr, "Number of threads %d is incorrect\n", val); 
      exit (1);
    }
  }
  else 
    /* create number of threads equals to the number of CPU (core)s */
    nthreads = my_thread_info.ncores;

  /**
   * Initialize thread information
   */
  my_thread_info.nsthreads = nthreads;
  for (i = 0; i < my_thread_info.nsthreads; i++) {
    my_thread_info.tid[i] = 0;
    my_thread_info.thptr[i] = 0;
  }

  /*
   * allocate and initialize thread data structure for the master thread
   */
  if ((thr = (qmt_thread_p) malloc(sizeof(qmt_thread_t))) == NULL ) {
    fprintf (stderr, "Cannot allocate thread specific data\n");
    exit (1);
  }
  thr->nsthreads = nthreads;
  thr->stid = 0;
  thr->release_flag = qmt_idle_release_flag;
  master_thread_ptr = thr;

  /**
   * Set the first one the the parent thread
   */
  my_thread_info.tid[0] = pthread_self ();
  my_thread_info.thptr[0] = thr;

#ifdef _BIND_CORE
  qmt_bind_thread_to_core (pthread_self());
#endif
 
  for(i = 1; i < nthreads; i++ ) {
    /*
     * allocate and initialize thread data structure.
     */
    if ((thr = (qmt_thread_p) malloc(sizeof(qmt_thread_t))) == NULL ) {
      fprintf (stderr, "Cannot allocate thread specific data\n");
      exit (1);
    }
      
    /**
     * Set data structure for thread to be created
     */
    thr->nsthreads = nthreads;
    thr->stid = i;
    thr->release_flag = qmt_idle_release_flag;
    rv = pthread_create(&my_thread_info.tid[i], 0, 
			(void *(*)(void *))qmt_idle_threads, (void *) thr);
    if ( rv != 0 ) {
      free(thr);
      fprintf (stderr, "Cannot spawn a thread\n");
      exit (1);
    }
    /**
     * Keep track the thread pointer
     */
    my_thread_info.thptr[i] = thr;
  }
 
  qmt_threads_are_init = 1;
 
  qmt_barrier_alloc(&my_barrier, nthreads);
 
}

/**
 * Routine to initialize qmt package
 */
int
qmt_init (void)
{
  if (qmt_threads_are_init)
    return EINVAL;

  qmt_spin_init (&qmt_slock);

  /*
   * create the child threads, if they are not already created.
   */
  if ( !qmt_threads_are_init )
    qmt_create_threads ();
  
  return 0;
}

/**
 * Routine to finalize qmt package
 */ 
int
qmt_finalize (void)
{
  int rv, i;
 
  /*
   * if my_threads_are_init is set, then we are parallel,
   * otherwise we not.
   */
  qmt_spin_lock(&qmt_slock);
  rv = qmt_threads_are_parallel;
  qmt_spin_unlock(&qmt_slock);
  
  if (rv)
    return EINVAL;

  qmt_spin_lock(&qmt_slock);
  user_func = qmt_thread_exit;
  user_arg = 0;
  lqcd_count = 0;

  /*
   * signal all of the child threads to exit the spin loop
   */
  ++qmt_idle_release_flag;
 
  qmt_spin_unlock(&qmt_slock);

  /*
   * call join to make sure all of the threads are done doing
   * there work.
   */
  qmt_barrier_sync (my_barrier, 0);

  /**
   * Now it is time to free thread pointers
   */
  for (i = 0; i < my_thread_info.nsthreads; i++) 
    free (my_thread_info.thptr[i]);

  my_thread_info.nsthreads = 0;
  
  qmt_threads_are_init = 0;

  /**
   * Destroy barrier
   */
  qmt_barrier_destroy (my_barrier);

  return 0;
}


#ifdef _QMT_PUBLIC
/*
 * qmt_pexec
 *   Call and execute user specified routine in parallel.
 *
 * arguments:
 *   func- user specified function to call
 *   arg- user specified argument to pass to func
 *
 * return:
 *   0- success
 *   -1- error
 *
 * algorithm:
 *   If we are already parallel, then return with an error 
 *   code. Allocate threads and internal data structures, 
 *   if this is the first call.
 *   Determine how many threads we need.
 *   Set global variables.
 *   Signal the child threads that they have parallel work. 
 *   At this point we signal all of the child threads and 
 *   let them determine if they need to take part in the 
 *   parallel call. Call the user specified function.
 *   Barrier call will sync up all threads.
 */
 
int
qmt_pexec (qmt_userfunc_t func, void *arg)
{
  /*
   * check for error conditions
   */
  if (func == NULL )
    return EINVAL;

  if ( qmt_threads_are_parallel )
    return EAGAIN;
  
  qmt_spin_lock(&qmt_slock);
  if ( qmt_threads_are_parallel ) {
    qmt_spin_unlock(&qmt_slock);
    return EAGAIN;
  }

  ++qmt_threads_are_parallel;

  /*
   * set global variables to communicate to child threads.
   */
  user_func = func;
  user_arg = arg;
 
  /*
   * signal all of the child threads to exit the spin loop
   */
  ++qmt_idle_release_flag;


  qmt_spin_unlock(&qmt_slock); 
 
  /*
   * call user func with user specified argument
   */
  (*user_func)(user_arg, master_thread_ptr->stid);
 
  /*
   * call join to make sure all of the threads are done doing
   * there work.
   */
  qmt_barrier_sync (my_barrier, 0); 

  /*
   * reset the parallel flag
   */
  qmt_spin_lock(&qmt_slock);
  qmt_threads_are_parallel = 0;
  qmt_spin_unlock(&qmt_slock);
 
  return 0;
}

#else

/*
 * qmt_call
 *   Call and execute user specified lqcd routine in parallel.
 *
 * arguments:
 *   func- user specified function to call
 *   count - lqcd subvolume on this node
 *   arg- user specified argument to pass to func
 *
 * return:
 *   0- success
 *
 *
 * algorithm:
 *   If we are already parallel, then return with an error 
 *   code. Allocate threads and internal data structures, 
 *   if this is the first call.
 *   Determine how many threads we need.
 *   Set global variables.
 *   Signal the child threads that they have parallel work. 
 *   At this point we signal all of the child threads and 
 *   let them determine if they need to take part in the 
 *   parallel call. Call the user specified function.
 *   Barrier call will sync up all threads.
 */
 
int
qmt_call (qmt_userfunc_t func, unsigned int count, void *arg)
{
  /*
   * check for error conditions
   */
  if (func == NULL )
    return EINVAL;

  if ( qmt_threads_are_parallel )
    return EAGAIN;
  
  qmt_spin_lock(&qmt_slock);
  if ( qmt_threads_are_parallel ) {
    qmt_spin_unlock(&qmt_slock);
    return EAGAIN;
  }

  ++qmt_threads_are_parallel;

  /*
   * set global variables to communicate to child threads.
   */
  user_func = func;
  user_arg = arg;
  lqcd_count = count;
 
  /*
   * signal all of the child threads to exit the spin loop
   */
  ++qmt_idle_release_flag;


  qmt_spin_unlock(&qmt_slock); 
 
  /*
   * call user func with user specified argument
   */
  (*user_func)(0, lqcd_count/master_thread_ptr->nsthreads,
	       master_thread_ptr->stid, user_arg);
 
  /*
   * call join to make sure all of the threads are done doing
   * there work.
   */
  qmt_barrier_sync (my_barrier, 0); 

  /*
   * reset the parallel flag
   */
  qmt_spin_lock(&qmt_slock);
  qmt_threads_are_parallel = 0;
  qmt_spin_unlock(&qmt_slock);
 
  return 0;
}
#endif


/**
 * qmt_thread_id
 *   Return thread spawn thread id. This will be in the range 
 *   of 0 to N-1, where N is the number of threads in the 
 *   process.
 * arguments:
 *   none
 *
 * return
 *   spawn thread id
 */
 
int
qmt_thread_id (void)
{
  int i;
  pthread_t tid = pthread_self ();

  for (i = 0; i < my_thread_info.nsthreads; i++)
    if (pthread_equal(tid, my_thread_info.tid[i]) != 0)
      return i;
  fprintf (stderr, "Cannot find thread id for this thread, error\n");
  return -1;
}
 
/*
 * qmt_num_threads
 *   Return the number of threads in the current spawn.
 *
 * arguments:
 *   none
 *
 * return
 *   number of threads in the current spawn
 */
 
int
qmt_num_threads(void)
{
  return my_thread_info.nsthreads;
}
 
/*
 * qmt_in_parallel
 *   Return the is parallel flag
 *
 * arguments:
 *   none
 *
 * return
 *   1- if we are parallel
 *   0- otherwise
 */
 
int
qmt_in_parallel(void)
{
  int rv;
 
  /*
   * if my_threads_are_init is set, then we are parallel,
   * otherwise we not.
   */
  qmt_spin_lock(&qmt_slock);
  rv = qmt_threads_are_parallel;
  qmt_spin_unlock(&qmt_slock);
  
  return rv;
}

/**
 * External routine to do a barrier synchronization
 */
void
qmt_sync (int sid)
{
  qmt_barrier_sync (my_barrier, sid);
}

