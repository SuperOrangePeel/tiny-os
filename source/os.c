/**
 * 功能：32位代码，完成多任务的运行
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
#include "os.h"


typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

void do_syscall(int func/*功能号*/, char* str, char color) {
    static int row = 0;
    if (func == 2) {
        unsigned short* dest = (unsigned short *)0xb8000 + 80 * row; // 25行 80列
        while (*str) {
            *dest ++ = *str++ | (color << 8);
        }
        row = ( ++ row) % 26;

        for (int i = 0; i < 0xFFFFF; i ++ );
    }
}


void sys_show(char* str, char color) {
    uint32_t addr[] = {0, SYSCALL_SEG}; // gdt表中有偏移量 所以selector中就填0就行了

    __asm__ __volatile__("push %[color];push %[str];push %[id];lcalll *(%[a])"::
        [a]"r"(addr), [color]"m"(color), [str]"m"(str), [id]"r"(2)); // 跳转
}
void task_0 (void) {
    uint8_t color = 0;
    char* str = "task a: 1234";
    for(;;) {
        sys_show(str, color ++);
    }
}

void task_1 (void) {
    uint8_t color = 0xff;
    char* str = "task b: 5678";
    for(;;) {
        sys_show(str, color --);
    }
}




#define PDE_PRESENT     (1<<0)
#define PDE_RW          (1<<1)
#define PDE_USER        (1<<2)
#define PDE_PS          (1<<7) // Page Size if set, the page dictory entry is a 4MB page instead of a page table

#define MAP_ADDR 0x80000000

uint32_t pg_dir[1024] __attribute__((aligned(4096))) = {
    [0] = (uint32_t)0 | PDE_PRESENT | PDE_PS | PDE_RW | PDE_USER, // 映射0x00000000到0x00000000, 大小为4MB
};
// 从0x80000000地址开始后面的4MB地址，为该pg_table映射的virtual地址空间，真实地址空间是map_phy_buffer
uint32_t pg_table[1024] __attribute__((aligned(4096))) = {PDE_USER};

// MAP_ADDR is the address we want to map, which is 0x80000000
uint8_t map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x36};

uint32_t task0_dpl3_stack[1024];
uint32_t task1_dpl3_stack[1024];
uint32_t task0_dpl0_stack[1024]; // 当运行在特权级3的任务，遇到中断时，就需要特权级别为0的栈，让中断代码使用。这样可以隔离不同特权级别的数据
uint32_t task1_dpl0_stack[1024];

uint32_t task0_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task0_dpl0_stack + 4*1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,  (uint32_t)task_0/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task0_dpl3_stack + 4*1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    APP_DATA_SEG, APP_CODE_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, (uint32_t)0x0, 0x0,
    //TASK_DATA_SEG, TASK_CODE_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK0_LDT_SEL, 0x0,
};

uint32_t task1_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task1_dpl0_stack + 4*1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,  (uint32_t)task_1/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task1_dpl3_stack + 4*1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    APP_DATA_SEG, APP_CODE_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, 0x0, 0x0,
    // TASK_DATA_SEG, TASK_CODE_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK_DATA_SEG, TASK1_LDT_SEL, 0x0,
};

// every IDT entry is 8 bytes
struct {uint16_t offset_l, selector, attr, offset_h;}idt_table[256] __attribute__((aligned(8)));

// struct gdt_entry
// {
//     uint16_t limit_low; // 段界限低16位
//     uint16_t base_low;  // 基址低16位
//     uint8_t base_middle; // 基址中8位
//     uint8_t access;     // 段描述符属性
//     uint8_t granularity; // 段界限高4位和其他属性
//     uint8_t base_high;   // 基址高8位
// } __attribute__((packed)); // 按照字节对齐

