#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <devices/shutdown.h>
#include <threads/thread.h>
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <userprog/process.h>
#include <devices/input.h>
#include "filesys/inode.h"
#include "filesys/filesys.h"
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f ) 
{
  /* VALUE */	
  int syscall_num;
  int arg[5];
  void *esp = f->esp;
  /* VALUE */
  check_address(esp);
  syscall_num = *(int *)esp;
  
  switch(syscall_num)
  {
	  case SYS_HALT:
		  halt();
		  break;
	  case SYS_EXIT:
		  get_argument(esp,arg,1);
		  exit(arg[0]);
		  break;
	  case SYS_EXEC:
		  get_argument(esp,arg,1);
		  check_address((void *)arg[0]);
		  f->eax = exec((const char *)arg[0]);
		  break;
	  case SYS_WAIT:
		  get_argument(esp,arg,1);
		  f->eax = wait(arg[0]);
		  break;
	  case SYS_CREATE:
		  get_argument(esp,arg,2);
		  check_address((void *)arg[0]);
		  f->eax = create((const char *)arg[0],(unsigned)arg[1]);
		  break;
	  case SYS_REMOVE:
		  get_argument(esp,arg,1);
		  check_address((void *)arg[0]);
		  f->eax=remove((const char *)arg[0]);
		  break;
	  case SYS_OPEN:
		  get_argument(esp,arg,1);
		  check_address((void *)arg[0]);
		  f->eax = open((const char *)arg[0]);
		  break;
	  case SYS_FILESIZE:
		  get_argument(esp,arg,1);
		  f->eax = filesize(arg[0]);
		  break;
	  case SYS_READ:
		  get_argument(esp,arg,3);
		  check_address((void *)arg[1]);
		  f->eax = read(arg[0],(void *)arg[1],(unsigned)arg[2]);
		  break;
	  case SYS_WRITE:
		  get_argument(esp,arg,3);
		  check_address((void *)arg[1]);
		  f->eax = write(arg[0],(void *)arg[1],(unsigned)arg[2]);
		  break;
	  case SYS_SEEK:
		  get_argument(esp,arg,2);
		  seek(arg[0],(unsigned)arg[1]);
		  break;
	  case SYS_TELL:
		  get_argument(esp,arg,1);
		  f->eax = tell(arg[0]);
		  break;
	  case SYS_CLOSE:
		  get_argument(esp,arg,1);
		  close(arg[0]);
		  break;
	  case SYS_ISDIR:
		  get_argument(esp,arg,1);
		  sys_isdir(arg[0]);
		  break;
	  case SYS_MKDIR:
		  get_argument(esp, arg, 1);
		  check_address((void *)arg[0]);
		  f->eax = sys_mkdir((const char *)arg[0]);
		  break;
	  case SYS_READDIR:
		  get_argument(esp, arg, 2);
		  check_address((char *)arg[1]);
		  f->eax = sys_readdir(arg[0], (char *)arg[1]);
		  break;
	  case SYS_CHDIR:
		  get_argument(esp, arg, 1);
		  check_address((void *)arg[0]);
		  f->eax = sys_chdir((const char *)arg[0]);
		  break;
  }
}
/* chack_address function */
void
check_address(void *addr)
{
	uint32_t address=(unsigned int)addr;

	uint32_t lowest_address=0x8048000;
	uint32_t highest_address=0xc0000000;
	if(address >= lowest_address && address < highest_address){
		return;
	}
	else{
		exit(-1);
	}
}
/* get_argument function */
void
get_argument(void *esp, int *arg, int count)
{
	int i;
	void *stack_pointer=esp+4;
	if(count > 0)
	{
		for(i=0; i<count; i++){
			check_address(stack_pointer);
			arg[i] = *(int *)stack_pointer;
			stack_pointer = stack_pointer + 4;
		}
	}
}

