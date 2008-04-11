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
 *     QCD Multi-Threading Public Header File
 *
 * Author:  
 *      Jie Chen
 *      Jefferson Lab Scientific Computing Group
 *
 * Revision History:
 *   $Log: qmt.h,v $
 *   Revision 1.3  2008-04-11 14:46:36  chen
 *   remove OMP_NUM_THREADS env
 *
 *   Revision 1.2  2007/10/05 04:52:50  edwards
 *   Added some doxygen starting info.
 *
 *   Revision 1.1.1.1  2007/03/02 19:44:55  chen
 *   Initial import qmt source
 *
 *
 */

/*! \mainpage  QMT
 *
 * \section Description
 *
 * QMT is a software library providing OpenMP like fork-join multi-thread APIs.
 * The current implementation works on i386 and x86-64 using pthread and other
 * optimizations.
 */

#ifndef _QMT_H
#define _QMT_H

#ifdef _QMT_PUBLIC
/**
 * General User provided function to execute on a thread
 *
 * User can retrieve thread id through the second argument 
 */
typedef void (*qmt_userfunc_t) (void *usrarg, int thid);

#else

/**
 * User provided LQCD function to be executed by a thread
 */
typedef void (*qmt_userfunc_t) (unsigned int lo, 
				unsigned int  hi, 
				int thid, 
				void *ptr);
#endif


#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Initialize qmt multi-threading package. This routine must be called
 * before any other qmt_routine
 *
 * @return 0 if the qmt system is initialize correctly
 */
extern int qmt_init (void);

/**
 * Finalize the qmt multi-threading library. Release system memory
 * and other resources. Any qmt routines after this routine will have
 * undefined behaviour.
 *
 * @return 0 if the qmt system is shutdown correctly
 */
extern int qmt_finalize (void);


#ifdef _QMT_PUBLIC
/**
 * Parallel execution of user supplied function on multiple
 * threads. The number of threads is defined through the following order
 * QMT_NUM_THREADS
 * Otherwise, the number of threads will be the number of processing cores.
 *
 * This function is a synchronized call. This function will block until
 * all threads finish executing func.
 *
 * @param func a user supplied function pointer
 * @param arg  a void pointer a user can pass anything into the above func
 * @return 0 if the user function has been executed on all threads
 */
extern int qmt_pexec (qmt_userfunc_t func, void* arg);

#else

/**
 * Parallel execution of user supplied lattice QCD code on multiple
 * threads. The number of threads is defined through the following order
 * QMT_NUM_THREADS
 * Otherwise, the number of threads will be the number of processing cores.
 *
 * This function is a synchronized call. This function will block until
 * all threads finish executing func.
 *
 * @param func a user supplied lqcd function pointer
 * @param count a subvolume (number of lattice sites) on this node
 * @param arg   a void pointer a user can pass anything
 * @return 0 if the user function has been executed on all threads
 *
 */
extern int qmt_call (qmt_userfunc_t func, unsigned int count, void* arg);

#endif


/**
 * Caller can check its own thread id
 *
 * @return a thread id
 */
extern int  qmt_thread_id (void);

/**
 * Number of threads in use
 *
 * @return number of threads
 */
extern int qmt_num_threads(void);



#ifdef __cplusplus
}
#endif


#endif

