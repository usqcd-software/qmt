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
 *     QCD Multi-Threading Atomic Instructions for x86 machines
 *
 * Author:  
 *      Jie Chen
 *      Jefferson Lab Scientific Computing Group
 *
 * Revision History:
 *   $Log: qmt_atomic.c,v $
 *   Revision 1.1.1.1  2007-03-02 19:44:55  chen
 *   Initial import qmt source
 *
 *
 */
#if defined (__i386)
int
qmt_atomic_int_exchange_and_add (volatile int *atomic, 
				 int           val)
{
  int result;

  __asm__ __volatile__ ("lock; xaddl %0,%1"
                        : "=r" (result), "=m" (*atomic) 
			: "0" (val), "m" (*atomic));
  return result;
}

int
qmt_atomic_int_compare_and_exchange (volatile int *atomic, 
				     int           oldval, 
				     int           newval)
{
  int result;
 
  __asm__ __volatile__ ("lock; cmpxchgl %2, %1"
			: "=a" (result), "=m" (*atomic)
			: "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}



void
qmt_atomic_int_add (volatile int *atomic, 
		    int           val)
{
  __asm__ __volatile__ ("lock; addl %1,%0"
			: "=m" (*atomic) 
			: "ir" (val), "m" (*atomic));
}


void
qmt_atomic_int_inc (volatile int *atomic)
{
  __asm__ __volatile__ ("lock; incl %0"
			: "=m" (*atomic)
			: "m" (*atomic));
}

void
qmt_atomic_float_inc (volatile float* atomic)
{
  __asm__ __volatile__ ("fld %0; fld1; faddp; fstp %0"
			: "=m" (*atomic)
			: "m" (*atomic));
}

unsigned char
qmt_atomic_int_inc_and_test (volatile int *atomic)
{
  unsigned char result;

  __asm__ __volatile__ ("lock; incl %0; sete %1"
			: "=m" (*atomic), "=qm" (result)
			: "m" (*atomic));	
  return result;
}

void
qmt_atomic_int_dec (volatile int *atomic)
{
  __asm__ __volatile__ ("lock; decl %0"
			: "=m" (*atomic)
			: "m" (*atomic));	
}

unsigned char
qmt_atomic_int_dec_and_test (volatile int *atomic)
{
  unsigned char result;

  __asm__ __volatile__ ("lock; decl %0; sete %1"
			: "=m" (*atomic), "=qm" (result)
			: "m" (*atomic));	
  return result;
}

inline void
qmt_atomic_int_set (volatile int *atomic, int value)
{
  *(atomic) = value;
}

inline int
qmt_atomic_int_get (volatile int *atomic)
{
  return (*(atomic));
}

#elif defined (__x86_64)
int
qmt_atomic_int_exchange_and_add (volatile int *atomic,
				 int           val)
{
  int result;

  __asm__ __volatile__ ("lock; xaddl %0,%1"
                        : "=r" (result), "=m" (*atomic) 
			: "0" (val), "m" (*atomic));
  return result;
}
 
void
qmt_atomic_int_add (volatile int *atomic, 
		    int           val)
{
  __asm__ __volatile__ ("lock; addl %1,%0"
			: "=m" (*atomic) 
			: "ir" (val), "m" (*atomic));
}

int
qmt_atomic_int_compare_and_exchange (volatile int *atomic, 
				     int           oldval, 
				     int           newval)
{
  int result;
 
  __asm__ __volatile__ ("lock; cmpxchgl %2, %1"
			: "=a" (result), "=m" (*atomic)
			: "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

void
qmt_atomic_float_inc (volatile float* atomic)
{
  __asm__ __volatile__ ("fld %0; fld1; faddp; fstp %0"
			: "=m" (*atomic)
			: "m" (*atomic));
}

void
qmt_atomic_int_inc (volatile int *atomic)
{
  __asm__ __volatile__ ("lock; incl %0"
			: "=m" (*atomic)
			: "m" (*atomic));
}


unsigned char
qmt_atomic_int_inc_and_test (volatile int *atomic)
{
  unsigned char result;

  __asm__ __volatile__ ("lock; incl %0; sete %1"
			: "=m" (*atomic), "=qm" (result)
			: "m" (*atomic));
  return result;
}

void
qmt_atomic_int_dec (volatile int *atomic)
{
   __asm__ __volatile__ ("lock; decl %0"
			 : "=m" (*atomic)
			 : "m" (*atomic));
}

#if 0
unsigned char
qmt_atomic_int_dec_and_test(volatile int *atomic)
{
  unsigend char result;

  __asm__ __volatile__ ("lock; decl %0; sete %1"
			: "=m" (*atomic), "=qm" (result)
			: "m" (*atomic));	 
}
#endif


inline void
qmt_atomic_int_set (volatile int *atomic, int value)
{
  *(atomic) = value;
}

inline int
qmt_atomic_int_get (volatile int *atomic)
{
  return (*(atomic));
}

#else

#error atomic operations have not been implemented

#endif
#include "qmt_atomic.h"

int
qmt_qlock_init (qmt_qlock_t* lock)
{
  *(lock)= 0;
  return 0;
}

int
qmt_qlock_lock (qmt_qlock_t* lock)
{
  while (!qmt_atomic_int_compare_and_exchange (lock, 0, 1))
    ;
  return 0;
}

int
qmt_qlock_unlock (qmt_qlock_t* lock)
{
  *(lock) = 0;
  return 0;
}

int
qmt_qlock_destroy (qmt_qlock_t* lock)
{
  return 0;
}

#ifdef _ATOMIC_TEST
#include <stdio.h>
#include <pthread.h>
#include "qmt_lock.h"
#include "qmt_atomic.h"

int
main (int argc, char** argv)
{
  volatile int number;
  int value, oldval, i;
  unsigned char res = 12;
  volatile float aaa = 1.22;

  value = atoi (argv[1]);
  
  number = value;
  oldval = value;
  res = qmt_atomic_int_compare_and_exchange (&number, oldval, value - 1);

  fprintf (stderr, "number = %d oldval = %d and res = %d\n", number, 
	   oldval, res);

  fprintf (stderr, "aaa = %f\n", aaa);
  for (i = 0; i < 100; i++)
    qmt_atomic_float_inc (&aaa);
  fprintf (stderr, "aaa = %f\n", aaa);

  
}
#endif
