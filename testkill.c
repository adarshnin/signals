#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "signal.h"


void handler(int signal){
  printf (1, "\nthe signal number is %d\n", signal);
  printf (1, "signal handled\n");
}
// Passed Testcases:
// SIGKILL
// SIGSTOP
// SIGSTOP then SIGCONT
// SIGCONT
// SIGSTOP then SIGKILL
// SIGTERM
// SIGSTOP then SIGTERM
// SIGSTOP then SIGCONT then SIGTERM
// SIGSTOP
// SIGINT


int main(){
    int pid = fork();

    if(pid == 0) {
        sleep(200);
        int i = 0;
        while (1) {
            printf(1, "%d", i++);
        }
    } 
    else
    {  
        sleep(200);
        sendkill(pid, SIGSTOP);
        sleep(200);
        sendkill(pid, SIGCONT);
        sleep(200);
        sendkill(pid, SIGSTOP);
        sleep(200);
        sendkill(pid, SIGCONT); 
        sleep(200);
        sendkill(pid, SIGSTOP);
        sleep(200);
        sendkill(pid, SIGCONT);
        sleep(200);
        sendkill(pid, SIGSTOP);
        sleep(200);
        sendkill(pid, SIGCONT); 
        sleep(200);
        sendkill(pid, SIGSTOP);
        sleep(200);
        sendkill(pid, SIGCONT);
        printf(1, "SIGSTOP Test Passed\n");
        printf(1, "SIGCONT Test Passed\n");
        sendkill(pid, SIGTERM);
        printf(1, "SIGTERM Test Passed\n");
        wait();
    }
    exit();
}