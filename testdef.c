#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "signal.h"

int main(){
  char c;
  int pid = fork(); 
  if (pid == 0){
    signal(SIGSTOP, (sighandler_t)SIG_DFL);
    read(0, &c, 1); 
    printf(1,"child stopped\n");
    printf(1,"This is the child\n");


  }
  else{
    printf (1, "in parent\n");
    sendkill(pid, SIGSTOP); 
    read(0, &c, 1);
    wait();
    printf (1, "parent over\n");
  }
  exit();
}
