/**
 * This is a simple pthread syhchronization overhead test
 * This emulates EPCC OpenMP syncbench test
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>
#include <ctype.h>

#include "qmt.h"
#include "qmt_pthread.h"
#include "qmt_lock.h"
#include "qmt_atomic.h"


#ifdef _USE_PAPI
#include <papi.h>

static int is_number (char* str)
{
  char* p = str;

  while (p && *p) {
    if (!isdigit(*p))
      return 0;
    p++;
  }
  return 1;
}

static char *is_derived(PAPI_event_info_t *info)
{
  if (strlen(info->derived) == 0)
    return("No");
  else if (strcmp(info->derived,"NOT_DERIVED") == 0)
    return("No");
  else if (strcmp(info->derived,"DERIVED_CMPD") == 0)
    return("No");
  else
    return("Yes");
}

extern void qmt_papi_attach_event_to_thread (int papi_events, int stid);
extern void qmt_papi_detach_event_from_thread (int papi_events);
#endif

#define OUTERREPS 20 
#define CONF95 1.96 

int nthreads = 0;
int innerreps = 0;
long delaylength = 0;
double times[OUTERREPS+1], reftime, refsd; 

/**
 * Align the local variable with cache line size
 */
typedef struct _local_val_
{
  int val;
  int pad[W_CACHE_SIZE - 1];

}local_val_t;


static qmt_spinlock_t redlock;
static int            redaaaa;
static float          at_aaaa;
static local_val_t    locala[MAX_THREADS];

typedef struct _reduction_arg_
{
  int aaaa;
  qmt_spinlock_t lock;
}reduction_arg_t;

#ifdef _QMT_PUBLIC
static void delay(void* arg, int thid)
{
  int i;
  float a=0.; 

  for (i=0; i<delaylength; i++) a+=i; 
  if (a < 0) printf("%f \n",a); 
} 
#else
static void delay(unsigned int lo, unsigned int hi, int thid, void* arg)
{
  int i;
  float a=0.; 

  for (i=0; i<delaylength; i++) a+=i; 
  if (a < 0) printf("%f \n",a); 
} 
#endif

/**
 * Test barrier over head
 */
static void loop_delay(void* arg, int stid)
{
  int i, j;
  float a=0.; 

  for (j = 0; j < innerreps; j++) {
    for (i=0; i<delaylength; i++) a+=i; 
    if (a < 0) printf("%f \n",a); 
    qmt_sync (stid);
  }
} 

/**
 * Test lock/unlock overhead
 */
static void test_lock_r (void* arg, int stid)
{
  int i, j;
  float a=0.; 
  qmt_spinlock_t *lock = (qmt_spinlock_t *)arg;

  for (j = 0; j < innerreps/nthreads; j++) {
    qmt_spin_lock (lock);
    for (i=0; i<delaylength; i++) a+=i; 
    if (a < 0) printf("%f \n",a); 
    qmt_spin_unlock (lock);
  }
} 


/**
 * Test lock/unlock overhead
 */
static void atomic_test (void* arg, int thid)
{
  int j;
  qmt_qlock_t *lock = (qmt_qlock_t *)arg;

  for (j = 0; j < innerreps/nthreads; j++) {
    qmt_spin_lock (lock);
    /* qmt_atomic_float_inc (&at_aaaa);  */
    at_aaaa += 1.0;
    qmt_spin_unlock (lock);
  }
} 


/**
 * Test global reduction
 */
static void test_reduction (void* arg, int stid)
{
  int i;
  float a=0.;

  a = 0;
  for (i=0; i<delaylength; i++) a+=i; 
  if (a < 0) printf("%f \n",a); 

  (locala[stid].val) += 1;
} 


/* Intial time */
static time_t starttime = 0; 

double get_time_of_day_()
{

  struct timeval ts; 

  double t;

  int err; 

  err = gettimeofday(&ts, NULL); 

  t = (double) (ts.tv_sec - starttime)  + (double) ts.tv_usec * 1.0e-6; 
 
  return t; 

}

