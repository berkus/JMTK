#ifndef HAL_H
#define HAL_H

#include "types.h"

/* Call to send the system into a panic. */
void panic(const char *message);

/*******************************************************************************
 * Initialisation / finalisation function registration
 ******************************************************************************/

/* Structure defining a function to be run either on startup or shutdown. */
typedef struct init_fini_fn {
  const char *name;           /* A unique identifier for this function.*/
  const char **prerequisites; /* Either NULL or a NULL-terminated list of
                                 function IDs that must have already run. */
  int (*fn)(void);            /* The function to run. Returns zero on success. */
  int padding; /* Pad so the structure is 4 * sizeof(int) large. */
} init_fini_fn_t;

/* Mark an instance of an init_fini_fn_t object as 'run_on_startup' to
   have it run on startup, e.g.:
  
     static init_fini_fn_t x run_on_startup = {"foo", NULL, &foo}; */
#define run_on_startup __attribute__((__section__(".startup"),used))
/* Mark an instance of an init_fini_fn_t object as 'run_on_shutdown' to
   have it run on shutdown, e.g.:
 
     static init_fini_fn_t x run_on_shutdown = {"bar", NULL, &bar}; */
#define run_on_shutdown __attribute__((__section__(".shutdown"),used))

/*******************************************************************************
 * Console
 *******************************************************************************/

typedef struct console {
  /* Initialise a console object. */
  int (*open)(struct console *obj);
  /* Finish a console object. */
  int (*close)(struct console *obj);
  /* Read from a console - if no data is available, return zero without blocking.
     Else return as much data as is available, up to 'len'. Return the
     number of bytes read, or -1 on error. */
  int (*read)(struct console *obj, char *buf, int len);
  /* Write to a console - return the number of bytes written, or -1 on
     failure. */
  int (*write)(struct console *obj, const char *buf, int len);
  /* Flush the write buffer of a console. */
  void (*flush)(struct console *obj);

  /* Intrusive linked list, for HAL's use only. */
  struct console *prev, *next;
  /* Implementation dependent data. */
  void *data;
} console_t;

/* Register 'c' as a new console. Returns zero on success. */
int register_console(console_t *c);
/* Unregister 'c' from the set of consoles. */
void unregister_console(console_t *c);
/* Write to all consoles. */
void write_console(const char *buf, int len);
/* Read from the default console into a buffer. If there is no data 
   available, this call will block. Returns the number of bytes actually
   read, or -1 on error. */
int read_console(char *buf, int len);

/*******************************************************************************
 * Interrupt handling
 ******************************************************************************/

/* Callback type for an interrupt handler. Takes a pointer to the target
   specific struct 'regs' containing the register contents when the interrupt
   was taken. Return nonzero if any changes you made to 'r' should be 
   reflected when the handler returns. */
struct regs;
typedef int (*interrupt_handler_t)(struct regs *r, void *p);

/* Register a new interrupt handler on the given target specific interrupt
   identifer 'num'. There can be multiple handlers per interrupt identifier.
   Returns -1 on failure. */
int register_interrupt_handler(int num, interrupt_handler_t handler, void *p);
/* Unregisters the given interrupt handler from the given intterupt identifer.
   Returns -1 on failure. */
int unregister_interrupt_handler(int num, interrupt_handler_t handler, void *p);

/*******************************************************************************
 * Debugging
 ******************************************************************************/

/* Cause a debug or breakpoint trap. */
void trap();

/* Invoke the debugger, from a debug/breakpoint trap interrupt handler. */
void debugger_trap(struct regs *regs);
/* Invoke the debugger, from an interrupt handler that is reporting abnormal 
   behaviour (i.e. not a trap or breakpoint exception).

   Description holds an implementation-defined string describing the error. */
void debugger_except(struct regs *regs, const char *description);

/* Perform a backtrace.
  
   If called with *data=0, return the IP location the current function was called
   from, and save implementation specific information in 'data'.

   Iteratively call until the return value is 0, at which point the backtrace
   is complete. */
uintptr_t backtrace(uintptr_t *data);

