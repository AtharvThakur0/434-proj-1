#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define N 4
#define M 8

int sigs[M] = {
    SIGINT, SIGABRT, SIGILL, SIGCHLD,
    SIGSEGV, SIGFPE, SIGHUP, SIGTSTP
};

pid_t kids[N];

void h(int sig, siginfo_t *info, void *u) {
    printf("caught sig=%d recv=%d send=%d\n",
           sig,
           getpid(),
           info->si_pid);
    fflush(stdout);
}

void set_ignore_all() {
    for (int i = 0; i < M; i++) {
        signal(sigs[i], SIG_IGN);
    }
}

void set_default_all() {
    for (int i = 0; i < M; i++) {
        signal(sigs[i], SIG_DFL);
    }
}

void child_job(int id) {
    struct sigaction a;
    sigset_t always;

    sigemptyset(&always);

    if (id == 0) {
        sigaddset(&always, SIGHUP);
        sigaddset(&always, SIGTSTP);
    } else if (id == 1) {
        sigaddset(&always, SIGINT);
        sigaddset(&always, SIGABRT);
    } else if (id == 2) {
        sigaddset(&always, SIGILL);
        sigaddset(&always, SIGFPE);
    } else {
        sigaddset(&always, SIGSEGV);
        sigaddset(&always, SIGCHLD);
    }

    sigprocmask(SIG_BLOCK, &always, NULL);

    for (int i = 0; i < M; i++) {
        signal(sigs[i], SIG_IGN);
    }

    sigemptyset(&a.sa_mask);
    a.sa_sigaction = h;
    a.sa_flags = SA_SIGINFO;

    if (id == 0) {
        sigaddset(&a.sa_mask, SIGILL);
        sigaddset(&a.sa_mask, SIGFPE);

        sigaction(SIGINT, &a, NULL);
        sigaction(SIGABRT, &a, NULL);
        sigaction(SIGILL, &a, NULL);
        sigaction(SIGFPE, &a, NULL);
    } else if (id == 1) {
        sigaddset(&a.sa_mask, SIGSEGV);
        sigaddset(&a.sa_mask, SIGHUP);

        sigaction(SIGILL, &a, NULL);
        sigaction(SIGCHLD, &a, NULL);
        sigaction(SIGSEGV, &a, NULL);
        sigaction(SIGHUP, &a, NULL);
    } else if (id == 2) {
        sigaddset(&a.sa_mask, SIGINT);
        sigaddset(&a.sa_mask, SIGTSTP);

        sigaction(SIGINT, &a, NULL);
        sigaction(SIGSEGV, &a, NULL);
        sigaction(SIGHUP, &a, NULL);
        sigaction(SIGTSTP, &a, NULL);
    } else {
        sigaddset(&a.sa_mask, SIGABRT);
        sigaddset(&a.sa_mask, SIGFPE);

        sigaction(SIGABRT, &a, NULL);
        sigaction(SIGCHLD, &a, NULL);
        sigaction(SIGFPE, &a, NULL);
        sigaction(SIGTSTP, &a, NULL);
    }

    long long sum = 0;
    int top = 10 * getpid();

    for (int i = 0; i <= top; i++) {
        sum += i;

        if (i % 2000 == 0) {
            printf("child=%d pid=%d sum=%lld\n", id, getpid(), sum);
            fflush(stdout);
            sleep(10);
        }
    }

    printf("done child=%d pid=%d sum=%lld\n", id, getpid(), sum);
    fflush(stdout);

    exit(0);
}

int main() {
    srand(time(NULL));

    set_ignore_all();

    for (int i = 0; i < N; i++) {
        kids[i] = fork();

        if (kids[i] == 0) {
            child_job(i);
        }
    }

    signal(SIGCHLD, SIG_DFL);

    sleep(2);

    for (int i = 0; i < 16; i++) {
        int c = rand() % N;
        int s = sigs[rand() % M];

        printf("parent sends sig=%d to pid=%d\n", s, kids[c]);
        fflush(stdout);

        kill(kids[c], s);
        sleep(1);
    }

    for (int i = 0; i < N; i++) {
        waitpid(kids[i], NULL, 0);
    }

    set_default_all();

    printf("parent default restored pid=%d\n", getpid());
    fflush(stdout);

    sleep(20);

    return 0;
}
