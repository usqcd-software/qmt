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
 *     QCD Multi-Threading Pthread Implementation (Private Header)
 *
 * Author:  
 *      Jie Chen
 *      Jefferson Lab Scientific Computing Group
 *
 * Revision History:
 *   $Log: qmt_pthread.h,v $
 *   Revision 1.1.1.1  2007-03-02 19:44:55  chen
 *   Initial import qmt source
 *
 *
 */
#ifndef _THREAD_UTIL_H
#define _THREAD_UTIL_H

#include <pthread.h>
#include "qmt_lock.h"

#define K        1024
#define MB        K*K

/**
 * Now we are allowing only 16 threads
 */
#define MAX_THREADS 16

struct qmt_thread_data {
  int       nsthreads;
  pthread_t tid;
  int       stid;
  int       release_flag;
};

typedef struct qmt_thread_data qmt_thread_t;
typedef struct qmt_thread_data *qmt_thread_p;

/**
 * Keep track spwaned thread information
 */
typedef struct qmt_thread_info {
  int           nsthreads;
  int           ncores;
  pthread_t     tid[MAX_THREADS];
  qmt_thread_t* thptr[MAX_THREADS];
}qmt_thread_info_t;

 
/**
 * Cache line size in terms of word and byte
 * The following values are for Intel woodcrest
 */
#define W_CACHE_SIZE    32
#define B_CACHE_SIZE    128

#if !defined(_USE_SPIN_BARRIER) && !defined(_USE_LOCK_BARRIER)
 
typedef struct {
  int volatile c_cell;
  int          c_pad[W_CACHE_SIZE-1];
} qmt_cell_t;

/**
 * This defines size of cell barrier before array of cells
 * defined above.
 * If the following structure is changed, the ICELL_SZ
 * has to be changed.
 */
#define ICELL_SZ        (sizeof(int)*3+sizeof(char *))
 
struct qmt_cell_barrier {
  int               br_c_magic;             
  int volatile      br_c_release;
  char *            br_c_free_ptr;
  int               br_c_cell_cnt;
  /**
   * Pad the strcuture so that the structure occupies 
   * the cache line width 
   */
  char              br_c_pad[B_CACHE_SIZE-ICELL_SZ];
  /**
   * Array of cells each occupies a different cache line
   */
  qmt_cell_t        br_c_cells[1];
};


/**
 * Total size of memory for a barrier object 
 * In order to align the memory pointer to the cache line size,
 * we need to allocate B_CACHE_SIZE - 1 more than the following
 * size
 */
#define BR_CELL_T_SIZE(x) (sizeof(struct qmt_cell_barrier) + (sizeof(qmt_cell_t)*x))

/**
 * Barrier type 
 */
typedef struct qmt_cell_barrier * qmt_cell_barrier_t;
typedef struct qmt_cell_barrier * qmt_barrier_t;

#else

#define SISIZE (3*sizeof(int) + sizeof(char) + sizeof(qmt_spinlock_t))
struct qmt_spin_barrier {
  int volatile      counter;
  int               magic;
  int               num;
  char*             optr;
  qmt_spinlock_t    lock;
  char              pad[B_CACHE_SIZE-SISIZE];
  int volatile      release;
};
 
#define VALID_BARRIER        0x4242beef
#define INVALID_BARRIER      0xdeadbeef
 
typedef struct qmt_spin_barrier * qmt_cell_barrier_t;
typedef struct qmt_spin_barrier * qmt_barrier_t;
#endif
 
/*
 * ALIGN - to align objects on specific alignments (usually on 
 * cache line boundaries.
 *
 * arguments
 *       obj- pointer object to align
 *       alignment- alignment to align obj on
 *
 * Notes:
 *      We cast obj to a long, so that this code will work in 
 *      either narrow or wide modes of the compilers.
 */
#define ALIGN(obj, alignment)\
   ((((long) obj) + alignment - 1) & ~(alignment - 1))
 

/**
 * External routine to execute barrier synchronization
 *
 * argument: tid spawned thread id
 */
extern void qmt_sync (int tid);

#endif