/* Set an instruction breakpoint. Returns -2 if this is not possible on the
   target, or -1 if it is supported but an error occurred.

   Otherwise the return value is an implementation defined number that
   can be passed to unset_insn_breakpoint. */
int set_insn_breakpoint(uintptr_t loc);
/* Removes an instruction breakpoint previously set with set_insn_breakpoint.
   Returns -2 if not supported, -1 on error or 0 on success. */
int unset_insn_breakpoint(int id);

/* Set a data read breakpoint. Returns -2 if this is not possible on the
   target, or -1 if it is supported but an error occurred.

   Otherwise the return value is an implementation defined number that
   can be passed to unset_read_breakpoint. */
int set_read_breakpoint(uintptr_t loc);
/* Removes a data read breakpoint previously set with set_read_breakpoint.
   Returns -2 if not supported, -1 on error or 0 on success. */
int unset_read_breakpoint(int id);

/* Set a data write breakpoint. Returns -2 if this is not possible on the
   target, or -1 if it is supported but an error occurred.

   Otherwise the return value is an implementation defined number that
   can be passed to unset_write_breakpoint. */
int set_write_breakpoint(uintptr_t loc);
/* Removes a data write breakpoint previously set with set_write_breakpoint.
   Returns -2 if not supported, -1 on error or 0 on success. */
int unset_write_breakpoint(int id);

/* Returns a string giving the name of the symbol the address 'addr' is in,
   and stores the offset from that symbol in '*offs'.

   Returns NULL if not supported or a symbol is unavailable. */
const char *lookup_kernel_symbol(uintptr_t addr, int *offs);

/* Given an implementation defined structure 'regs' (as given to an interrupt
   handler), fill in the given array of name-value pairs.

   names and values will have enough space to hold up to 'max' entries. The
   function will store names of registers and their equivalent values into these
   arrays, and return the number of registers saved.

   Returns -1 on failure. */
int describe_regs(struct regs *regs, int max, const char **names,
                  uintptr_t **values);

/********************************************************************************
 * Peripherals
 *******************************************************************************/

uint64_t get_timestamp();
void set_timestamp(uint64_t ts);

/* Registers a callback to be fired in 'num_millis' milliseconds. If periodic
   is nonzero, the callback should be fired every 'num_millis' milliseconds,
   else it should only be called once.

   Returns -1 on error, otherwise an ID to be passed to unregister_callback. */
int register_callback(uint32_t num_millis, int periodic, void (*cb)(void*),
                      void *data);
/* Unregisters a callback registered with register_callback. */
int unregister_callback(void (*cb)(void*));

/*******************************************************************************
 * Memory management
 ******************************************************************************/

typedef struct address_space address_space_t;

#define PAGE_WRITE   /* Page is writable */
#define PAGE_EXECUTE /* Page is executable */
#define PAGE_USER    /* Page is useable by user mode code (else kernel only) */
#define PAGE_COW     /* Page is marked copy-on-write. It must be copied if
                        written to. */

/* Returns the (default) page size in bytes. Not all pages may be this size
   (large pages etc.) */
uintptr_t get_page_size();

/* Allocate a physical page of the size returned by get_page_size(), returning
   the address of the page in the physical address space. Returns ~0ULL on
   failure. */
uint64_t alloc_page();
/* Free a physical page allocated by alloc_page(). Returns -1 on failure. */
int free_page(uint64_t page);

/* Creates a new address space based on 'src' and stores it in 'dest'. */
int clone_address_space(address_space_t *dest, address_space_t *src);

/* Returns the current address space. */
address_space_t *get_current_address_space();

/* Maps 'num_pages' * get_page_size() bytes from 'p' in the physical address
   space to 'v' in the virtual address space 'a'.

   Returns zero on success or -1 on failure. */
int map(address_space_t *a, uintptr_t v, uint64_t p, int num_pages,
        unsigned flags);
/* Unmaps 'num_pages' * get_page_size() bytes from 'v' in the virtual address
   space 'a'. Returns zero on success or -1 on failure. */
int unmap(address_space_t *a, uintptr_t v, int num_pages);

/* If 'v' has a V->P mapping associated with it, return 'v'. Else return
   the next page (multiple of get_page_size()) which has a mapping associated
   with it. */
