#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // Required for usleep
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

// === 新增：CPU 周期函数 ===
void emulate_cycle(Chip8 *cpu) {
    // 1. 取指 (Fetch)
    // 从内存 pc 处拿两个字节，拼成一个 16 位的 opcode
    uint8_t byte1 = cpu->memory[cpu->pc];
    uint8_t byte2 = cpu->memory[cpu->pc + 1];
    
    // 这里的 | 是位运算 "OR" (拼接)
    uint16_t opcode = (byte1 << 8) | byte2;

    // 2. 译码与执行 (Decode & Execute)
    // 我们用 & 0xF000 取出最左边的 4 位 (指令类别)
    switch (opcode & 0xF000) {
        
        case 0x0000:
            // 0x00E0: 清屏指令 (Clear Screen)
            if ((opcode & 0x00FF) == 0x00E0) {
                printf("指令执行: 清除屏幕 (00E0)\n");
                // TODO: 后面再写真正的清屏代码
                memset(cpu->gfx, 0, sizeof(cpu->gfx));
                cpu->pc += 2; // 别忘了走两步
            } 
            else {
                printf("未知指令: 0x%X\n", opcode);
                cpu->pc += 2;
            }
            break;

        case 0x1000:
            // 0x1NNN: 跳转 (Jump) 到地址 NNN
            // 比如 1200 就是跳到 0x200
            printf("指令执行: 跳转到 0x%X\n", opcode & 0x0FFF);
            cpu->pc = opcode & 0x0FFF; 
            // 注意：跳转指令直接修改了 pc，所以不需要 cpu->pc += 2
            break;

        case 0x6000:
            // 0x6XNN: 设置寄存器 VX = NN
            // 比如 61AA -> 把寄存器 V1 设为 0xAA (170)
            {
                uint8_t x = (opcode & 0x0F00) >> 8; // 取出 X (第2位)
                uint8_t nn = (opcode & 0x00FF);     // 取出 NN (最后2位)
                cpu->V[x] = nn;
                printf("指令执行: 设置 V[%d] = 0x%X\n", x, nn);
                cpu->pc += 2;
            }
            break;

        case 0x7000:
            // 0x7XNN: 寄存器加值 VX += NN
            // 比如 7101 -> V1 = V1 + 1
            {
                uint8_t x = (opcode & 0x0F00) >> 8;
                uint8_t nn = (opcode & 0x00FF);
                cpu->V[x] += nn;
                printf("指令执行: V[%d] += 0x%X\n", x, nn);
                cpu->pc += 2;
            }
            break;

        case 0xD000:
            // 0xDXYN: 在 (VX, VY) 画一个宽 8 高 N 的精灵
            {
                // 1. 取出坐标 (X, Y)
                uint16_t x = cpu->V[(opcode & 0x0F00) >> 8];
                uint16_t y = cpu->V[(opcode & 0x00F0) >> 4];
                uint16_t height = opcode & 0x000F; // N (高度)
                uint16_t pixel;

                // 2. 重置碰撞标志 VF = 0
                cpu->V[0xF] = 0;

                // 3. 逐行绘制
                for (int yline = 0; yline < height; yline++) {
                    // 从内存 I 处取出一行像素数据 (1个字节 = 8个点)
                    pixel = cpu->memory[cpu->I + yline];

                    // 4. 逐个比特处理 (一行8个点)
                    for (int xline = 0; xline < 8; xline++) {
                        // 检查数据里这一个 bit 是不是 1 (0x80 是 10000000)
                        if ((pixel & (0x80 >> xline)) != 0) {
                            // 算出在屏幕数组 gfx 中的绝对位置
                            // % 64 和 % 32 是为了防止画出屏幕外面 (Wrap around)
                            int idx = ((x + xline) % 64) + ((y + yline) % 32) * 64;
                            
                            // 碰撞检测：如果屏幕上这个点本来就是亮的(1)
                            if (cpu->gfx[idx] == 1) {
                                cpu->V[0xF] = 1; // 撞车了！
                            }
                            
                            // 异或操作：亮变暗，暗变亮
                            cpu->gfx[idx] ^= 1;
                        }
                    }
                }
                
                // 别忘了刷新标志，告诉 Main 函数“屏幕变了，该重画了”
                // (你可以自己在 struct Chip8 里加个 bool drawFlag，也可以不管，每帧都画)
                cpu->pc += 2;
            }
            break;

        case 0xA000:
            // 0xANNN: 设置 I = NNN
            cpu->I = opcode & 0x0FFF;
            cpu->pc += 2;
            break;

        // ... 以后还有更多指令填在这里 ...

        default:
            printf("尚未实现的指令: 0x%X\n", opcode);
            cpu->pc += 2; // 遇到不认识的也跳过，防止死循环
            break;
    }

    // 3. 更新计时器 (以后做)
    if (cpu->delay_timer > 0) cpu->delay_timer--;
    if (cpu->sound_timer > 0) cpu->sound_timer--;
}

// 在 main 函数上面加
void debug_render(Chip8 *cpu) {
    // 这是一个清屏命令 (Linux/Mac 专用)，为了不让屏幕闪烁太厉害
    printf("\033[H\033[J"); 

    // 遍历 32 行
    for (int y = 0; y < 32; ++y) {
        // 遍历 64 列
        for (int x = 0; x < 64; ++x) {
            // 如果显存是 1，画个实心块，否则画空格
            if (cpu->gfx[x + (y * 64)]) {
                printf("█"); 
            } else {
                printf(" ");
            }
        }
        printf("\n"); // 换行
    }
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
// ...
    while (1) {
        emulate_cycle(&cpu);
        
        // 这一步虽然暴力，但对 IBM Logo 足够了
        // 每次 CPU 动一下，我们就把显存打印出来看看
        debug_render(&cpu);
        
        usleep(10000); // 10毫秒刷一次，稍微快点
    }
    // ...

    return 0;
}
