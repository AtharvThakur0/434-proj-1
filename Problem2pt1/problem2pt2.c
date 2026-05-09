#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#define KIDS 4
#define SIGCNT 9

int sigs[SIGCNT] = {
    SIGINT, SIGABRT, SIGILL, SIGCHLD, SIGSEGV,
    SIGFPE, SIGHUP, SIGTSTP, SIGQUIT
};

pid_t kids[KIDS];

void handler(int sig, siginfo_t *info, void *u) {
    (void)u;

    printf("handler: pid=%d caught sig=%d from=%d\n",
           getpid(), sig, info->si_pid);
    fflush(stdout);
}

void add_first_group(sigset_t *set) {
    sigaddset(set, SIGINT);
    sigaddset(set, SIGQUIT);
    sigaddset(set, SIGTSTP);
}

void add_second_group(sigset_t *set) {
    sigaddset(set, SIGABRT);
    sigaddset(set, SIGILL);
    sigaddset(set, SIGCHLD);
    sigaddset(set, SIGSEGV);
    sigaddset(set, SIGFPE);
    sigaddset(set, SIGHUP);
}

void install_handlers(void) {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;

    for (int i = 0; i < SIGCNT; i++) {
        sigaction(sigs[i], &sa, NULL);
    }
}

void print_pending(const char *name) {
    sigset_t pending;

    sigpending(&pending);

    printf("pending check for %s pid=%d:", name, getpid());

    for (int i = 0; i < SIGCNT; i++) {
        if (sigismember(&pending, sigs[i])) {
            printf(" %d", sigs[i]);
        }
    }

    printf("\n");
    fflush(stdout);
}

void drain_with_sigtimedwait(sigset_t *set, const char *name) {
    struct timespec wait_time;
    siginfo_t info;
    int sig;

    wait_time.tv_sec = 0;
    wait_time.tv_nsec = 100000000;

    while (1) {
        sig = sigtimedwait(set, &info, &wait_time);

        if (sig == -1) {
            if (errno == EAGAIN) {
                printf("%s pid=%d: no more pending blocked signals\n",
                       name, getpid());
                fflush(stdout);
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            printf("%s pid=%d: sigtimedwait error: %s\n",
                   name, getpid(), strerror(errno));
            fflush(stdout);
            break;
        }

        printf("%s pid=%d: sigtimedwait got sig=%d from=%d\n",
               name, getpid(), sig, info.si_pid);
        fflush(stdout);
    }
}

void child_code(int id) {
    sigset_t blocked;
    long long total = 0;
    int top = 20000 + id * 2000;

    sigemptyset(&blocked);
    install_handlers();

    if (id < 2) {
        add_first_group(&blocked);
        printf("child %d pid=%d blocks SIGINT SIGQUIT SIGTSTP\n",
               id, getpid());
    } else {
        add_second_group(&blocked);
        printf("child %d pid=%d blocks the other assignment signals\n",
               id, getpid());
    }

    fflush(stdout);
    sigprocmask(SIG_SETMASK, &blocked, NULL);

    for (int i = 0; i <= top; i++) {
        total += i;

        if (i % 5000 == 0) {
            printf("child %d pid=%d total=%lld\n", id, getpid(), total);
            fflush(stdout);
            usleep(250000);
        }
    }

    sleep(10);

    print_pending("child");
    drain_with_sigtimedwait(&blocked, "child");
    print_pending("child after wait");

    printf("child %d pid=%d done total=%lld\n", id, getpid(), total);
    fflush(stdout);

    exit(0);
}

void parent_wait_examples(sigset_t *set) {
    siginfo_t info;
    struct timespec wait_time;
    int sig;

    print_pending("parent before waits");

    sig = sigwaitinfo(set, &info);
    if (sig > 0) {
        printf("parent pid=%d: sigwaitinfo got sig=%d from=%d\n",
               getpid(), sig, info.si_pid);
    }
    fflush(stdout);

    wait_time.tv_sec = 1;
    wait_time.tv_nsec = 0;

    sig = sigtimedwait(set, &info, &wait_time);
    if (sig > 0) {
        printf("parent pid=%d: sigtimedwait got sig=%d from=%d\n",
               getpid(), sig, info.si_pid);
    } else {
        printf("parent pid=%d: sigtimedwait found nothing else\n", getpid());
    }
    fflush(stdout);

    if (sigwait(set, &sig) == 0) {
        printf("parent pid=%d: sigwait got sig=%d\n", getpid(), sig);
    }
    fflush(stdout);

    print_pending("parent after waits");
}

int main(void) {
    sigset_t parent_block;

    sigemptyset(&parent_block);
    add_first_group(&parent_block);

    install_handlers();

    sigprocmask(SIG_BLOCK, &parent_block, NULL);

    printf("parent pid=%d blocks SIGINT SIGQUIT SIGTSTP before fork\n",
           getpid());
    fflush(stdout);

    for (int i = 0; i < KIDS; i++) {
        kids[i] = fork();

        if (kids[i] < 0) {
            perror("fork");
            exit(1);
        }

        if (kids[i] == 0) {
            child_code(i);
        }
    }

    sleep(1);

    printf("\nparent sends each signal 3 times to itself\n");
    fflush(stdout);

    for (int i = 0; i < SIGCNT; i++) {
        for (int j = 0; j < 3; j++) {
            kill(getpid(), sigs[i]);
        }
    }

    parent_wait_examples(&parent_block);

    printf("\nparent sends each signal 3 times to every child\n");
    fflush(stdout);

    for (int c = 0; c < KIDS; c++) {
        for (int i = 0; i < SIGCNT; i++) {
            for (int j = 0; j < 3; j++) {
                printf("parent pid=%d sends sig=%d to child pid=%d\n",
                       getpid(), sigs[i], kids[c]);
                fflush(stdout);

                kill(kids[c], sigs[i]);
                usleep(50000);
            }
        }
    }

    for (int i = 0; i < KIDS; i++) {
        int st;

        waitpid(kids[i], &st, 0);

        if (WIFEXITED(st)) {
            printf("parent: child pid=%d exited status=%d\n",
                   kids[i], WEXITSTATUS(st));
        } else if (WIFSIGNALED(st)) {
            printf("parent: child pid=%d died from sig=%d\n",
                   kids[i], WTERMSIG(st));
        }

        fflush(stdout);
    }

    sigprocmask(SIG_UNBLOCK, &parent_block, NULL);

    printf("parent pid=%d unblocked its signals and finished\n", getpid());
    fflush(stdout);

    return 0;
}