uintptr_t iterate_mappings(address_space_t *a, uintptr_t v);

/* If 'v' is mapped in 'a', return the physical page it is mapped to
   and fill 'flags' with the mapping flags. Else return ~0ULL. */
uint64_t get_mapping(address_space_t *a, uintptr_t v, unsigned *flags);

/* Return 1 if 'v' is mapped in 'a', else 0, or -1 if not implemented. */
int is_mapped(address_space_t *a, uintptr_t v);

/*******************************************************************************
 * Devices
 ******************************************************************************/

/* A character device - a streaming device. */
typedef struct char_device {
  /* Read from a character device - if no data is available, block until some is.
     Else return as much data as is available, up to 'len'. Return the
     number of bytes read, or -1 on error. */
  int (*read)(struct char_device *obj, char *buf, uint64_t len);
  /* Write - return the number of bytes written, or -1 on
     failure. */
  int (*write)(struct char_device *obj, char *buf, uint64_t len);
  /* Flush the write buffer, if applicable. */
  void (*flush)(struct char_device *obj);
  /* Register a callback which will fire when the device has data available. */
  int (*register_callback)(struct char_device *obj, void (*cb)(void*),
                           void *cb_param);
  /* Unregister a callback. Both the callback pointer and its associated 
     parameter must match a previously registered callback. */
  int (*unregister_callback)(struct char_device *obj, void (*cb)(void*),
                             void *cb_param);

  /* Implementation dependent */
  void *data;
} char_device_t;

/* A block device - a random access device. */
typedef struct block_device {
  /* Read up to 'len' bytes from 'offset' bytes on the device into 'buf'.
     Return -1 on failure, or the number of bytes read. */
  int (*read)(struct block_device *obj, uint64_t offset, void *buf, uint64_t len);
  /* Write - return the number of bytes written, or -1 on
     failure. */
  int (*write)(struct block_device *obj, uint64_t offset, void *buf, uint64_t len);
  /* Flush the write buffer, if applicable. */
  void (*flush)(struct block_device *obj);

  /* Implementation dependent */
  void *data;
} block_device_t;

/* Device ID type is a pure 32-bit number, made up of minor and major
   parts. */
typedef uint32_t dev_t;

static inline unsigned minor(dev_t x) {
  return x & 0xFFFF;
}
static inline unsigned major(dev_t x) {
  return (x >> 16) & 0xFFFF;
}
static inline dev_t makedev(unsigned major, unsigned minor) {
  return ((major&0xFFFF) << 16) | (minor & 0xFFFF);
}

/* Registers 'dev' as a character device with ID 'id'. Returns 0 on success
   or -1 on failure. */
int register_char_device(dev_t id, char_device_t *dev);
/* Registers 'dev' as a block device with ID 'id'. Returns 0 on success
   or -1 on failure. */
int register_block_device(dev_t id, block_device_t *dev);
/* Unregisters the device associated with ID 'id', and returns an opaque
   pointer to the char_device_t or block_device_t. */
void *unregister_device(dev_t id);
/* Returns a character device object for the given id. If 'id' is not a
   registered character device, it returns NULL. */
char_device_t *get_char_device(dev_t id);
/* Returns a blockacter device object for the given id. If 'id' is not a
   registered blockacter device, it returns NULL. */
block_device_t *get_block_device(dev_t id);

/*******************************************************************************
 * Virtual filesystem
 ******************************************************************************/

struct inode;

