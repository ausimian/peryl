#include "/usr/include/python2.7/Python.h"
#include "erl_driver.h"

static ErlDrvTid    tid_main;
static ErlDrvCond*  cond_stop_requested;
static ErlDrvMutex* mutex_stop_requested;

typedef struct _peryl_drv_data {
  ErlDrvPort port;
} peryl_drv_data;


#define SAFE_COND_DESTROY(X)                    \
  if (X) {                                      \
    erl_drv_cond_destroy(X);                    \
    X = NULL;                                   \
  }

#define SAFE_MUTEX_DESTROY(X)                   \
  if (X) {                                      \
    erl_drv_mutex_destroy(X);                   \
    X = NULL;                                   \
  }

static
void peryl_destroy_vars() {
  SAFE_COND_DESTROY(cond_stop_requested);
  SAFE_MUTEX_DESTROY(mutex_stop_requested);
}

static
int peryl_create_vars() {
  if (!(cond_stop_requested = erl_drv_cond_create("peryl_cond_stop_requested")))
    goto failed;
  if (!(mutex_stop_requested = erl_drv_mutex_create("peryl_mutex_stop_requested")))
    goto failed;

  return 0;

 failed:
  peryl_destroy_vars();
  return -1;
}

static
void* peryl_py_thread(void* arg) {

  PyThreadState* threadState;
  ErlDrvCond*    cond_started = (ErlDrvCond*)arg;

  Py_InitializeEx(0);
  PyEval_InitThreads();
  threadState = PyEval_SaveThread();

  erl_drv_mutex_lock(mutex_stop_requested);
  erl_drv_cond_signal(cond_started);
  erl_drv_cond_wait(cond_stop_requested, mutex_stop_requested);
  erl_drv_mutex_unlock(mutex_stop_requested);

  PyEval_RestoreThread(threadState);
  Py_Finalize();

  return NULL;
}


static
int peryl_drv_init(void) {

  int init_res = -1;
  ErlDrvSysInfo si;
  ErlDrvCond*   cond_started;
  ErlDrvMutex*  mutex_started;

  driver_system_info(&si, sizeof(si));
  if (!si.smp_support)
    return -1;

  if (peryl_create_vars())
    return -1;

  if ((cond_started = erl_drv_cond_create("peryl_cond_started"))) {
    if ((mutex_started = erl_drv_mutex_create("peryl_mutex_started"))) {
      erl_drv_mutex_lock(mutex_started);
      if (!(init_res = erl_drv_thread_create("peryl_main", &tid_main, peryl_py_thread, cond_started, NULL))) {
        erl_drv_cond_wait(cond_started, mutex_started);
      }
      erl_drv_mutex_unlock(mutex_started);
      erl_drv_mutex_destroy(mutex_started);
    }
    erl_drv_cond_destroy(cond_started);
  }

  if (init_res) {
    peryl_destroy_vars();
  }

  return init_res;
}

static
void peryl_drv_finish(void) {
  erl_drv_cond_signal(cond_stop_requested);
  erl_drv_thread_join(tid_main, NULL);
  peryl_destroy_vars();
}

static
ErlDrvData peryl_drv_start(ErlDrvPort port, char *buf) {
  (void)buf;
  peryl_drv_data *data = (peryl_drv_data*)driver_alloc_binary(sizeof(peryl_drv_data));
  data->port = port;
  return (ErlDrvData)data;
}

static
void peryl_drv_stop(ErlDrvData handle) {
  driver_free(handle);
}

static
ErlDrvEntry peryl_driver_entry = {
  peryl_drv_init,			/* F_PTR init, called when driver is loaded */
  peryl_drv_start,		/* L_PTR start, called when port is opened */
  peryl_drv_stop,		/* F_PTR stop, called when port is closed */
  NULL,		/* F_PTR output, called when erlang has sent */
  NULL,			/* F_PTR ready_input, called when input descriptor ready */
  NULL,			/* F_PTR ready_output, called when output descriptor ready */
  "peryl",		/* char *driver_name, the argument to open_port */
  peryl_drv_finish,			/* F_PTR finish, called when unloaded */
  NULL,                       /* void *handle, Reserved by VM */
  NULL,			/* F_PTR control, port_command callback */
  NULL,			/* F_PTR timeout, reserved */
  NULL,			/* F_PTR outputv, reserved */
  NULL,                       /* F_PTR ready_async, only for async drivers */
  NULL,                       /* F_PTR flush, called when port is about
                                 to be closed, but there is data in driver
                                 queue */
  NULL,                       /* F_PTR call, much like control, sync call
                                 to driver */
  NULL,                       /* F_PTR event, called when an event selected
                                 by driver_event() occurs. */
  ERL_DRV_EXTENDED_MARKER,    /* int extended marker, Should always be
                                 set to indicate driver versioning */
  ERL_DRV_EXTENDED_MAJOR_VERSION, /* int major_version, should always be
                                     set to this value */
  ERL_DRV_EXTENDED_MINOR_VERSION, /* int minor_version, should always be
                                     set to this value */
  0,                          /* int driver_flags, see documentation */
  NULL,                       /* void *handle2, reserved for VM use */
  NULL,                       /* F_PTR process_exit, called when a
                                 monitored process dies */
  NULL,                       /* F_PTR stop_select, called to close an
                                 event object */
  NULL
};

DRIVER_INIT(peryl) {
	return &peryl_driver_entry;
}
