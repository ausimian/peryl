#include <stdio.h>
#include "python2.7/Python.h"
#include "erl_nif.h"

static ErlNifTid    tid_main;
static ErlNifCond*  cond_stop_requested;
static ErlNifMutex* mutex_stop_requested;

#define SAFE_COND_DESTROY(X)                    \
  if (X) {                                      \
    enif_cond_destroy(X);                       \
    X = NULL;                                   \
  }

#define SAFE_MUTEX_DESTROY(X)                   \
  if (X) {                                      \
    enif_mutex_destroy(X);                      \
    X = NULL;                                   \
  }

static
void peryl_destroy_vars() {
  SAFE_COND_DESTROY(cond_stop_requested);
  SAFE_MUTEX_DESTROY(mutex_stop_requested);
}

static
int peryl_create_vars() {
  if (!(cond_stop_requested = enif_cond_create("peryl_cond_stop_requested")))
    goto failed;
  if (!(mutex_stop_requested = enif_mutex_create("peryl_mutex_stop_requested")))
    goto failed;

  return 0;

 failed:
  peryl_destroy_vars();
  return -1;
}

static
void* peryl_py_thread(void* arg) {

  PyThreadState* threadState;
  ErlNifCond*    cond_started = (ErlNifCond*)arg;

  Py_InitializeEx(0);
  PyEval_InitThreads();
  threadState = PyEval_SaveThread();

  enif_mutex_lock(mutex_stop_requested);
  enif_cond_signal(cond_started);
  enif_cond_wait(cond_stop_requested, mutex_stop_requested);
  enif_mutex_unlock(mutex_stop_requested);

  PyEval_RestoreThread(threadState);
  Py_Finalize();

  return NULL;
}


static
int load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info) {

  int init_res = -1;
  ErlNifCond*   cond_started;
  ErlNifMutex*  mutex_started;

  (void)env;
  (void)priv_data;
  (void)load_info;

  if (peryl_create_vars())
    return -1;

  if ((cond_started = enif_cond_create("peryl_cond_started"))) {
    if ((mutex_started = enif_mutex_create("peryl_mutex_started"))) {
      enif_mutex_lock(mutex_started);
      if (!(init_res = enif_thread_create("peryl_main", &tid_main, peryl_py_thread, cond_started, NULL))) {
        enif_cond_wait(cond_started, mutex_started);
      }
      enif_mutex_unlock(mutex_started);
      enif_mutex_destroy(mutex_started);
    }
    enif_cond_destroy(cond_started);
  }

  if (init_res) {
    peryl_destroy_vars();
  }

  return init_res;
}

static
void unload(ErlNifEnv* env, void* priv_data) {
  (void)env;
  (void)priv_data;
  enif_cond_signal(cond_stop_requested);
  enif_thread_join(tid_main, NULL);
  peryl_destroy_vars();
}

static
ERL_NIF_TERM demo(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  (void)argc;
  (void)argv;

  return enif_make_atom(env, "ok");
}

typedef struct _peryl_task_state {
  ErlNifEnv*     env;
  ERL_NIF_TERM   script;
  PyThreadState* py_thread_state;
} peryl_task_state;


static
void* peryl_task_thread(void* arg) {

  FILE* fp = NULL;
  char  filename[256];

  peryl_task_state* pts = (peryl_task_state*)arg;

  if (enif_get_string(pts->env, pts->script, filename, sizeof(filename) - 1, ERL_NIF_LATIN1) <= 0)
    goto done;

  if (!(fp = fopen(filename, "r")))
    goto done;

  PyEval_AcquireLock();
  pts->py_thread_state = Py_NewInterpreter();
  if (!pts->py_thread_state)
    goto failed;

  PyRun_SimpleFile(fp, filename);

 failed:
  if (pts->py_thread_state)
    Py_EndInterpreter(pts->py_thread_state);
  PyEval_ReleaseLock();

 done:
  if (fp)
    fclose(fp);

  enif_free_env(pts->env);
  enif_free(pts);

  return NULL;
}


static
ERL_NIF_TERM run(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
  (void)argv;

  ERL_NIF_TERM res;

  peryl_task_state* pts = NULL;
  if (argc != 1) {
    res = enif_make_badarg(env);
    goto failed;
  }

  if (!(pts = (peryl_task_state*)enif_alloc(sizeof(pts))))
    goto failed;

  pts->env    = enif_alloc_env();
  pts->script = enif_make_copy(pts->env, argv[0]);
  pts->py_thread_state = NULL;

  ErlNifTid tid_task;
  if (enif_thread_create(NULL, &tid_task, peryl_task_thread, pts, NULL))
    goto failed;

  res = enif_make_atom(env, "ok");
  goto done;

 failed:
  if (pts) {
    enif_free_env(pts->env);
    enif_free(pts);
  }
  res = enif_make_atom(env, "failed");

 done:
  return res;

}


static ErlNifFunc funcs[] = {
  {"demo", 0, demo, 0},
  {"run",  1, run, 0}
};

ERL_NIF_INIT(peryl, funcs, load, NULL, NULL, unload)
