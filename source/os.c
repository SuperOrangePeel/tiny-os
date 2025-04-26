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


uint32_t pg_dir[1024] __attribute__((aligned(4096))) = {
    [0] = (uint32_t)0 | PDE_PRESENT | PDE_PS | PDE_RW | PDE_USER,
};
uint32_t pg_table[1024] __attribute__((aligned(4096))) = {PDE_USER};

uint8_t map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x36};

// struct gdt_entry
// {
//     uint16_t limit_low; // 段界限低16位
//     uint16_t base_low;  // 基址低16位
//     uint8_t base_middle; // 基址中8位
//     uint8_t access;     // 段描述符属性
//     uint8_t granularity; // 段界限高4位和其他属性
//     uint8_t base_high;   // 基址高8位
// } __attribute__((packed)); // 按照字节对齐

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
    }
};

// 封装outb指令 outb是一个x86架构的I/O端口输出指令，用于向I/O端口写入一个字节的数据。
// outb 
void outb(uint8_t data, uint16_t port) {
    /*
    0f b7 55 f8          movzwl -0x8(%ebp),%edx
    0f b6 45 fc          movzbl -0x4(%ebp),%eax
    ee                   out    %al,(%dx)
    */
    __asm__ __volatile__("outb %[v], %[p]"::[p]"d"(port), [v]"a"(data));
}

void os_init() {
    outb(0x11, 0x20); // 表示开始初始化8259主片
    outb(0x11, 0xA0); // 表示开始初始化8259从片
    outb(0x20, 0x21); // 告诉8259主片，8259主片的中断号从0x20开始
    outb(0x28, 0xA1); // 告诉8259从片，8259从片的中断号从0x28开始
    outb(1 << 2, 0x21); //告诉8259主片，2号管脚(0 1 2)连接了一个从片
    outb(2, 0xa1); // 告诉8259从片，2号管脚(0 1 2)连接了一个主片
    outb(0x1, 0x21);// 告诉8259A主片，要给i8086发送中断
    outb(0x1, 0xA1); // 告诉8259A从片，要给i8086发送中断
    outb(0xfe, 0x21); //只留下时钟中断
    outb(0xff, 0xA1); //关闭从片所有中断

    pg_dir[MAP_ADDR >> 22] = (uint32_t)pg_table | PDE_PRESENT | PDE_RW | PDE_USER;
    pg_table[(MAP_ADDR >> 12) & 0x3FF] = (uint32_t)map_phy_buffer | PDE_PRESENT | PDE_RW | PDE_USER;
}