// every GDT entry is 8 bytes
struct {
    uint16_t limit_l, base_l, basehl_attr, base_limit;
} gdt_table[256] __attribute__((aligned(8))) = {
    [KERNEL_CODE_SEG / 8] = {
        .limit_l = 0xFFFF, // limit全部为1，代表最大值为4GB
        .base_l = 0x0000, // 代码段基址
        .basehl_attr = 0x9A00, // 可执行，读权限
        .base_limit = 0x00CF
    },
    [KERNEL_DATA_SEG / 8] = {
        .limit_l = 0xFFFF,
        .base_l = 0x0000,
        .basehl_attr = 0x9200, // 可读写
        .base_limit = 0x00CF
    },
    [APP_CODE_SEG / 8] = {
        .limit_l = 0xFFFF, // limit全部为1，代表最大值为4GB
        .base_l = 0x0000, // 代码段基址
        .basehl_attr = 0xFA00, // 可执行，读权限
        .base_limit = 0x00CF
    },
    [APP_DATA_SEG / 8] = {
        .limit_l = 0xFFFF,
        .base_l = 0x0000,
        .basehl_attr = 0xF300, // 可读写
        .base_limit = 0x00CF
    },
    [TASK0_TSS_SEG / 8] = {0x68, 0x0, 0xe900, 0x0},
    [TASK1_TSS_SEG / 8] = {0x68, 0x0, 0xe900, 0x0},
    [SYSCALL_SEG / 8] = {0x0000, KERNEL_CODE_SEG, 0xec03, 0},
};

// 封装outb指令 outb是一个x86架构的I/O端口输出指令，用于向I/O端口写入一个字节的数据。
// outb 
void outb(uint8_t data, uint16_t port) {
    /*
    outb --> out byte  还有 outw, outl
    0f b7 55 f8          movzwl -0x8(%ebp),%edx
    0f b6 45 fc          movzbl -0x4(%ebp),%eax
    ee                   out    %al,(%dx)
    */
    __asm__ __volatile__("outb %[v], %[p]"::[p]"d"(port), [v]"a"(data));
}
void task_sched(void) {
    static int task_tss = TASK0_TSS_SEG;
    task_tss = (task_tss == TASK0_TSS_SEG) ? TASK1_TSS_SEG : TASK0_TSS_SEG;

    uint32_t addr[] = {0, task_tss};
    __asm__ __volatile__("ljmpl *(%[a])"::[a]"r"(addr)); // 跳转
}
void timer_int(void);
void syscall_handler(void);

void os_init(void) {
    outb(0x11, 0x20); // 表示开始初始化8259主片
    outb(0x11, 0xA0); // 表示开始初始化8259从片
    outb(0x20, 0x21); // 告诉8259主片，8259主片的中断号从0x20开始（IDT表
    outb(0x28, 0xA1); // 告诉8259从片，8259从片的中断号从0x28开始（IDT表
    outb(1 << 2, 0x21); //告诉8259主片，2号管脚(0 1 2)连接了一个从片
    outb(2, 0xa1); // 告诉8259从片，2号管脚(0 1 2)连接了一个主片
    outb(0x1, 0x21);// 告诉8259A主片，要给i8086发送中断
    outb(0x1, 0xA1); // 告诉8259A从片，要给i8086发送中断
    outb(0xfe, 0x21); //只留下时钟中断
    outb(0xff, 0xA1); //关闭从片所有中断

    /*
    1193180 是 8253 的输入时钟频率（1.193180 MHz）
    除以 100 表示想要获得 100Hz 的中断频率（即每秒触发100次中断）
    即告诉8253芯片，1193180/100次个周期 发送一次中断信号。
    */
    int tmo = 1193180 / 100; 
    outb(0x36, 0x43); // 设置8253的工作模式, 第0个定时器工作在模式3，周期性产中断
    outb((uint8_t)tmo, 0x40);
    outb(tmo >> 8, 0x40);

    idt_table[0x20].offset_l = (uint32_t)timer_int & 0xFFFF;
    idt_table[0x20].offset_h = (uint32_t)timer_int >> 16;
    idt_table[0x20].selector = KERNEL_CODE_SEG; // 代码段选择子
    idt_table[0x20].attr = 0x8E00; // 中断门，特权级0(最高），段存在

    gdt_table[TASK0_TSS_SEG / 8].base_l = (uint16_t)(uint32_t)task0_tss;
    gdt_table[TASK1_TSS_SEG / 8].base_l = (uint16_t)(uint32_t)task1_tss;
    gdt_table[SYSCALL_SEG / 8].limit_l = (uint16_t)(uint32_t)syscall_handler;

    // 设置映射0x80000000地址到map_phy_buffer所标记的物理内存 
    pg_dir[MAP_ADDR >> 22] = (uint32_t)pg_table | PDE_PRESENT | PDE_RW | PDE_USER;
    pg_table[(MAP_ADDR >> 12) & 0x3FF] = (uint32_t)map_phy_buffer | PDE_PRESENT | PDE_RW | PDE_USER;
}