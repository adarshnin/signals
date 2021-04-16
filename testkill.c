#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "signal.h"


void handler(int signal){
  printf (1, "signal handled\n");
}

int main(){
  char c;
  int pid = fork(); 
  if (pid == 0){
    signal(SIGCONT, handler);
    read(0, &c, 1);
  }
  else{
    printf (1, "in parent\n");
    read(0, &c, 1);
    sendkill(pid, SIGCONT); 
    wait();
    printf (1, "parent over\n");
  }
  exit();
}
