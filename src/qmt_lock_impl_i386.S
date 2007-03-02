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
 *     QCD Multi-Threading Spin Lock Assembly Code
 *
 * Author:  
 *      Jie Chen
 *      Jefferson Lab Scientific Computing Group
 *
 * Revision History:
 *   $Log: qmt_lock_impl_i386.S,v $
 *   Revision 1.1.1.1  2007-03-02 19:44:55  chen
 *   Initial import qmt source
 *
 *
 */
	.globl  qmt_spin_unlock
	.type   qmt_spin_unlock,@function
	.align  16
qmt_spin_unlock:
	movl    4(%esp), %eax
	movl    $1, (%eax)
	xorl    %eax, %eax
	ret
	.size   qmt_spin_unlock,.-qmt_spin_unlock

	/* The implementation of qmt_spin_init is identical.  */
	.globl  qmt_spin_init
qmt_spin_init = qmt_spin_unlock
	
	