void init_time_of_day_()
{
  struct  timeval  ts;
  int err; 

  err = gettimeofday(&ts, NULL);
  starttime = ts.tv_sec; 
}

/* Check whether getclock is called first time */
static int firstcall = 1; 

double getclock()
{
  double time;
  
  if (firstcall) {
    init_time_of_day_(); 
    firstcall = 0;
  }
  time = get_time_of_day_(); 
  return time;
} 

void stats (double *mtp, double *sdp) 
{

  double meantime, totaltime, sumsq, mintime, maxtime, sd, cutoff; 

  int i, nr; 

  mintime = 1.0e10;
  maxtime = 0.;
  totaltime = 0.;

  for (i=1; i<=OUTERREPS; i++){
    mintime = (mintime < times[i]) ? mintime : times[i];
    maxtime = (maxtime > times[i]) ? maxtime : times[i];
    totaltime +=times[i]; 
  } 

  meantime  = totaltime / OUTERREPS;
  sumsq = 0; 

  for (i=1; i<=OUTERREPS; i++){
    sumsq += (times[i]-meantime)* (times[i]-meantime); 
  } 
  sd = sqrt(sumsq/(OUTERREPS-1));

  cutoff = 3.0 * sd; 

  nr = 0; 
  
  for (i=1; i<=OUTERREPS; i++){
    if ( fabs(times[i]-meantime) > cutoff ) nr ++; 
  }
  
  fprintf(stderr,"\n"); 
  fprintf(stderr,"Sample_size       Average     Min         Max          S.D.          Outliers\n");
  fprintf(stderr," %d                %f   %f   %f    %f      %d\n",OUTERREPS, meantime, mintime, maxtime, sd, nr); 
  fprintf(stderr,"\n");

  *mtp = meantime; 
  *sdp = sd; 

} 



static void refer()
{
  int j,k; 
  double start; 
  double meantime, sd; 

  fprintf(stderr, "\n");
  fprintf(stderr, "--------------------------------------------------------\n");
  fprintf(stderr, "Computing reference time 1\n"); 

  for (k=0; k<=OUTERREPS; k++){
    start  = getclock(); 
    for (j=0; j<innerreps; j++){
#ifdef _QMT_PUBLIC
      delay((void *)0, 0); 
#else
      delay(0, 0, 0, (void *)0);
#endif
    }
    times[k] = (getclock() - start) * 1.0e6 / (double) innerreps;
  }

  stats (&meantime, &sd);

  printf("Reference_time_1 =                        %f microseconds +/- %f\n", meantime, CONF95*sd);

  reftime = meantime;
  refsd = sd;  
}

static void referred()
{
  int j,k; 
  double start; 
  double meantime, sd; 
  int aaaa; 

  printf("\n");
  printf("--------------------------------------------------------\n");
  printf("Computing reference time 3\n"); 

  for (k=0; k<=OUTERREPS; k++){
    aaaa=0; 
    start  = getclock(); 
#if 1
    for (j=0; j<innerreps; j++){
#ifdef _QMT_PUBLIC
      delay((void *)0, 0); 
#else
      delay(0, 0, 0, (void *)0); 
#endif
      redaaaa += 1; 
    }
#endif

#if 0
    /* calculate parallel launch and barrier overhead */
    for (j=0; j<innerreps; j++) {
      qmt_pexec (delay, (void *)0);
      aaaa += 1;
    }
#endif
    times[k] = (getclock() - start) * 1.0e6 / (double) innerreps;
    if (aaaa < 0) printf("%d\n",aaaa); 
  }

  stats (&meantime, &sd);

  printf("Reference_time_3 =                        %f microseconds +/- %f\n", meantime, CONF95*sd);

  reftime = meantime;
  refsd = sd;  
}



