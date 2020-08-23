#include "skynet.h"
#include "skynet_mq.h"
#include "skynet_server.h"
#include "skynet_service.h"
#include "skynet_config.h"
#include "skynet_timer.h"
#include "skynet_socket.h"
#include "skynet_harbor.h"

#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

struct monitor {
    int count;
    int sleep;
    int quit;
    pthread_t * pids;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
};

static struct monitor *m = NULL;

void create_thread(pthread_t *thread, void *(*start_routine) (void *), void *arg) {
    if (pthread_create(thread, NULL, start_routine, arg)) {
        skynet_logger_error(0, "[skynet]Create thread failed");
        exit(1);
    }
}

void wakeup(struct monitor *m, int busy) {
    if (m->sleep >= m->count - busy) {
        // signal sleep worker, "spurious wakeup" is harmless
        pthread_cond_signal(&m->cond);
    }
}

void * thread_socket(void *p) {
    struct monitor * m = p;
    while (!m->quit) {
        int r = skynet_socket_poll();
        if (r==0)
            break;
        if (r<0) {
            continue;
        }
        wakeup(m,0);
    }
    return NULL;
}

void * thread_timer(void *p) {
    struct monitor * m = p;
    while (!m->quit) {
        skynet_updatetime();
        wakeup(m,m->count-1);
        usleep(1000);
    }
    return NULL;
}

void * thread_worker(void *p) {
    struct monitor *m = p;
    while (!m->quit) {
        if (skynet_message_dispatch()) {
            if (pthread_mutex_lock(&m->mutex) == 0) {
                ++ m->sleep;
                // "spurious wakeup" is harmless,
                // because skynet_message_dispatch() can be call at any time.
                if (!m->quit)
                    pthread_cond_wait(&m->cond, &m->mutex);
                -- m->sleep;
                if (pthread_mutex_unlock(&m->mutex)) {
                    skynet_logger_error(0, "[skynet]unlock mutex error");
                    exit(1);
                }
            }
        }
    }
    return NULL;
}

void skynet_start(unsigned harbor, unsigned thread) {
    unsigned i;

    m = skynet_malloc(sizeof(*m));
    memset(m, 0, sizeof(*m));
    m->count = thread;
    m->sleep = 0;
    m->quit = 0;
    m->pids = skynet_malloc(sizeof(*m->pids) * (thread+2));

    if (pthread_mutex_init(&m->mutex, NULL)) {
        skynet_logger_error(0, "[skynet]Init mutex error");
        exit(1);
    }
    if (pthread_cond_init(&m->cond, NULL)) {
        skynet_logger_error(0, "[skynet]Init cond error");
        exit(1);
    }

    for (i=0;i<thread;i++) {
        create_thread(m->pids+i, thread_worker, m);
    }
    create_thread(m->pids+i, thread_timer, m);
    create_thread(m->pids+i+1, thread_socket, m);

    skynet_logger_notice(0, "[skynet]skynet start, harbor:%u workers:%u", harbor, thread);

    for (i=0;i<thread;i++) {
        pthread_join(*(m->pids+i), NULL); 
    }

    skynet_logger_notice(0, "[skynet]skynet shutdown, harbor:%u", harbor);

    pthread_mutex_destroy(&m->mutex);
    pthread_cond_destroy(&m->cond);
    skynet_free(m->pids);
    skynet_free(m);
}

void skynet_shutdown(int sig) {
    m->quit = 1;

    // wakeup socket thread
    skynet_socket_exit();
    
    // wakeup all worker thread
    pthread_mutex_lock(&m->mutex);
    pthread_cond_broadcast(&m->cond);
    pthread_mutex_unlock(&m->mutex);
}

void skynet_coredump(int sig) {
    unsigned i;

    fprintf(stdout, "skynet coredump. sig:%d\n", sig);

    skynet_shutdown(sig);

    for (i=0;i<m->count;i++) {
        pthread_join(*(m->pids+i), NULL); 
    }

    skynet_service_releaseall();

    signal(sig, SIG_DFL);
    raise(sig);
}

void skynet_signal_init() {
    struct sigaction actTerminate;
    actTerminate.sa_handler = skynet_shutdown;
    sigemptyset(&actTerminate.sa_mask);
    actTerminate.sa_flags = 0;
    sigaction(SIGINT, &actTerminate, NULL);
    sigaction(SIGTERM, &actTerminate, NULL);

    struct sigaction actCoredump;
    actCoredump.sa_handler = skynet_coredump;
    sigemptyset(&actCoredump.sa_mask);
    actCoredump.sa_flags = 0;
    sigaction(SIGSEGV, &actCoredump, NULL);
    sigaction(SIGILL, &actCoredump, NULL);
    sigaction(SIGFPE, &actCoredump, NULL);
    sigaction(SIGABRT, &actCoredump, NULL);
    sigaction(SIGKILL, &actCoredump, NULL);
    sigaction(SIGXFSZ, &actCoredump, NULL);

    /*// block SIGINT to all child process:
    sigset_t bset, oset;
    sigemptyset(&bset);
    sigaddset(&bset, SIGINT);
    // equivalent to sigprocmask
    pthread_sigmask(SIG_BLOCK, &bset, &oset);*/
}

int main(int argc, char *argv[]) {
    int harbor, thread, concurrent;
    char * service_name;
    char service_lib[1024], service_args[1024], service_list[1024], service_path[1024];

    skynet_malloc_init();
    skynet_config_init(argv[1]);
    skynet_config_int("skynet", "harbor", &harbor);
    skynet_config_int("skynet", "thread", &thread);
    skynet_config_string("skynet", "service_path", service_path, sizeof(service_path));
    skynet_config_string("skynet", "service_list", service_list, sizeof(service_list));

    skynet_mq_init();
    skynet_signal_init();
    skynet_harbor_init(harbor);
    skynet_service_init(service_path);
    skynet_timer_init();
    skynet_socket_init();

    service_name = strtok(service_list, ",");
    while (service_name != NULL) {
        skynet_config_int(service_name, "concurrent", &concurrent);
        skynet_config_string(service_name, "lib", service_lib, sizeof(service_lib));
        skynet_config_string(service_name, "args", service_args, sizeof(service_args));
        
        uint32_t handle = skynet_service_create(service_name, harbor, service_lib, service_args, concurrent);
        if (handle == 0) {
            fprintf(stdout, "skynet start failed.\n");
            return 1;
        }
        skynet_handle_namehandle(handle, service_name);

        service_name = strtok(NULL, ",");
    }

    skynet_start(harbor, thread);
    skynet_service_releaseall();
    skynet_harbor_exit();
    skynet_socket_free();
    skynet_config_free();

    fprintf(stdout, "skynet exit.\n");

    return 0;
}
