#ifndef HOSTED_MMAP_H
#define HOSTED_MMAP_H

#define MMAP_KERNEL_START 0xC0000000

#define MMAP_KERNEL_VMSPACE_START \
                          0xD0000000
#define MMAP_KERNEL_VMSPACE_END \
                          0xFEFF0000

#define MMAP_PMM_STACK2   0xFF000000
#define MMAP_PMM_STACK1   0xFF400000
#define MMAP_PMM_STACK0   0xFF800000
#define MMAP_PMM_STACKEND 0xFFBFF000

#define MMAP_PHYS_BASE (0x200000000UL)
#define MMAP_PHYS_END  (0x201000000UL) /* 16MB */

#endif