void testpr()
{

  int j,k;
  double start; 
  double meantime, sd; 

  fprintf(stderr,"\n");
  fprintf(stderr, "--------------------------------------------------------\n");
  fprintf(stderr, "Computing PARALLEL time\n"); 

  for (k=0; k<=OUTERREPS; k++){
    start  = getclock(); 

    /* Master threads are doing the same work */
    for (j=0; j<innerreps; j++){
#ifdef _QMT_PUBLIC
      qmt_pexec (delay, (void *)0);
#else
      qmt_call (delay, 32000, (void *)0);
#endif
    }     
    times[k] = (getclock() - start) * 1.0e6 / (double) innerreps;
  }

  stats (&meantime, &sd);

  fprintf(stderr, "PARALLEL time =                           %f microseconds +/- %f\n", meantime, CONF95*sd);
  
  fprintf(stderr, "PARALLEL overhead =                       %f microseconds +/- %f\n", meantime-reftime, CONF95*(sd+refsd));

}


#ifdef _QMT_PUBLIC
void testbar()
{

  int k;
  double start; 
  double meantime, sd; 

  fprintf(stderr,"\n");
  fprintf(stderr, "--------------------------------------------------------\n");
  fprintf(stderr, "Computing BARRIER time\n"); 

  for (k=0; k<=OUTERREPS; k++){
    start  = getclock(); 

    /* Master threads are doing the same work */
    qmt_pexec (loop_delay, (void *)0);

    /* Here is a simple barrier */
    times[k] = (getclock() - start) * 1.0e6 / (double) innerreps;
  }

  stats (&meantime, &sd);

  fprintf(stderr, "BARRIER time =                           %f microseconds +/- %f\n", meantime, CONF95*sd);
  
  fprintf(stderr, "BARRIER overhead =                       %f microseconds +/- %f\n", meantime-reftime, CONF95*(sd+refsd));

}


void testlock()
{

  int k; 
  double start; 
  double meantime, sd; 
  qmt_spinlock_t lock;

  printf("\n");
  printf("--------------------------------------------------------\n");
  printf("Computing LOCK/UNLOCK time\n"); 

  qmt_spin_init (&lock);
  for (k=0; k<=OUTERREPS; k++){
    start  = getclock(); 
    qmt_pexec (test_lock_r, (void *)&lock);
    times[k] = (getclock() - start) * 1.0e6 / (double) innerreps;
  }

  stats (&meantime, &sd);

  printf("LOCK/UNLOCK time =                           %f microseconds +/- %f\n", meantime, CONF95*sd);
  
  printf("LOCK/UNLOCK overhead =                       %f microseconds +/- %f\n", meantime-reftime, CONF95*(sd+refsd));

}

void testred()
{

  int j,k,l;
  double start; 
  double meantime, sd; 

  printf("\n");
  printf("--------------------------------------------------------\n");
  printf("Computing REDUCTION time\n"); 

  qmt_spin_init (&redlock);
  for (k=0; k<=OUTERREPS; k++){
    redaaaa = 0;
    for (l = 0; l < MAX_THREADS; l++)
      locala[l].val = redaaaa;
    start  = getclock(); 

    /*
     * Executed innerloops of times 
     * reduction is done inside each threads
     */
    for (j = 0; j < innerreps; j++) {
      qmt_pexec (test_reduction, (void *)0);

      redaaaa = locala[0].val;
      for (l = 1; l < nthreads; l++)
	redaaaa += locala[l].val;
    }

    times[k] = (getclock() - start) * 1.0e6 / (double) innerreps;
    if (redaaaa < 0) printf("%d\n",redaaaa); 
  }

  stats (&meantime, &sd);

  printf("REDUCTION time =                           %f microseconds +/- %f\n", meantime, CONF95*sd);
  
  printf("REDUCTION overhead =                       %f microseconds +/- %f\n", meantime-reftime, CONF95*(sd+refsd));

}

