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
	signal(SIGCONT, handler);
        int i = 0;
        while (1) {
            printf(1,"%d\n",i++);
        }
    } 
    else
    {
            sleep(200);
            sendkill(pid, SIGCONT);
        wait();
    }
    // int ret = 0;

    // int pid = fork();

    // if(pid == 0) {
    //     ret = pause();
    //     if (-1 == ret)
    //         printf(1,"Process exited\n");
    //     } 
    // else
    // {
    //     printf(1,"Parent\n");

    //     sendkill(pid, SIGTERM);
    //     wait();
    // }
   
    
    exit();
}
