#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <stdint.h> // 引入 uint8_t 这种标准类型

typedef struct {
    // === 内存 ===
    // 对应书第 1.5 章
    // CHIP-8 只有 4KB 内存 (4096 字节)
    uint8_t memory[4096]; 

    // === 寄存器 ===
    // 对应书第 1.6 章
    // 16 个 8位通用寄存器，名字叫 V0 到 VF
    uint8_t V[16]; 
    
    // 索引寄存器 (Index Register)，用来存内存地址的
    uint16_t I; 
    
    // 程序计数器 (Program Counter)
    // 记录当前运行到哪一行代码了
    uint16_t pc; 

    // === 显存 ===
    // 对应书第 6.3 章 "直接操作 Video RAM"
    // 64x32 个像素，每个像素要么亮(1)要么灭(0)
    uint8_t gfx[64 * 32]; 

    // === 倒计时器 ===
    // 对应书第 4 章 "定时器"
    // 只要这两个数大于 0，每秒钟就会自动减 60 次
    uint8_t delay_timer;
    uint8_t sound_timer; 

    // === 堆栈 ===
    // 对应书第 5.1.2 章 "堆栈"
    // 用来记住 "我是从哪行代码跳过来的"，以便跳回去
    uint16_t stack[16];
    uint16_t sp; // 栈指针 (Stack Pointer)

    // === 键盘 ===
    // 对应书第 6.5 章 "键盘输入"
    // 记录 16 个按键现在的状态（按下了没）
    uint8_t key[16]; 

} Chip8;

// 2. 初始化函数 (给 CPU 通电复位)
void init_cpu(Chip8 *cpu) {
    // PC 起始位置设为 0x200 (512)，因为前 512 字节是留空的
    cpu->pc = 0x200;
    cpu->I = 0;
    cpu->sp = 0;
    
    // 清空内存、寄存器、显存 (全部填 0)
    // memset 是 C 语言最快的清零方法：(目标地址, 填什么数, 填多长)
    memset(cpu->memory, 0, sizeof(cpu->memory));
    memset(cpu->V, 0, sizeof(cpu->V));
    memset(cpu->gfx, 0, sizeof(cpu->gfx));
    memset(cpu->stack, 0, sizeof(cpu->stack));
    memset(cpu->key, 0, sizeof(cpu->key));
}
// === 新增：加载 ROM 函数 ===
bool load_rom(Chip8 *cpu, const char *filename) {
    printf("Loading: %s\n", filename);

    // 1. 打开文件 (rb = read binary)
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        printf("Error: Failed to open file\n");
        return false;
    }

    // 2. 获取文件大小
    fseek(f, 0, SEEK_END); // 光标移到末尾
    long size = ftell(f);  // 告诉我当前位置 (即文件大小)
    rewind(f);             // 光标回到开头

    printf("File size: %ld bytes\n", size);

    // 3. 检查文件是否太大 (内存只有 4096，前 512 被占用了，所以剩 3584)
    if (size > (4096 - 512)) {
        printf("Error: ROM is too big!\n");
        fclose(f); // 别忘了关文件
        return false;
    }

    // 4. 读取文件内容到内存
    // fread(目标地址, 每个块多大, 读几块, 文件指针)
    // 目标地址是 &cpu->memory[0x200]，也就是从第 512 个格子开始填
    fread(&cpu->memory[0x200], 1, size, f);

    // 5. 收尾
    fclose(f);
    return true;
}

// === 修改：Main 函数 ===
int main(int argc, char *argv[]) {
    // 检查用户有没有输入文件名
    // argc 是参数个数，argv 是参数列表
    // ./chip8 "pong.ch8" -> argc=2, argv[0]="./chip8", argv[1]="pong.ch8"
    if (argc < 2) {
        printf("Usage: %s <rom_file>\n", argv[0]);
        return 1;
    }

    Chip8 cpu;
    init_cpu(&cpu);

    // 加载 ROM
    // 如果加载失败，就退出程序
    if (!load_rom(&cpu, argv[1])) {
        return 1;
    }
    
    printf("ROM loaded successfully.\n");
    
    // 我们暂时还没有循环，所以这里程序就结束了
    // 下一步我们会在这里加 while 循环
    return 0;
}
