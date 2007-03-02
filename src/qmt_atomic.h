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
 *     QCD Multi-Threading Atomic Operation for Intel/AMD
 *
 * Author:  
 *      Jie Chen
 *      Jefferson Lab Scientific Computing Group
 *
 * Revision History:
 *   $Log: qmt_atomic.h,v $
 *   Revision 1.1  2007-03-02 19:44:55  chen
 *   Initial revision
 *
 *
 */
#ifndef _QMT_ATOMIC_H
#define _QMT_ATOMIC_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Add value 'val' to atomic and return the original value of atomic
 */
extern int qmt_atomic_int_exchange_and_add (volatile int *atomic, 
					    int           val);


/**
 * Compare old value and value stored at atomic, if they are the same,
 * new value will be copid to atomic, return true
 * else, return false
 */
extern int qmt_atomic_int_compare_and_exchange (volatile int *atomic, 
						int           oldval, 
						int           newval);

/**
 * Add value 'val' to atomic value
 */
extern void qmt_atomic_int_add (volatile int *atomic, 
				int           val);

/**
 * Increase atomic value by one
 */
extern void qmt_atomic_int_inc (volatile int *atomic);


/**
 * Increase atomic value by 1 and return 1 if the new value == 0
 */
extern unsigned char qmt_atomic_int_inc_and_test (volatile int *atomic);


/**
 * Decrease atomic value by 1
 */
extern void qmt_atomic_int_dec (volatile int *atomic);


/**
 * Set atomic value to 'value'
 */
extern void qmt_atomic_int_set (volatile int *atomic, int value);


/**
 * Atomically get value from atomic
 */
extern int qmt_atomic_int_get (volatile int *atomic);


#if defined (__i386)

/**
 * Decrease atomic value by 1. return 1 if the new value == 0
 */
extern unsigned char qmt_atomic_int_dec_and_test (volatile int *atomic);

#endif

#if defined (__x86_64)
/**
 * sete instruction has some problem on x86_64 platform
 * It does not change a register value to 1
 */
#define qmt_atomic_int_dec_and_test(atomic)		\
  (qmt_atomic_int_exchange_and_add ((atomic), -1) == 1)
#endif

typedef volatile int   qmt_qlock_t;

extern int qmt_qlock_init (qmt_qlock_t* lock);
extern int qmt_qlock_lock (qmt_qlock_t* lock);
extern int qmt_qlock_unlock (qmt_qlock_t* lock);
extern int qmt_qlock_destroy (qmt_qlock_t* lock);

#if defined (__x86_64) || defined (__i386)
/**
 * Intel and AMD 32bit read/write instructions are atomic
 * Check Intel Software Developer Vol 3A
 */
#define qmt_atomic_int_get(atomic) (*(atomic))
#define qmt_atomic_int_set(atomic,newval) ((void) (*(atomic) = (newval)))
#endif


#ifdef __cplusplus
}
#endif

#endif