/* A filesystem driver. */
typedef struct filesystem {
  /* Reads from node 'node_data' 'sz' bytes from the given offset.
     Requires 'node_data' to be a normal file. Returns the number of
     bytes written or -errno on error. */
  int64_t (*read)(struct filesystem *fs,
                  void *node_data, uint64_t offset, void *buf, uint64_t sz);
  /* Writes to node 'node_data', 'sz' bytes from the given offset.
     Requires 'node_data' to be a normal file. Returns the number of
     bytes written or -errno on error. */
  int64_t (*write)(struct filesystem *fs,
                   void *node_data, uint64_t offset, void *buf, uint64_t sz);
  /* Returns the number of directory entries 'node_data' has. Requires
     'node_data' to be a directory. */
  unsigned (*num_dir_entries)(struct filesystem *fs,
                              void *node_data);
  /* Returns the name of the n'th entry in the directory 'node_data'.
     n must be less than the return value of num_dir_entries(). */
  const char * (*read_dir_entry_name)(struct filesystem *fs,
                                      void *node_data, unsigned n);
  /* Given a directory node and a blank inode structure, fill in the inode
     structure with data from the n'th directory entry. Return 0 on success,
     -errno on failure. */
  int (*fill_dir_entry)(struct filesystem *fs,
                        void *node_data, unsigned n,
                        struct inode *inode);
  /* Create a new node as a child of the directory 'node_data'. Requires
     'node_data' to be a directory. The fields 'name', 'type', 'mode',
     'uid' and 'gid' are taken from 'inode', and node specific data is
     written to it, along with 'ctime', 'mtime' and 'atime' which are
     all set to the current timestamp, and 'nlink' which is set to 1.
     'size' is set to 0. */
  int (*mknod)(struct filesystem *fs,
               void *node_data, struct inode *inode);
} filesystem_t;

/* Registers a filesystem with name "ident", and a probe function that
   will attempt to find a filesystem of this kind on the given device.

   If it succeeds, it should fill in the 'fs' structure and return zero.
   Else, it should return nonzero.

   This function returns zero if the FS was successfully registered,
   nonzero otherwise. */
int register_filesystem(const char *ident,
                        int (*probe)(dev_t dev, filesystem_t *fs));
/* Unregisters a filesystem previously registered by register_filesystem.
   Returns zero on success, nonzero on failure. */
int unregister_filesystem(const char *ident);

// JM: Move this to vfs.h
#if 0

/* A mount point. This has a device that has been mounted, a location,
   and a filesystem to handle requests. */
typedef struct mountpoint {
  dev_t         dev;
  struct inode *node;
  filesystem_t *fs;
} mountpoint_t;

/* The type a VFS node can be. Normal, character/block device, pipe (FIFO) or
   socket. */
typedef enum inode_type {
  it_file, it_dir, it_chardev, it_blockdev, it_fifo, it_socket
} inode_type_t;

/* A VFS node, commonly known as an "inode". */
typedef struct inode {
  const char   *name;
  mountpoint_t *mountpoint;
  inode_type_t  type;
  struct inode *parent;

  /* Standard UNIX stat state. */
  int mode, nlink, uid, gid, size;
  uint64_t atime, mtime, ctime;

  /* Write buffer */
  void *write_buffer;

  /* How many times this inode has been opened. */
  unsigned handles;

  union {
    /* If this is a directory, this will hold a cache of the currently known
       directory entries. */
    struct directory_cache *dir_cache;
    /* If this is a special device, this will hold the device ID. */
    dev_t dev;
  };

  /* Implementation dependent data passed to the filesystem. */
  void         *data;
} inode_t;

/* Mounts a filesystem on the directory 'node'. If 'fs' is NULL, all known
   filesystems are probed. If not, only the filesystem specified is probed.
   
   Returns zero on success, -errno on failure. */
int mount(dev_t dev, inode_t node, const char *fs);
/* Unmounts. If the device is given (isn't 0), the mountpoint
   associated with it is unmounted. If not, the inode is expected to be
   valid and is unmounted. */
int umount(dev_t dev, inode_t inode);

/* Returns an inode_t for the given path and increments its open count. */
inode_t open(const char *path);
/* Performs a read of sz bytes into buf at offset. */
int64_t read(uint64_t offset, void *buf, uint64_t sz);
/* Performs a write of sz bytes from buf at offset. */
int64_t write(uint64_t offset, void *buf, uint64_t sz);
/* Decrements the open count of inode. */
void close(inode_t inode);

#endif // JM: Move this to vfs.h

/********************************************************************************
 * Threading
 *******************************************************************************/

struct thread_target_state;

int thread_setjmp(struct thread_target_state *buf);
void thread_longjmp(struct thread_target_state *buf, int val);

#endif // HAL_H
