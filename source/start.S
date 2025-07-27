/**
 * 功能：16位与32位的启动混合代码
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
	#include "os.h"

	// 声明本地以下符号是全局的，在其它源文件中可以访问
	.global _start, timer_int, syscall_handler

	// 指定以下的代码生成16位的机器指令，这样才能在启动时的实模式下运行
  	.code16

	// 以下是代码区
 	.text
_start: 
	mov $0, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

read_self_all:
	mov $0x02, %ah // 读磁盘
	mov $0x40, %al // 64个扇区
	mov $_start_32, %bx // 读到内存的0x7e00处 
	mov $0x2, %cl
	mov $0x80, %dl 
	int $0x13
	jc read_self_all // jmp if carry flag is set, retry reading

	cli   // 关中断
	lgdt [gdt_desc] // Load Global Descriptor Table
	lidt [idt_desc] // Load Interrupt Descriptor Table
	mov $0x1, %ax
	lmsw %ax // Load Machine Status Word 将PE位置1
	jmp $KERNEL_CODE_SEG, $_start_32 // 加载新的代码段选择子到 CS 寄存器，进入保护模式，跳转到32位代码段
	


	.org 0x1fe  //跳转到510处  汇编器会自动用0填充之前的空间
	.byte 0x55, 0xaa
	// .word 0xaa55      //x86为小端序，0x55为低位，存在低内存地址，也就是0x1fe处


	.code32
	.text
//因为伪指令.byte会被编译器编译到0x7dfe和0x7dff处的指令
//故 _start_32_ 指向 0x7e00处
_start_32:
	// 此时进入保护模式了
	mov $KERNEL_CODE_SEG, %ax // 选择内核代码段
	mov %ax, %ds // 选择数据段
	mov %ax, %es // 选择额外段
	mov %ax, %fs // 选择fs段
	mov %ax, %gs // 选择gs段
	mov $_start, %esp // 设置栈指针, _start在0x7c00处，前面的空间用作栈

	call os_init
	
	mov $pg_dir, %eax // 设置页目录表的基地址
	mov %eax, %cr3 // 设置页目录表基地址寄存器

	mov %cr4, %eax // 设置控制寄存器4
	orl $(1 << 4), %eax // 设置页大小位 orl means OR with long
	mov %eax, %cr4 // 设置控制寄存器4

	mov %cr0, %eax // 读取控制寄存器0
	orl $(1 << 31), %eax // 设置PG位
	mov %eax, %cr0 // 设置控制寄存器0

	// sti // 开启中断

	mov $TASK0_TSS_SEG, %ax
	ltr %ax // Load task register


	push $APP_DATA_SEG
	push $task0_dpl3_stack + 1024 * 4			// ESP, 栈从高地址向低地址压栈
	push $0x202  // 开启中断
	push $APP_CODE_SEG
	push $task_0_entry
	iret

	// jmp .


timer_int:
	// 因为中断处理过程中，会使用到寄存器，所以中断处理之前要保存当前运行进程的寄存器
	push %ds // 保存数据段寄存器
	pusha // 保存寄存器状态

	mov $0x20, %al 
	outb %al, $0x20 // 发送结束信号到定时器

	mov $KERNEL_DATA_SEG, %ax
	mov %ax, %ds
	call task_sched

	popa // 恢复寄存器状态
	pop %ds // 恢复数据段寄存器
	iret // 返回中断

syscall_handler:
	// 因为system call处理过程中，会使用到寄存器，所以system call之前要保存当前运行进程的寄存器
	push %ds // 保存数据段寄存器
	pusha // 保存寄存器状态

	mov $KERNEL_DATA_SEG, %ax
	mov %ax, %ds

	mov %esp, %ebp
	push 13*4(%ebp)
	push 12*4(%ebp)
	push 11*4(%ebp)


	call do_syscall

	add $(3*4), %esp // 将上面的三个压栈 弹出！！！！！

	popa 
	pop %ds
	retf $(3 * 4)// return far 手册要求，要额外弹 3 个系统调用传参 每个4字节

task_0_entry: // 任务0的入口函数, 运行在privilege level 3 
	mov %ss, %ax // ss寄存器中是$APP_DATA_SEG
	mov %ax, %ds // 选择数据段
	mov %ax, %es // 选择额外段
	mov %ax, %fs // 选择fs段
	mov %ax, %gs // 选择gs段
	jmp task_0

// 手册要求的规范写法	
gdt_desc:
	.word (256*8) - 1 // gdt表的大小 .word表示16位的空间 (256*8) - 1表示在空间中存储的值
	.long gdt_table // gdt表的地址

idt_desc:
	.word (256*8) - 1
	.long idt_table // IDT表的地址 