static void referatom()
{
  int j,k; 
  double start; 
  double meantime, sd; 
  float aaaa; 

  printf("\n");
  printf("--------------------------------------------------------\n");
  printf("Computing reference time 2\n"); 

  for (k=0; k<=OUTERREPS; k++){
    aaaa=0; 
    start  = getclock(); 
    for (j=0; j<innerreps; j++){
       aaaa += 1; 
    }
    times[k] = (getclock() - start) * 1.0e6 / (double) innerreps;
    if (aaaa < 0) printf("%f\n",aaaa); 
  }

  stats (&meantime, &sd);

  printf("Reference_time_2 =                        %f microseconds +/- %f\n", meantime, CONF95*sd);

  reftime = meantime;
  refsd = sd;  
}


static void testatom()
{

  int k; 
  double start; 
  double meantime, sd; 
  qmt_spinlock_t lock;

  printf("\n");
  printf("--------------------------------------------------------\n");
  printf("Computing ATOMIC time\n"); 

  qmt_spin_init (&lock);
  for (k=0; k<=OUTERREPS; k++){
    at_aaaa = 0.0; 
    start  = getclock(); 
    qmt_pexec (atomic_test, (void *)&lock);
    times[k] = (getclock() - start) * 1.0e6 / (double) innerreps;
    if (at_aaaa < 0.0) printf("%f\n",at_aaaa);  
  }

  stats (&meantime, &sd);

  printf("ATOMIC time =                           %f microseconds +/- %f\n", meantime, CONF95*sd);
  
  printf("ATOMIC overhead =                       %f microseconds +/- %f\n", meantime-reftime, CONF95*(sd+refsd));

}
#endif