/* exit pintos */
void 
halt(void)
{
	shutdown_power_off();
}
/* exit process */
void 
exit(int status)
{
	struct thread *current_process=thread_current();
	
	current_process->process_exit_status = status;     
	printf("%s: exit(%d)\n",current_process->name,status);
	thread_exit();
}
/* wait child process */
int
wait(tid_t tid)
{
	int status;
	status = process_wait(tid);
	return status;
}
/* create file */
bool
create(const char *file, unsigned initial_size)
{
	bool result=false;
	if(filesys_create(file,initial_size)==true)
		result=true;
	return result;
}
/* remove file */
bool
remove(const char *file)
{
	bool result = false;
	if(filesys_remove(file)==true)
		result = true;
	return result;
}
/* create child process and wait until childprocess is loaded */
tid_t
exec(const char *cmd_name)
{
	struct thread *child_process;
	tid_t pid;
	
	pid = process_execute(cmd_name);
	child_process = get_child_process(pid);
    /* wait child process */	
	sema_down(&(child_process->load_semaphore));      
	if(child_process->load_success==true)
		return pid;
	else
	{
		return -1;
	}
}
/* open file and return fd */
int
open(const char *file)
{
	int fd;
	struct file *new_file;
	new_file=filesys_open(file);

	if(new_file != NULL)
	{
		fd = process_add_file(new_file);
		return fd;
	}
	else
	{
		return -1;
	}
}
/* return filesize */
int
filesize(int fd)
{
	int file_size;
	struct file *current_file;
	current_file = process_get_file(fd);
	if(current_file != NULL)
	{
		file_size = file_length(current_file);
		return file_size;
	}
	else
	{
		return -1;
	}
}
/* read file */
int 
read(int fd, void *buffer, unsigned size)
{
	int read_size = 0;
	struct file *current_file;
	char *read_buffer = (char *)buffer;
    
	lock_acquire(&file_lock);

	if(fd == 0)              /* stdin */
	{
		read_buffer[read_size]=input_getc();
		while(read_buffer[read_size] != '\n' && read_size < size)
		{
			read_size++;
			read_buffer[read_size]=input_getc();
		}
		read_buffer[read_size]='\0';
	}
	else
	{
		current_file = process_get_file(fd);
		if(current_file != NULL)
		{
			read_size = file_read(current_file,buffer,size);
		}
	}
	lock_release(&file_lock);
	return read_size;
}
/* write file */
int
write(int fd, void *buffer, unsigned size)
{
	int write_size = 0;
	struct file *current_file;
	struct inode *inode;

	lock_acquire(&file_lock);
	if(fd == 1)                    /* stdout */
	{ 
		putbuf((const char *)buffer,size);
		write_size = size;
	}
	else
	{
		current_file = process_get_file(fd);
		inode = file_get_inode(current_file);
		/* if file is directory, return -1 */
		if( inode_is_dir(inode) == true){
			lock_release(&file_lock);
			return -1;
		}
		if(current_file != NULL)
			write_size = file_write(current_file,(const void *)buffer,size);
	}
	lock_release(&file_lock);
	return write_size;
}
/* move file offset */
void 
seek(int fd, unsigned position)
{
	struct file *current_file;
	current_file = process_get_file(fd);
	
	if(current_file != NULL)
		file_seek(current_file,position);
}
/* return file offset */
unsigned
tell(int fd)
{
	struct file *current_file;
	current_file = process_get_file(fd);
	unsigned offset = 0;

	if(current_file != NULL)
		offset = file_tell(current_file);
	return offset;
}
/* close file */
void 
close(int fd)
{
	struct file *current_file;
	current_file = process_get_file(fd);
	if(current_file != NULL)
	{
		file_close(current_file);
		thread_current()->file_descriptor[fd]=NULL;
	}
}
/* if file is directory, return true */
bool sys_isdir(int fd)
{
	bool result = false;
	struct file *pfile;
	/* get file */
	pfile = process_get_file(fd);
	if(pfile != NULL)
	{
		result = inode_is_dir(file_get_inode(pfile));
	}
	return result;
}
/* change working directory */
bool sys_chdir(const char *dir)
{
	bool result = false;
	struct file *directory = filesys_open(dir);
	if(directory != NULL)
	{
		/* close thread's directory */
		dir_close(thread_current()->thread_dir);
		/* chage thread's directory to new directory */
		thread_current()->thread_dir = dir_open(file_get_inode(directory));
		result = true;
	}
	return result;
}

/* make directory */
bool sys_mkdir(const char *dir)
{
	if(dir==NULL || strlen(dir) == 0)
		return false;
	return filesys_create_dir(dir);
}

/* read directory */
bool sys_readdir(int fd, char *name)
{
	bool result = false;
	struct file *pfile = process_get_file(fd);
	struct inode *inode = file_get_inode(pfile);
	struct dir *dir;
	int offset = 0;
	char entry[100];
	if(pfile != NULL)
	{
		/* if file is not directory, return false */
		if(inode_is_dir(inode) == false )
			return result;
		dir = dir_open(inode);
		/* read directory and store to name */
		while(dir_readdir(dir, entry) == true)
		{
			/* read directory except . and .. */
			if( strcmp(entry,".") == 0 || strcmp(entry,"..") == 0 )
				continue;
			/* copy entry to name */
			strlcpy(&name[offset], entry, strlen(entry)+1);
			offset = strlen(entry) + 1;
		}
		result = true;
	}
	return true;
}

/* get ondisk block sector */
uint32_t sys_inumber(int fd)
{
	struct file *file = process_get_file(fd);
	struct inode *inode;
	if(file != NULL)
	{
		/* return ondisk block sector number */
		inode = file_get_inode(file);
		return inode_get_inumber(inode);
	}
	/* if file is not exist, return -1*/
	return -1;
}
