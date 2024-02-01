#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include <string.h>
#include <filesys/file.h>
#include <devices/input.h>
#include <threads/malloc.h>
#include <threads/palloc.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "process.h"
#include "pagedir.h"
#include <threads/vaddr.h>
#include <filesys/filesys.h>
# define max_syscall 20
# define USER_VADDR_BOUND (void*) 0x08048000
/* Our implementation for storing the array of system calls for Task2 and Task3 */
static void (*syscalls[max_syscall])(struct intr_frame *);

/* Our implementation for Task2: syscall halt,exec,wait and practice */
void sys_halt(struct intr_frame* f); /* syscall halt. */
void sys_exit(struct intr_frame* f); /* syscall exit. */
void sys_exec(struct intr_frame* f); /* syscall exec. */

/* Our implementation for Task3: syscall create, remove, open, filesize, read, write, seek, tell, and close */
void sys_create(struct intr_frame* f); /* syscall create */
void sys_remove(struct intr_frame* f); /* syscall remove */
void sys_open(struct intr_frame* f);/* syscall open */
void sys_wait(struct intr_frame* f); /*syscall wait */
void sys_filesize(struct intr_frame* f);/* syscall filesize */
void sys_read(struct intr_frame* f);  /* syscall read */
void sys_write(struct intr_frame* f); /* syscall write */
void sys_seek(struct intr_frame* f); /* syscall seek */
void sys_tell(struct intr_frame* f); /* syscall tell */
void sys_close(struct intr_frame* f); /* syscall close */
static void syscall_handler (struct intr_frame *);
struct thread_file * find_file_id(int fd);


void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /* Our implementation for Task2: initialize halt,exit,exec */
  syscalls[SYS_HALT] = &sys_halt;
  syscalls[SYS_EXIT] = &sys_exit;
  syscalls[SYS_EXEC] = &sys_exec;
  /* Our implementation for Task3: initialize create, remove, open, filesize, read, write, seek, tell, and close */
  syscalls[SYS_WAIT] = &sys_wait;
  syscalls[SYS_CREATE] = &sys_create;
  syscalls[SYS_REMOVE] = &sys_remove;
  syscalls[SYS_OPEN] = &sys_open;
  syscalls[SYS_WRITE] = &sys_write;
  syscalls[SYS_SEEK] = &sys_seek;
  syscalls[SYS_TELL] = &sys_tell;
  syscalls[SYS_CLOSE] =&sys_close;
  syscalls[SYS_READ] = &sys_read;
  syscalls[SYS_FILESIZE] = &sys_filesize;

}

/* Phương thức trong tài liệu để xử lý tình huống đặc biệt */
static int 
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Phương thức mới để kiểm tra địa chỉ và các trang vượt qua test sc-bad-boundary2, execute */
void * 
check_ptr2(const void *vaddr)
{ 
  /* Judge address */
  if (!is_user_vaddr(vaddr))
  {
    exit_special ();
  }
  /* Judge the page */
  void *ptr = pagedir_get_page (thread_current()->pagedir, vaddr);
  if (!ptr)
  {
    exit_special ();
  }
  /* Đánh giá nội dung trang */
  uint8_t *check_byteptr = (uint8_t *) vaddr;
  for (uint8_t i = 0; i < 4; i++) 
  {
    if (get_user(check_byteptr + i) == -1)
    {
      exit_special ();
    }
  }

  return ptr;
}


/* Our implementation for Task2: halt,exit,exec */
/* Do sytem halt */
/* Sử dụng hàm shutdown_power_ off(void) kết thúc pintos*/
void 
sys_halt (struct intr_frame* f)
{
  shutdown_power_off();
}

/* Do sytem exit */
/*     Mục đích: Hàm syscall này được gọi khi một tiến trình kết thúc (exit).
       Thực hiện:
        Lấy giá trị truyền vào từ user stack, đại diện cho trạng thái thoát của tiến trình (exit status).
        Lưu giữ giá trị trạng thái thoát trong biến st_exit của tiến trình hiện tại (thread_current()).
        Gọi hàm thread_exit() để kết thúc tiến trình.*/
void 
sys_exit (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  /* record the exit status of the process */
  thread_current()->st_exit = *user_ptr;
  thread_exit ();
}

/* Do sytem exec */
/*     Mục đích: Hàm syscall này được sử dụng để thực hiện một chương trình mới.
       Thực hiện:
        Nó lấy dòng lệnh từ ngăn xếp người dùng.
        Gọi process_execute với dòng lệnh để bắt đầu thực hiện một tiến trình mới.
        Đặt giá trị trả về (f->eax) là ID của tiến trình mới được tạo. */
