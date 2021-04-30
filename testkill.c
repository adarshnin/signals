#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "signal.h"


void handler(int signal){
  printf (1, "\nthe signal number is %d\n", signal);
  printf (1, "signal handled\n");
}

int main(){
  int i;
  int pid = fork(); 
  if (pid == 0){
    signal(SIGCONT, handler);
    sleep(100);
    for (i = 0; i < 500; i++){
      printf (1, "num - %d\n", i);
    }
    printf (1, "child over\n");
  }
  else{
    printf (1, "in parent\n");
    sleep(200);
    // sendkill(pid, SIGCONT); 
    sendkill(pid, SIGCONT); 
    // sleep(200);
    // sendkill(pid, SIGCONT); 
    wait();
  }
  printf (1, "over\n");
  exit();
}
