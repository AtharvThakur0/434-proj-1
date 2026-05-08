#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define NUM_KIDS 4
#define NUM_SIGS 8

int allsig[NUM_SIGS] = {
    SIGINT, SIGABRT, SIGILL, SIGCHLD,
    SIGSEGV, SIGFPE, SIGHUP, SIGTSTP
};

pid_t childList[NUM_KIDS];

void catch_it(int signo, siginfo_t *stuff, void *x)
{
    printf("sig %d caught by %d from %d\n",
        signo, getpid(), stuff->si_pid);
    fflush(stdout);
}

void ignoreSigs()
{
    int k;

    for(k = 0; k < NUM_SIGS; k++) {
        signal(allsig[k], SIG_IGN);
    }
}

void defaultSigs()
{
    int k;

    for(k = 0; k < NUM_SIGS; k++) {
        signal(allsig[k], SIG_DFL);
    }
}

void do_child(int num)
{
    struct sigaction act;
    sigset_t blocked;
    int j;
    long long total = 0;
    int lim;

    sigemptyset(&blocked);

    if(num == 0) {
        sigaddset(&blocked, SIGHUP);
        sigaddset(&blocked, SIGTSTP);
    }
    else if(num == 1) {
        sigaddset(&blocked, SIGINT);
        sigaddset(&blocked, SIGABRT);
    }
    else if(num == 2) {
        sigaddset(&blocked, SIGILL);
        sigaddset(&blocked, SIGFPE);
    }
    else {
        sigaddset(&blocked, SIGSEGV);
        sigaddset(&blocked, SIGCHLD);
    }

    sigprocmask(SIG_BLOCK, &blocked, NULL);

    for(j = 0; j < NUM_SIGS; j++) {
        signal(allsig[j], SIG_IGN);
    }

    sigemptyset(&act.sa_mask);
    act.sa_sigaction = catch_it;
    act.sa_flags = SA_SIGINFO;

    if(num == 0) {
        sigaddset(&act.sa_mask, SIGILL);
        sigaddset(&act.sa_mask, SIGFPE);

        sigaction(SIGINT,  &act, NULL);
        sigaction(SIGABRT, &act, NULL);
        sigaction(SIGILL,  &act, NULL);
        sigaction(SIGFPE,  &act, NULL);
    }
    else if(num == 1) {
        sigaddset(&act.sa_mask, SIGSEGV);
        sigaddset(&act.sa_mask, SIGHUP);

        sigaction(SIGILL,  &act, NULL);
        sigaction(SIGCHLD, &act, NULL);
        sigaction(SIGSEGV, &act, NULL);
        sigaction(SIGHUP,  &act, NULL);
    }
    else if(num == 2) {
        sigaddset(&act.sa_mask, SIGINT);
        sigaddset(&act.sa_mask, SIGTSTP);

        sigaction(SIGINT,  &act, NULL);
        sigaction(SIGSEGV, &act, NULL);
        sigaction(SIGHUP,  &act, NULL);
        sigaction(SIGTSTP, &act, NULL);
    }
    else {
        sigaddset(&act.sa_mask, SIGABRT);
        sigaddset(&act.sa_mask, SIGFPE);

        sigaction(SIGABRT, &act, NULL);
        sigaction(SIGCHLD, &act, NULL);
        sigaction(SIGFPE,  &act, NULL);
        sigaction(SIGTSTP, &act, NULL);
    }

    lim = 10 * getpid();

    for(j = 0; j <= lim; j++) {
        total = total + j;

        if(j % 2000 == 0) {
            printf("child %d pid %d total %lld\n", num, getpid(), total);
            fflush(stdout);
            sleep(10);
        }
    }

    printf("child %d done pid %d total %lld\n", num, getpid(), total);
    fflush(stdout);

    exit(0);
}

int main()
{
    int i;

    srand(time(NULL));

    ignoreSigs();

    for(i = 0; i < NUM_KIDS; i++) {
        childList[i] = fork();

        if(childList[i] == 0) {
            do_child(i);
        }
    }

    signal(SIGCHLD, SIG_DFL);

    sleep(2);

    for(i = 0; i < 16; i++) {
        int whichKid = rand() % NUM_KIDS;
        int whichSig = allsig[rand() % NUM_SIGS];

        printf("send %d to %d\n", whichSig, childList[whichKid]);
        fflush(stdout);

        kill(childList[whichKid], whichSig);

        sleep(1);
    }

    for(i = 0; i < NUM_KIDS; i++) {
        waitpid(childList[i], NULL, 0);
    }

    defaultSigs();

    printf("parent back to default pid %d\n", getpid());
    fflush(stdout);

    sleep(20);

    return 0;
}
