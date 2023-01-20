//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

#define BIT 255 /*T_CS file system for 20180775*/ 

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      /*T_CS file system for 20180775*/
      if (r == -2) // error for allocate data block length overflow
      {
        cprintf("ERROR : Allocate overflow\n");
        return -2;
      }

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

/*T_CS file system for 20180775*/
/* for use cs file information print */
void cs_printinfo(struct file *n, char *fname)
{
  /* INODE dircet block = 4B = 3B(number) + 1B(length)*/
  uint info_addr;
  uint backward_area = BIT; // operation bit for 1B (length area)

  cprintf("FILE NAME: %s\n", fname); // print file name file system info
  cprintf("INODE NUM: %d\n", n->ip->inum); // print inode file system info

  switch (n->ip->type) // print what type case 1 : dir | case 2 : file | case 3 : dev | case 4 : t_cs | default : no type 
  {
  case 1:
    cprintf("FILE TYPE: DIR\n");
    break;
  case 2:
    cprintf("FILE TYPE: FILE\n");
    break;
  case 3:
    cprintf("FILE TYPE: DEV\n");
    break;
  case 4:
    cprintf("FILE TYPE: CS\n");
    break;
  default:
    cprintf("FILE TYPE: NO TYPE\n");
    break;
  }
  cprintf("FILE SIZE: %d Bytes\n", n->ip->size); // print for file size for file system info 
  cprintf("DIRECT BLOCK INFO: \n");

  if (n->ip->type == 2) // when type -> FILE 
  {
    for (int i = 0; i < NDIRECT; i++)
    {
      // cprintf("%d",info_addr);
      if ((info_addr = n->ip->addrs[i]) != 0)
      {
        cprintf("[%d] %d\n", i, info_addr); // for print direct block 
      }
    }
  }
  else if (n->ip->type == 4) // when type -> T_CS  
  {
    // cprintf("%d",info_addr);
    for (int i = 0; i < NDIRECT; i++)
    {
      if ((info_addr = n->ip->addrs[i]) != 0)
      {
        uint num = info_addr >> 8; // number for move the bit 8 
        uint length = info_addr & backward_area; // info_addr&(1<<8) - 1 = length  
        cprintf("[%d] %d (num : %d, length: %d)\n", i, info_addr, num, length);
      }
    }
  }
  cprintf("\n");
  cprintf("\n");
}