void 
sys_exec (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  check_ptr2 (*(user_ptr + 1));
  *user_ptr++;
  f->eax = process_execute((char*)* user_ptr);
}

/* Do sytem wait */
/*     Mục đích: Hàm syscall này chịu trách nhiệm chờ đợi cho đến khi một tiến trình con thoát và sau đó lấy thông tin trạng thái thoát của tiến trình con đó.
       Thực hiện:
        Nó lấy ID của tiến trình con từ ngăn xếp người dùng.
        Gọi hàm process_wait với ID của tiến trình con để chờ đợi cho đến khi tiến trình con thoát.
        Đặt giá trị trả về (f->eax) là trạng thái thoát của tiến trình con.*/
void 
sys_wait (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  f->eax = process_wait(*user_ptr);
}

/*Our implementation for Task3: create, remove, open, filesize, read, write, seek, tell, and close */

/*     Mục đích: Hàm syscall này chịu trách nhiệm tạo một tệp mới.
       Thực hiện:
        Nó lấy tên tệp và kích thước ban đầu từ ngăn xếp người dùng.
        Gọi filesys_create để tạo tệp với tên và kích thước ban đầu đã chỉ định.
        Đặt giá trị trả về (f->eax) là true nếu quá trình tạo tệp thành công, ngược lại là false. */
void 
sys_create(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 5);
  check_ptr2 (*(user_ptr + 4));
  *user_ptr++;
  acquire_lock_f ();
  f->eax = filesys_create ((const char *)*user_ptr, *(user_ptr+1));
  release_lock_f ();
}

/*     Mục đích: Hàm syscall này được sử dụng để xóa một tệp.
       Thực hiện:
        Nó lấy tên tệp từ ngăn xếp người dùng.
        Gọi filesys_remove để xóa tệp có tên đã chỉ định.
        Đặt giá trị trả về (f->eax) là true nếu quá trình xóa tệp thành công, ngược lại là false. */
void 
sys_remove(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  check_ptr2 (*(user_ptr + 1));
  *user_ptr++;
  acquire_lock_f ();
  f->eax = filesys_remove ((const char *)*user_ptr);
  release_lock_f ();
}

/*     Mục đích: Hàm syscall này chịu trách nhiệm mở một tệp đã tồn tại.
       Thực hiện:
        Nó lấy tên tệp từ ngăn xếp người dùng.
        Gọi filesys_open để mở tệp có tên đã chỉ định.
        Nếu tệp được mở thành công, nó tạo một mục mới trong danh sách tệp của luồng hiện tại,
        gán một bộ số định danh tệp và đặt giá trị trả về (f->eax) là số định danh tệp đã được gán. Nếu không mở được tệp, nó đặt giá trị trả về là -1. */
void 
sys_open (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  check_ptr2 (*(user_ptr + 1));
  *user_ptr++;
  acquire_lock_f ();
  struct file * file_opened = filesys_open((const char *)*user_ptr);
  release_lock_f ();
  struct thread * t = thread_current();
  if (file_opened)
  {
    struct thread_file *thread_file_temp = malloc(sizeof(struct thread_file));
    thread_file_temp->fd = t->file_fd++;
    thread_file_temp->file = file_opened;
    list_push_back (&t->files, &thread_file_temp->file_elem);
    f->eax = thread_file_temp->fd;
  } 
  else
  {
    f->eax = -1;
  }
}
/*     Mục đích: Hàm syscall này được sử dụng để ghi dữ liệu vào một tệp hoặc vào bảng điều khiển.
       Thực hiện:
        Nó lấy số định danh tệp, bộ đệm và kích thước từ ngăn xếp người dùng.
        Nếu số định danh tệp là 1 (stdout), nó sử dụng putbuf để ghi vào bảng điều khiển. Nếu không, nó ghi vào tệp liên quan đến số định danh tệp đã chỉ định.
        Đặt giá trị trả về (f->eax) là số byte thực sự đã được ghi.*/
void 
sys_write (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 7);
  check_ptr2 (*(user_ptr + 6));
  *user_ptr++;
  int temp2 = *user_ptr;
  const char * buffer = (const char *)*(user_ptr+1);
  off_t size = *(user_ptr+2);
  if (temp2 == 1) {
    /* Use putbuf to do testing */
    putbuf(buffer,size);
    f->eax = size;
  }
  else
  {
    /* Write to Files */
    struct thread_file * thread_file_temp = find_file_id (*user_ptr);
    if (thread_file_temp)
    {
      acquire_lock_f ();
      f->eax = file_write (thread_file_temp->file, buffer, size);
      release_lock_f ();
    } 
    else
    {
      f->eax = 0;
    }
  }
}
/*     Mục đích: Hàm syscall này thay đổi byte tiếp theo để đọc hoặc ghi trong một tệp đã mở.
       Thực hiện:
        Nó lấy số định danh tệp và vị trí mới từ ngăn xếp người dùng.
        Gọi file_seek để đặt vị trí byte tiếp theo trong tệp.*/
