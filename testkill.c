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
    int ret = 0;
    int pid;
  pid = fork();

  if(pid == 0) {
    printf(1, "pause() test\n");
    ret = pause();
    if (-1 == ret)
      printf(1, "pause() test ok\n");
    } 
  else
  {
    sleep(200);
    // printf(1,"pid = %d\n", pid);
    sendkill(pid, SIGTERM);
    wait();
  }
          // sleep(200);
        //     sendkill(pid, SIGSTOP);
        //     sendkill(pid, SIGCONT);
        //     sendkill(pid, SIGSTOP);
        //     sendkill(pid, SIGCONT);
        //     sendkill(pid, SIGSTOP);
        //     sendkill(pid, SIGCONT);
        //     sendkill(pid, SIGTERM);
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