int 
main (int argc, char** argv)
{
#ifdef _USE_PAPI
  int papi_event, event_number;
  char papi_event_name[80];
  int i, stid, p_num_threads;
  long_long evalue[1];
  PAPI_thread_id_t ptid[16];
  int retval, EventSet = PAPI_NULL;
  PAPI_event_info_t info;
  const PAPI_hw_info_t *hwinfo = NULL;

  if (argc < 3) {
    fprintf (stderr, "Usage: %s papievent|eventnumber tid\n", argv[0]);
    exit (1);
  }
  if (is_number (argv[1])) {
    papi_event = atoi (argv[1]);
    event_number = 1;
    sprintf (papi_event_name, "%d", papi_event);
  }
  else {
    strcpy (papi_event_name, argv[1]);
    event_number = 0;
  }
  stid = atoi (argv[2]);

  retval = PAPI_library_init (PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    fprintf(stderr, "PAPI library init error retval = %d current = 0x%x!\n", 
	    retval, PAPI_VER_CURRENT);
    exit(1);
  }

  /* initialize thread package */
  retval = PAPI_thread_init (pthread_self);
  if (retval != PAPI_OK) {
    fprintf (stderr, "Cannot initialize papi_thread\n");
    exit (1);
  }

  /**
   * Get hardware information of this system
   */
  if ((hwinfo = PAPI_get_hardware_info()) == NULL) {
    fprintf (stderr, "Cannot retrieve hardware information of this machine\n");
    exit (1);
  }
  printf("Available events and hardware information.\n");
  printf
    ("-------------------------------------------------------------------------\n");
  printf("Vendor string and code   : %s (%d)\n", hwinfo->vendor_string,
	 hwinfo->vendor);
  printf("Model string and code    : %s (%d)\n", hwinfo->model_string, hwinfo->model);
  printf("CPU Revision             : %f\n", hwinfo->revision);
  printf("CPU Megahertz            : %f\n", hwinfo->mhz);
  printf("CPU's in this Node       : %d\n", hwinfo->ncpu);
  printf("Nodes in this System     : %d\n", hwinfo->nnodes);
  printf("Total CPU's              : %d\n", hwinfo->totalcpus);
  printf("Number Hardware Counters : %d\n", PAPI_get_opt(PAPI_MAX_HWCTRS, NULL));
  printf("Max Multiplex Counters   : %d\n", PAPI_get_opt(PAPI_MAX_MPX_CTRS, NULL));
  printf
    ("-------------------------------------------------------------------------\n");
  
  /* Scan for all preset available events on this platform */
  
  i = PAPI_PRESET_MASK;
  printf("Name\t\tDerived\tDescription (Mgr. Note)\n");
  do { 
    retval = PAPI_get_event_info(i, &info); 
    if (retval == PAPI_OK) { 
      if (info.count)
	printf("%s\t%s\t%s",
	       info.symbol,
	       is_derived(&info),
	       info.long_descr);
      if (info.note[0]) printf(" (%s)", info.note);
      printf("\n");
    }
  }while (PAPI_enum_event(&i, PAPI_ENUM_ALL) == PAPI_OK); 

  if(PAPI_create_eventset(&EventSet) != PAPI_OK) {
    fprintf (stderr, "PAPI_create_eventset failed\n");
    exit (1);
  }

  /**
   * Convert event name to event number
   */
  if (!event_number) {
    if (PAPI_event_name_to_code (argv[1], &papi_event) != PAPI_OK) {
      fprintf (stderr, "Cannot convert event name %s to event number\n",
	       argv[1]);
      exit (1);
    }
  }

  if ((retval = PAPI_add_event (EventSet, papi_event)) != PAPI_OK) {
    fprintf (stderr, "PAPI_add_events failed with retval = %d\n",retval);
    exit (1);
  }
#endif

  /**
   * Initialize the qmt package
   */
  qmt_init ();


  nthreads = qmt_num_threads ();
  fprintf (stderr, "Running pthread benchmark on %d thread(s)\n", nthreads);

#ifdef _USE_PAPI
  sleep (5);

  p_num_threads = nthreads;
  PAPI_list_threads (ptid, &p_num_threads);

#if 0
  fprintf (stderr, "PAPI number of threads is %d\n", p_num_threads);
  for (i = 0; i < p_num_threads; i++) {
    fprintf (stderr, "Threads id = %u\n", ptid[i]);
  }

  if ((retval = PAPI_attach (EventSet, ptid[stid])) != PAPI_OK)
    fprintf (stderr, "PAPI attach failed for thread %d ret = %d\n", stid,
	     retval);
#endif

#endif

  delaylength = 5000;
  innerreps = 10000;

  /* GENERATE REFERENCE TIME */ 
  refer();   

  refer();

  /* TEST  PARALLEL REGION */ 
  innerreps = 20000;

  testpr();

#ifdef _QMT_PUBLIC  /* only test for public release */

  /* Test Barrier */
  innerreps = 20000;
#ifdef _USE_PAPI
  if (PAPI_reset (EventSet) != PAPI_OK) {
    fprintf (stderr, "Cannot reset all event counters\n");
    exit (1);
  }

  if (PAPI_start(EventSet) != PAPI_OK) {
    fprintf (stderr, "Cannot start all event counters\n");
    exit (1);
  }
#endif
  testbar ();

#ifdef _USE_PAPI
  if (PAPI_read(EventSet, evalue) != PAPI_OK) {
    fprintf (stderr, "Cannot read event %s value\n", papi_event_name);
    return -1;
  }

  if (PAPI_stop(EventSet, evalue) != PAPI_OK) {
    fprintf (stderr, "Cannot stop couting event %s \n", papi_event_name);    
    return -1;
  }

  fprintf (stderr, "%s event has value = %lld\n", papi_event_name, evalue[0]);
#endif


  /* Test lock/unlock */
  testlock ();


  /* GENERATE REFERENCE TIME */ 
  innerreps = 10000;

  referred();   
  /* Test reduction */
  innerreps = 20000; 
  testred ();


  innerreps = 100000;
  referatom ();

  /* test atomic */
  testatom ();

#endif

  qmt_finalize ();

  return 0;
}
