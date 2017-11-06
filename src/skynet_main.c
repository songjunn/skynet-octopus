#include "skynet.h"
#include "skynet_mq.h"
#include "skynet_server.h"
#include "skynet_service.h"
#include "skynet_config.h"
#include "skynet_timer.h"
#include "skynet_socket.h"
#include "skynet_logger.h"

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
    pthread_cond_t cond;
    pthread_mutex_t mutex;
};

static struct monitor *m = NULL;

void create_thread(pthread_t *thread, void *(*start_routine) (void *), void *arg) {
    if (pthread_create(thread,NULL, start_routine, arg)) {
        skynet_logger_error(NULL, "Create thread failed");
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
                    skynet_logger_error(NULL, "unlock mutex error");
                    exit(1);
                }
            }
        }
    }
    return NULL;
}

void skynet_start(unsigned harbor, unsigned thread) {
    unsigned i;
    pthread_t pid[thread+2];

    m = skynet_malloc(sizeof(*m));
    memset(m, 0, sizeof(*m));
    m->count = thread;
    m->sleep = 0;
    m->quit = 0;

    if (pthread_mutex_init(&m->mutex, NULL)) {
        skynet_logger_error(NULL, "Init mutex error");
        exit(1);
    }
    if (pthread_cond_init(&m->cond, NULL)) {
        skynet_logger_error(NULL, "Init cond error");
        exit(1);
    }

    for (i=0;i<thread;i++) {
        create_thread(&pid[i], thread_worker, m);
    }
    create_thread(&pid[i++], thread_timer, m);
    create_thread(&pid[i], thread_socket, m);

    skynet_logger_notice(NULL, "skynet start, harbor:%u workers:%u", harbor, thread);

    for (i=0;i<thread;i++) {
        pthread_join(pid[i], NULL); 
    }

    skynet_logger_notice(NULL, "skynet shutdown, harbor:%u", harbor);

    pthread_mutex_destroy(&m->mutex);
    pthread_cond_destroy(&m->cond);
    skynet_free(m);
}

void skynet_shutdown(int signal) {
    skynet_logger_notice(NULL, "recv signal:%d", signal);

    m->quit = 1;

    // wakeup socket thread
    skynet_socket_exit();
    
    // wakeup all worker thread
    pthread_mutex_lock(&m->mutex);
    pthread_cond_broadcast(&m->cond);
    pthread_mutex_unlock(&m->mutex);

    // SIGTERM for normal exit, otherwise make coredump 
    if (signal != SIGTERM) {
        abort();
    }
}

void skynet_signal_init() {
    struct sigaction act;
    act.sa_handler = skynet_shutdown;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGFPE, &act, NULL);
    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGKILL, &act, NULL);
    sigaction(SIGXFSZ, &act, NULL);

    // block SIGINT to all child process:
    sigset_t bset, oset;
    sigemptyset(&bset);
    sigaddset(&bset, SIGINT);
    // equivalent to sigprocmask
    pthread_sigmask(SIG_BLOCK, &bset, &oset);
}

int main(int argc, char *argv[]) {
    int harbor, thread, concurrent;
    char * service_name = NULL;
    char * service_args = skynet_malloc(1024);
    char * service_list = skynet_malloc(1024);
    char * service_path = skynet_malloc(1024);
    char * logger_args = skynet_malloc(1024);

    skynet_config_init(argv[1]);
    skynet_config_int("skynet", "harbor", &harbor);
    skynet_config_int("skynet", "thread", &thread);
    skynet_config_string("skynet", "logger_args", logger_args, 1024);
    skynet_config_string("skynet", "service_path", service_path, 1024);
    skynet_config_string("skynet", "service_list", service_list, 1024);

    skynet_mq_init();
    skynet_signal_init();
    skynet_service_init(service_path);
    skynet_logger_init(harbor, logger_args); // logger must be the first service
    skynet_timer_init();
    skynet_socket_init();

    service_name = strtok(service_list, ",");
    while (service_name != NULL) {
        skynet_config_int(service_name, "concurrent", &concurrent);
        skynet_config_string(service_name, "args", service_args, 1024);
        skynet_service_create(service_name, harbor, service_args, concurrent);

        service_name = strtok(NULL, ",");
    }

    skynet_start(harbor, thread);
    skynet_service_releaseall();
    skynet_socket_free();
    skynet_config_free();
    skynet_free(logger_args);
    skynet_free(service_list);
    skynet_free(service_args);
    skynet_free(service_path);

    printf("skynet exit.");

    return 0;
}
