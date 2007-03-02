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
 *     QCD Multi-Threading Spin Lock Implementation (If necessary)
 *
 * Author:  
 *      Jie Chen
 *      Jefferson Lab Scientific Computing Group
 *
 * Revision History:
 *   $Log: qmt_lock.h,v $
 *   Revision 1.1  2007-03-02 19:44:55  chen
 *   Initial revision
 *
 *
 */
#ifndef _QMT_LOCK_H
#define _QMT_LOCK_H

#include <pthread.h>

#if defined (__i386)
typedef volatile int qmt_spinlock_t;
#elif defined (__x86_64) 
typedef volatile int qmt_spinlock_t;
#else
#include <pthread.h>
typedef pthread_mutex_t qmt_spinlock_t;
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern int qmt_spin_init (qmt_spinlock_t* lock);
extern int qmt_spin_lock (qmt_spinlock_t* lock);
extern int qmt_spin_unlock (qmt_spinlock_t* lock);
extern int qmt_spin_destroy (qmt_spinlock_t* lock);

extern void qmt_spin_eq_wait (int val1, volatile int *val2);

#ifdef __cplusplus
}
#endif

#endif
