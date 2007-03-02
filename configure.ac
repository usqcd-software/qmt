# configure.ac file for qmt package
AC_INIT([qmt], [0.9], [qmt authors <chen@jlab.org]>,[qmt])
AC_COPYRIGHT([Jefferson Science Assoicates, LLC. Jefferson Lab])

AC_CONFIG_SRCDIR([src/qmt.h])
AC_CHECK_HEADERS([stdlib.h string.h pthread.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Get host information
AC_CANONICAL_BUILD
# Check whether this is a linux machine
case $build in
     *linux*)
     AC_MSG_NOTICE([This is a linux machine.])
     ;;
     *)
     AC_MSG_WARN([This package has not been tried on your platform!!!!!!])
     ;;
esac
# Check whether this is a 32 bit build
case $build in
     *x86_64*)
     AC_MSG_NOTICE([This is a 64 bit machine, cannot compile 32 bit library here.])
     qmt_64bit=yes
     ;;
     *)
     AC_MSG_NOTICE([This is a 32 bit machine, cannot compile 64 bit library here.])
     qmt_64bit=no
     ;;
esac
AM_CONDITIONAL([QMT_BUILD_32BIT],[test "$qmt_64bit" = no])

# Check whether this is a Intel machine
if grep -q Intel /proc/cpuinfo
then
	AC_MSG_NOTICE([This is an Intel Xeon Machine])
	AC_SUBST(CFLAGS,['-Wall -D_REENTRANT -O3 -fexpensive-optimizations -ffast-math -fomit-frame-pointer -finline-functions -fpeephole -funroll-loops'])
else
	AC_MSG_NOTICE([This is a AMD Opteron Machine])
	AC_SUBST(CFLAGS,['-Wall -D_REENTRANT -O3 -fexpensive-optimizations -ffast-math -fomit-frame-pointer -finline-functions -fpeephole -funroll-loops'])
fi

AC_PROG_CC

AM_PROG_AS

AC_PROG_INSTALL

AC_PROG_RANLIB

AC_CHECK_TOOL(AR, ar, [ar])



# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

#Enable Intel or AMD
PROC=opteron
AC_ARG_ENABLE([xeon],
	AC_HELP_STRING([--enable-xeon],[compile qmt library optimized for Intel Xeon processors.]),
	[if test "$enableval" = yes; then PROC=xeon; fi])
AC_ARG_ENABLE([opteron],
	AC_HELP_STRING([--enable-opteron],[compile qmt library optimized for AMD Opteron processors.]),
	[if test "$enableval" = yes; then PROC=opteron; fi])

if test "$PROC" = opteron
then
	AC_MSG_NOTICE([Compile qmt library optimized for AMD Opteron processors.])
	AC_SUBST(CPU_DEF,[-mtune=opteron])
else
	AC_MSG_NOTICE([Compile qmt library optimized for Intel Xeon processors.])
	AC_SUBST(CPU_DEF,[-mtune=nocona])
fi

# Enable different barrier algorithm
BARRIER_ALG=queue
AC_ARG_ENABLE([centralized-barrier],
	AC_HELP_STRING([--enable-centralized-barrier],[Use a centralized barrier algorithm instead of the default queue-based barrier algorithm]),
	[if test "$enableval" = yes; then BARRIER_ALG=centralized; fi])

AC_ARG_ENABLE([lock-barrier],
	AC_HELP_STRING([--enable-lock-barrier],[Use a spin lock barrier algorithm instead of the default queue-based barrier algorithm]),
	[if test "$enableval" = yes; then BARRIER_ALG=lock; fi])

if test "$BARRIER_ALG" = centralized
then
	AC_MSG_NOTICE([Choose an old centralized barrier algorithm.])
	AC_SUBST(BARRIER_DEF,[-D_USE_SPIN_BARRIER])
elif test "$BARRIER_ALG" = lock
then
	AC_MSG_NOTICE([Choose a slow spin lock barrier algorithm.])
	AC_SUBST(BARRIER_DEF,[-D_USE_LOCK_BARRIER])
else
	AC_MSG_NOTICE([Choose a default queue barrier algorithm.])
fi

RELEASE=lqcd
AC_ARG_ENABLE([public-release],
	AC_HELP_STRING([--enable-public-release],[Build this library for general public release instead of to the LQCD community]),
	[if test "$enableval" = yes; then RELEASE=public; fi])

if test "$RELEASE" = public
then
	AC_MSG_NOTICE([This qmt library will be built for general public. It will have different APIs from the LQCD style APIs.])
	AC_SUBST(RELEASE_DEF,[-D_QMT_PUBLIC])
fi


AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
