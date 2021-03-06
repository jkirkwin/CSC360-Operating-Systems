#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 10000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

/*
 * A parameterized version of my solution using pthreads. 
 * A little more code reuse but a lot more pain. :)
 */ 

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}

/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

int flag; 

uthread_cond_t resource_conds[10]; // holds the 3 condition variables for each resource
uthread_cond_t wakeups[10]; // holds the 3 wakeup condition variables used by the smokers

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};
  VERBOSE_PRINT("Agent thread running\n");
  uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = random() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) {
        VERBOSE_PRINT ("match available\n");
        uthread_cond_signal (a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        uthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        uthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      uthread_cond_wait (a->smoke);
    }
  uthread_mutex_unlock (a->mutex);
  return NULL;
}

typedef struct listener_package {
  struct Agent *agent;
  enum Resource resource;
} listener_package_t;

listener_package_t* get_listener_pkg(enum Resource resource, struct Agent *agent) {
  VERBOSE_PRINT("\tGetting listener pkg\n"); 
  listener_package_t *lp = (listener_package_t *) malloc(sizeof(listener_package_t));
  lp->agent = agent;
  VERBOSE_PRINT("\t\tassigned agent\n");
  lp->resource = resource;
  return lp;
}

int listeners_ready;
uthread_mutex_t listeners_ready_mutex; 

void* listener(void *p) {
  listener_package_t *pkg = (listener_package_t *) p;
  char *name = resource_name[pkg->resource];
  struct Agent *agent = pkg->agent; 
  uthread_cond_t resource_cond = resource_conds[pkg->resource]; 
  VERBOSE_PRINT("%s listener running\n", name);
  uthread_mutex_lock(agent->mutex);
  while(1) {
    if(listeners_ready < 3) {
      uthread_mutex_lock(listeners_ready_mutex);
      listeners_ready++; // To let us ensure all have entered the loop before passing control to the agent  
      uthread_mutex_unlock(listeners_ready_mutex);
    }
    VERBOSE_PRINT("%s listener ready\n", name);
    uthread_cond_wait(resource_cond);
    VERBOSE_PRINT("%s listener woken up\n", name);
    flag += pkg->resource;
    switch(flag) {
      case MATCH + PAPER:
      case MATCH + TOBACCO:
      case PAPER + TOBACCO:
        uthread_cond_signal(wakeups[flag]);
        flag = 0;
        break;
    }
  }
} 

typedef struct smoker_package {
  enum Resource resource;
  struct Agent *agent;
  uthread_cond_t wakeup;
} smoker_package_t;

smoker_package_t* get_smoker_pkg(enum Resource resource, struct Agent *agent, uthread_cond_t wakeup) {
  VERBOSE_PRINT("\tGetting smoker pkg\n"); 
  smoker_package_t *sp = (smoker_package_t *) malloc(sizeof(smoker_package_t));
  sp->resource = resource;
  sp->agent = agent; 
  sp->wakeup = wakeup;
}

int smokers_ready;
uthread_mutex_t smokers_ready_mutex;

void* smoker(void *p) {
  smoker_package_t *pkg = (smoker_package_t *) p;
  struct Agent *agent = pkg->agent;
  char *name = resource_name[pkg->resource];
  uthread_cond_t wakeup = pkg->wakeup;
  VERBOSE_PRINT("%s smoker running\n", name);
  
  uthread_mutex_lock(agent->mutex);
  while((1)){
    if(smokers_ready < 3) {
      uthread_mutex_lock(smokers_ready_mutex);
      smokers_ready++; // To let us ensure all have entered the loop before passing control to the agent  
      uthread_mutex_unlock(smokers_ready_mutex);
    }
    VERBOSE_PRINT("%s smoker ready\n", name);
    uthread_cond_wait(wakeup);
    VERBOSE_PRINT("%s smoker SMOKING\n", name);
    uthread_cond_signal(agent->smoke);
    smoke_count[pkg->resource]++;
  }
  
}

int main (int argc, char** argv) {
  uthread_init (7);
  struct Agent*  a = createAgent();
  flag = 0;

  VERBOSE_PRINT("MAIN STARTED\n");

  wakeups[MATCH + PAPER] = uthread_cond_create(a->mutex);
  wakeups[TOBACCO + PAPER] = uthread_cond_create(a->mutex);
  wakeups[MATCH + TOBACCO] = uthread_cond_create(a->mutex);

  VERBOSE_PRINT("WAKEUPS INITIALIZED\n");

  resource_conds[MATCH] = a->match;
  resource_conds[PAPER] = a->paper;
  resource_conds[TOBACCO] = a->tobacco;

  VERBOSE_PRINT("RESOURCE COND ARRAY SET UP\n");

  uthread_t match_smoker_ut, paper_smoker_ut, tobacco_smoker_ut; 
  uthread_t match_listener_ut, paper_listener_ut, tobacco_listener_ut;

  listeners_ready = 0;
  listeners_ready_mutex = uthread_mutex_create();
  match_listener_ut = uthread_create(listener, get_listener_pkg(MATCH, a));
  paper_listener_ut = uthread_create(listener, get_listener_pkg(PAPER, a));
  tobacco_listener_ut = uthread_create(listener, get_listener_pkg(TOBACCO, a));  
  while(listeners_ready < 3); // Wait for all listeners to enter their loop

  VERBOSE_PRINT("LISTENERS CREATED AND READY\n");

  smokers_ready = 0;
  smokers_ready_mutex = uthread_mutex_create();
  match_smoker_ut = uthread_create(smoker, get_smoker_pkg(MATCH, a, wakeups[PAPER + TOBACCO]));
  paper_smoker_ut = uthread_create(smoker, get_smoker_pkg(PAPER, a, wakeups[MATCH + TOBACCO]));
  paper_smoker_ut = uthread_create(smoker, get_smoker_pkg(TOBACCO, a, wakeups[PAPER + MATCH]));
  while(smokers_ready < 3); // Wait for all smokers to enter their loop

  VERBOSE_PRINT("SMOKERS CREATED AND READY. RUNNING AGENT\n");

  uthread_join (uthread_create (agent, a), 0);

  // Teardown
  uthread_mutex_destroy(a->mutex);

  uthread_cond_destroy(a->match);
  uthread_cond_destroy(a->tobacco);
  uthread_cond_destroy(a->paper);
  
  uthread_cond_destroy(wakeups[PAPER + TOBACCO]);
  uthread_cond_destroy(wakeups[MATCH + TOBACCO]);
  uthread_cond_destroy(wakeups[PAPER + MATCH]);

  uthread_mutex_destroy(smokers_ready_mutex);
  uthread_mutex_destroy(listeners_ready_mutex);

  // Verify
  VERBOSE_PRINT("Smoke  counts: %d matches, %d paper, %d tobacco\n", smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
  VERBOSE_PRINT("Signal counts: %d matches, %d paper, %d tobacco\n", signal_count [MATCH], signal_count [PAPER], signal_count [TOBACCO]);
  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}
