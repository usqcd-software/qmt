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
 *     QCD Multi-Threading Spin Lock Implementation
 *
 * Author:  
 *      Jie Chen
 *      Jefferson Lab Scientific Computing Group
 *
 * Revision History:
 *   $Log: qmt_lock.c,v $
 *   Revision 1.1  2007-03-02 19:44:55  chen
 *   Initial revision
 *
 *
 */
#include <errno.h>
#include "qmt_lock.h"

#if defined (__i386)

# ifdef UP
#  define LOCK_PREFIX   /* nothing */
# else
#  define LOCK_PREFIX   "lock;"
# endif


int
qmt_spin_lock (qmt_spinlock_t *lock)
{
  asm ("\n"
       "1:\t" LOCK_PREFIX "decl %0\n\t"
       "jne 2f\n\t"
       ".subsection 1\n\t"
       ".align 16\n"
       "2:\trep; nop\n\t"
       "cmpl $0, %0\n\t"
       "jg 1b\n\t"
       "jmp 2b\n\t"
       ".previous"
       : "=m" (*lock)
       : "m" (*lock));
  return 0;
}

int
qmt_spin_destroy (qmt_spinlock_t *lock)
{
  return 0;
}

void
qmt_spin_eq_wait (int value1, volatile int *value2)
{
  __asm__ __volatile__ ("qmt_loop:\n\t"
			"cmpl %0, %1\n\t"
			"jne qmt_ct\n\t"
			"pause\n\t"
			"jmp qmt_loop\n\t"
			"qmt_ct:"
			:
			: "r" (value1), "m" (*value2));
}
#elif defined (__x86_64) 
int
qmt_spin_init (qmt_spinlock_t* lock)
{
  return pthread_spin_init (lock, PTHREAD_PROCESS_PRIVATE);
}

int
qmt_spin_lock (qmt_spinlock_t *lock)
{
  return pthread_spin_lock (lock);
}

int
qmt_spin_unlock (qmt_spinlock_t* lock)
{
  return pthread_spin_unlock (lock);
}

int
qmt_spin_destroy (qmt_spinlock_t* lock)
{
  return pthread_spin_destroy (lock);
}

void
qmt_spin_eq_wait (int value1, volatile int *value2)
{
  __asm__ __volatile__ ("qmt_loop:\n\t"
			"cmpl %0, %1\n\t"
			"jne qmt_ct\n\t"
			"pause\n\t"
			"jmp qmt_loop\n\t"
			"qmt_ct:\n\t"
			: /* no output */
			: "r" (value1), "m" (*value2));
}

#else

int
qmt_spin_init (qmt_spinlock_t* lock)
{
  return pthread_mutex_init (lock, 0);
}

int
qmt_spin_lock (qmt_spinlock_t *lock)
{
  while (pthread_mutex_trylock (lock) == EBUSY)
    ;
  return 0;
}

int
qmt_spin_unlock (qmt_spinlock_t* lock)
{
  return pthread_mutex_unlock (lock);
}

int
qmt_spin_destroy (qmt_spinlock_t* lock)
{
  return pthread_mutex_destroy (lock);
}

#endif