void 
sys_seek(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 5);
  *user_ptr++;
  struct thread_file *file_temp = find_file_id (*user_ptr);
  if (file_temp)
  {
    acquire_lock_f ();
    file_seek (file_temp->file, *(user_ptr+1));
    release_lock_f ();
  }
}

/*     Mục đích: Hàm syscall này trả về vị trí hiện tại của byte tiếp theo để đọc hoặc ghi trong một tệp đã mở.
       Thực hiện:
        Nó lấy số định danh tệp từ ngăn xếp người dùng.
        Gọi file_tell để lấy vị trí hiện tại trong tệp. Nếu không tìm thấy tệp, nó đặt giá trị trả về là -1.*/
void 
sys_tell (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file *thread_file_temp = find_file_id (*user_ptr);
  if (thread_file_temp)
  {
    acquire_lock_f ();
    f->eax = file_tell (thread_file_temp->file);
    release_lock_f ();
  }else{
    f->eax = -1;
  }
}

/* Do system close, by calling the function file_close() in filesystem */
void 
sys_close (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file * opened_file = find_file_id (*user_ptr);
  if (opened_file)
  {
    acquire_lock_f ();
    file_close (opened_file->file);
    release_lock_f ();
    /* Remove the opened file from the list */
    list_remove (&opened_file->file_elem);
    /* Free opened files */
    free (opened_file);
  }
}
/* Do system filesize, by calling the function file_length() in filesystem */
void 
sys_filesize (struct intr_frame* f){
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file * thread_file_temp = find_file_id (*user_ptr);
  if (thread_file_temp)
  {
    acquire_lock_f ();
    f->eax = file_length (thread_file_temp->file);
    release_lock_f ();
  } 
  else
  {
    f->eax = -1;
  }
}

/* Check is the user pointer is valid */
bool 
is_valid_pointer (void* esp,uint8_t argc){
  for (uint8_t i = 0; i < argc; ++i)
  {
    if((!is_user_vaddr (esp)) || 
      (pagedir_get_page (thread_current()->pagedir, esp)==NULL)){
      return false;
    }
  }
  return true;
}


/* Do system read, by calling the function file_tell() in filesystem */
void 
sys_read (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  /* PASS the test bad read */
  *user_ptr++;
  /* We don't konw how to fix the bug, just check the pointer */
  int fd = *user_ptr;
  int i;
  uint8_t * buffer = (uint8_t*)*(user_ptr+1);
  off_t size = *(user_ptr+2);
  if (!is_valid_pointer (buffer, 1) || !is_valid_pointer (buffer + size,1)){
    exit_special ();
  }
  /* get the files buffer */
  if (fd == 0) 
  {
    for (i = 0; i < size; i++)
      buffer[i] = input_getc();
    f->eax = size;
  }
  else
  {
    struct thread_file * thread_file_temp = find_file_id (*user_ptr);
    if (thread_file_temp)
    {
      acquire_lock_f ();
      f->eax = file_read (thread_file_temp->file, buffer, size);
      release_lock_f ();
    } 
    else
    {
      f->eax = -1;
    }
  }
}

/* Handle the special situation for thread */
void 
exit_special (void)
{
  thread_current()->st_exit = -1;
  thread_exit ();
}

/* Find file by the file's ID */
struct thread_file * 
find_file_id (int file_id)
{
  struct list_elem *e;
  struct thread_file * thread_file_temp = NULL;
  struct list *files = &thread_current ()->files;
  for (e = list_begin (files); e != list_end (files); e = list_next (e)){
    thread_file_temp = list_entry (e, struct thread_file, file_elem);
    if (file_id == thread_file_temp->fd)
      return thread_file_temp;
  }
  return false;
}

/* Smplify the code to maintain the code more efficiently */
static void
syscall_handler (struct intr_frame *f UNUSED)
{
  /* For Task2 practice, just add 1 to its first argument, and print its result */
  int * p = f->esp;
  check_ptr2 (p + 1);
  int type = * (int *)f->esp;
  if(type <= 0 || type >= max_syscall){
    exit_special ();
  }
  syscalls[type](f);
}
