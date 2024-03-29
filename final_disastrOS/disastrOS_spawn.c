#include <ucontext.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include "disastrOS.h"
#include "disastrOS_syscalls.h"
#include "disastrOS_signals.h"

// creates a new instance of the running_process
// and puts it in the ready list
// returns the pid of the child
// it starts a function in the form of void f();
void internal_spawn(){
  static PCB* new_pcb;
  new_pcb=PCB_alloc();
  if (!new_pcb) {
    running->syscall_retvalue=DSOS_ESPAWN;
    return;
  }

  new_pcb->status=Ready;

  // sets the parent of the newly created process to the running process
  new_pcb->parent=running;
  
  // adds a pointer to the new process to the children list of running
  PCBPtr* new_pcb_ptr=PCBPtr_alloc(new_pcb);
  assert(new_pcb_ptr);
  List_insert(&running->children, running->children.last, (ListItem*) new_pcb_ptr);

  //adds the new process to the ready queue
  List_insert(&ready_list, ready_list.last, (ListItem*) new_pcb);

  //sets the retvalue for the caller to the new pid
  running->syscall_retvalue=new_pcb->pid;

  getcontext(&new_pcb->cpu_state);
  new_pcb->cpu_state.uc_stack.ss_sp = new_pcb->stack;
  new_pcb->cpu_state.uc_stack.ss_size = STACK_SIZE;
  new_pcb->cpu_state.uc_stack.ss_flags = 0;
  sigemptyset(&new_pcb->cpu_state.uc_sigmask);
  new_pcb->cpu_state.uc_link = &main_context;
  
  void (*new_function) (void*)= (void(*)(void*))  running->syscall_args[0];
  makecontext(&new_pcb->cpu_state, (void(*)())  new_function, 1, (void*)running->syscall_args[1]);
    
  // at creating signals contexts from an existing one
  for(int i=0; i<DEFINED_SIG; i++){
	new_pcb->signal_context[i]=new_pcb->cpu_state;
	new_pcb->signal_context[i].uc_stack.ss_sp = new_pcb->signal_stack;
	new_pcb->signal_context[i].uc_stack.ss_size = STACK_SIZE;
	sigemptyset(&new_pcb->signal_context[i].uc_sigmask);
	sigaddset(&new_pcb->signal_context[i].uc_sigmask, SIGALRM);
	new_pcb->signal_context[i].uc_stack.ss_flags=0;
	new_pcb->signal_context[i].uc_link=&main_context;
  }
  // at manually making context for each installed signals
  makecontext(&new_pcb->signal_context[DSOS_SIGCHLD-1], disastrOS_SIGCHLD_handler, 0);
  makecontext(&new_pcb->signal_context[DSOS_SIGHUP-1], disastrOS_SIGHUP_handler, 0);
}
