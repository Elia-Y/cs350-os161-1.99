#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h> 
#include <mips/trapframe.h> 
#include <vfs.h> 
#include <vm.h> 
//#include <limits.h>
#include "opt-A2.h"


#if OPT_A2
int
sys_execv(userptr_t progname, userptr_t args)
{
	struct addrspace *as;
	struct addrspace *old;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	int argc;

	/* Count the number of the arguments and copy into the kernel */
	argc = 0;
	char **temp = ((char **) args);
	char **c = temp;

	while((*c) != NULL) {
	  ++argc;
	  ++c;
	}
	
	//kprintf("The number of arguments is: %d\n", argc);
	char *arglist[argc+1];
	int index = 0;
	char *argu;
	c = temp;
	while((*c) != NULL) {
	  int l = strlen(*c)+1;
          argu = kmalloc(l);
	  memcpy(argu, *c, l);
	  arglist[index] = argu;
	  ++index;
          ++c;
        }

	/* Copy the program path into the kernel */
	

	/* Open the file. */
	result = vfs_open((char *)progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}
//DEBUG(DB_SYSCALL, "Open the file\n");

	/* Save the old as and create a new one */
	as_deactivate();  // do we need this?
	old = curproc_getas();

	/* Create a new address space. */
	as = as_create();
	if (as == NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}
//DEBUG(DB_SYSCALL, "Done loading\n");

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	/* Copy the arguments into the address space */
	vaddr_t argvs[argc+1];  /*arguments*/
	//int ptrLength;
	int argLen;
	
	unsigned int nothing = 0;

	stackptr = ROUNDUP(stackptr, 8);
//kprintf("Round up -> ptrLength now is %d\n", ptrLength);
	//stackptr = stackptr - 4;
	//copyout((void *)&nothing, (userptr_t)stackptr, (size_t)4);
	argvs[argc] = stackptr;

	for (int  i = argc-1; i >= 0; --i) {
	  argLen = strlen(arglist[i]) + 1;
	  argLen = ROUNDUP(argLen, 4);
	  stackptr -= argLen;
//kprintf("argLen here is: %d\n", argLen);
	  copyoutstr(arglist[i], (userptr_t)stackptr, (size_t)argLen, NULL);
	  argvs[i] = stackptr;
	}

	stackptr = stackptr - sizeof(vaddr_t);
        copyout((void *)&nothing, (userptr_t)stackptr, (size_t)4);

	for (int i = argc-1; i >= 0; i--) {
	  stackptr -= sizeof(vaddr_t);
	  copyout(&argvs[i], (userptr_t)stackptr, sizeof(vaddr_t));
        } /*this includes copying NULL char*/

	/* Delete old address space */
	as_destroy(old);

	/* Warp to user mode. */
	enter_new_process(argc /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
#endif

#if OPT_A2
int sys_fork(pid_t *retval, struct trapframe *tf)  {
  KASSERT(curproc != NULL);
DEBUG(DB_SYSCALL, "-------------Enter sys_fork successfully---------\n");
  /* Create process structure for child process 
       (sets up VFS and console later ?)*/
  struct proc *new_proc = proc_create_runprogram(curproc->p_name);
  if (new_proc == NULL) {
	return (ENOMEM);
  }

  /* Create and copy address space */
  struct addrspace *old_as = curproc_getas();
  struct addrspace *new_as;  
  int asr = as_copy(old_as, &new_as);  // where can we find this function? exact definition

  if (asr != 0) {
	proc_destroy(new_proc);
	return (ENOMEM);
  }

   spinlock_acquire(&new_proc->p_lock);
   new_proc->p_addrspace = new_as;
   spinlock_release(&new_proc->p_lock);
DEBUG(DB_SYSCALL, "Finish copying as\n");

  /* Assign PID to child process and create the parent/child relationship 
	pid assignment is finished in proc_create function*/
  new_proc->p_parent = curproc;
  array_init(new_proc->p_children);
 
  spinlock_acquire(&(curproc->p_lock));
  array_add(curproc->p_children, new_proc, NULL);
  spinlock_release(&(curproc->p_lock));

DEBUG(DB_SYSCALL, "Finish assigning PID and building relationship\n");
  /* Create thread for child process */
  struct trapframe *new_tf = kmalloc(sizeof(struct trapframe));
  if (new_tf == NULL) {
	proc_destroy(new_proc);
	return (ENOMEM);
  }
DEBUG(DB_SYSCALL, "------Fork: Process %d has parent Process %d\n------", new_proc->p_id, curproc->p_id);
  //memcpy(new_tf, tf, sizeof(struct trapframe));  // ! needs synchronization if pass pointer directly
  *new_tf = *tf;
  int tr = thread_fork("child process", new_proc, &enter_forked_process, new_tf, 0);
//DEBUG(DB_SYSCALL, "Finish thread creating\n");
  if (tr) {
	kfree(new_tf);
	proc_destroy(new_proc);
	return (tr);
  }

  *retval = new_proc->p_id;
  return 0;  
}
#endif


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
  KASSERT(curproc != NULL);
  struct addrspace *as;
  struct proc *p = curproc;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  //proc_destroy(p);   destroy after waitpid is called or parent is destroyed
  
  /* If child has a parent, wait the parent to exit and destroy for them*/
#if OPT_A2

  if(p->p_parent != NULL) {
DEBUG(DB_SYSCALL, "Process %d wants to _exit; however, it has parent %d\n", p->p_id, p->p_parent->p_id);
    p->exited = true;
    p->exitcode = _MKWAIT_EXIT(exitcode);   // need to pass it to parent

    lock_acquire(p->p_cvlock);
    cv_broadcast(p->p_cv, p->p_cvlock);
    lock_release(p->p_cvlock);
  } else {
DEBUG(DB_SYSCALL, "Process %d will _exit because it has no parent\n", p->p_id);
    proc_destroy(p);
  }

#else
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;
  proc_destroy(p);
#endif

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
  *retval = curproc->p_id;
#else
  *retval = 1;
#endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

#if OPT_A2
  KASSERT(curproc != NULL);

  struct proc *child = NULL;
  for(unsigned i = 0; i < array_num(curproc->p_children); i++) {
    pid_t temp = ((struct proc *)array_get(curproc->p_children, i))->p_id;
    if (temp == pid) {
	child = array_get(curproc->p_children, i);
	break;
    }
  }

  if (child == NULL) {
    panic("Invalid waitpid call on wrong child");
    return (ECHILD);
  }

  lock_acquire(child->p_cvlock);
  while (child->exited == false) {
    cv_wait(child->p_cv, child->p_cvlock);
  }
  exitstatus = child->exitcode;
  lock_release(child->p_cvlock);

  result =  copyout((void *)&exitstatus, status, sizeof(int));
  if (result) {
	return result;
  }

  *retval = pid;
  //proc_destroy(child);
#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }

  *retval = pid;
#endif
  return(0);
}

