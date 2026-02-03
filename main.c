#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h> // Required for usleep
#include <stdint.h> // 引入 uint8_t 这种标准类型

#include <time.h>
#include <SDL2/SDL.h> // 引入图形库

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

    // 新增：只有需要画图时才刷新屏幕
    bool draw_flag;

} Chip8;

uint8_t keymap[16] = {
    SDLK_x, SDLK_1, SDLK_2, SDLK_3,  // 0, 1, 2, 3
    SDLK_q, SDLK_w, SDLK_e, SDLK_a,  // 4, 5, 6, 7
    SDLK_s, SDLK_d, SDLK_z, SDLK_c,  // 8, 9, A, B
    SDLK_4, SDLK_r, SDLK_f, SDLK_v   // C, D, E, F
};

// 2. 初始化函数 (给 CPU 通电复位)
void init_cpu(Chip8 *cpu) {
    // PC 起始位置设为 0x200 (512)，因为前 512 字节是留空的
    cpu->pc = 0x200;
    cpu->I = 0;
    cpu->sp = 0;
    cpu->draw_flag = true;
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
            // 0x00E0: 清屏 (你之前写过了)
            if ((opcode & 0x00FF) == 0x00E0) {
                memset(cpu->gfx, 0, 64 * 32);
                cpu->draw_flag = true;
                cpu->pc += 2;
            } 
            // === 新增：0x00EE 返回指令 (Return from Subroutine) ===
            else if ((opcode & 0x00FF) == 0x00EE) {
                // 1. 栈指针往回退一格 (回到上一层)
                cpu->sp--;
                // 2. 把 PC 恢复成当时存进去的地址
                cpu->pc = cpu->stack[cpu->sp];
                // 3. 既然是“恢复”，那就已经是下一条指令的地址了
                // (我们在 Call 的时候存的就是 pc+2)
                cpu->pc += 2;
                // printf("指令执行: Return -> %X\n", cpu->pc);
            }
            else {
                // 别的 0x0NNN 指令我们不管
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

        // === 新增：0x2NNN 函数调用 (Call Subroutine) ===
        case 0x2000:
            // 1. 把当前的 PC (也就是回来后该执行的地方) 存进栈里
            // 注意：这里我们还没 +2，所以回来后会重新执行这一行？
            // 不对！我们要存的是“下一行”，所以通常是存 pc
            // 但 CHIP-8 只有在执行完 switch 后不统一 +2 才需要手动处理。
            // 你的代码结构里，case 内部都写了 pc += 2。
            // 所以这里最稳妥的写法是：
            
            cpu->stack[cpu->sp] = cpu->pc; // 记下“我现在在哪”
            cpu->sp++;                     // 栈指针进一格
            
            // 2. 跳转到新地址 NNN
            cpu->pc = opcode & 0x0FFF;
            
            // printf("指令执行: Call %X\n", cpu->pc);
            // 注意：这里不需要 cpu->pc += 2，因为我们直接跳过去了
            break;

        case 0x3000:
            {
                uint8_t x = (opcode & 0x0F00) >> 8;
                uint8_t nn = (opcode & 0x00FF);
                
                // 如果条件成立，pc 多加 2 (也就是总共加 4，跳过下一条指令)
                if (cpu->V[x] == nn) {
                    cpu->pc += 4;
                } else {
                    cpu->pc += 2;
                }
            }
            break;

        // ... (保持 6000, 7000, A000, D000 不变) ...

        // ... (在 case 0x3000 后面加上这个) ...

        // === 新增：4XNN (如果 VX != NN，跳过下一条) ===
        case 0x4000:
            {
                uint8_t x = (opcode & 0x0F00) >> 8;
                uint8_t nn = (opcode & 0x00FF);
                
                // 逻辑和 3XNN 正好相反：如果不相等，就跳
                if (cpu->V[x] != nn) {
                    cpu->pc += 4;
                } else {
                    cpu->pc += 2;
                }
            }
            break;
        // === 新增：CXNN (随机数) ===
        case 0xC000:
            {
                uint8_t x = (opcode & 0x0F00) >> 8;
                uint8_t nn = (opcode & 0x00FF);
                
                // 生成一个 0-255 的随机数，然后和 NN 做与运算
                // 需要在 main 开头加 srand(time(NULL));
                cpu->V[x] = (rand() % 256) & nn;
                
                cpu->pc += 2;
            }
            break;

        // === 新增：EXNN (按键跳过逻辑) ===
        case 0xE000:
            {
                uint8_t x = (opcode & 0x0F00) >> 8;
                
                switch (opcode & 0x00FF) {
                    
                    case 0x9E: // EX9E: 如果按键 V[x] 被按下了，就跳过下一条
                        {
                            uint8_t key_index = cpu->V[x];
                            if (cpu->key[key_index] != 0) {
                                cpu->pc += 4;
                            } else {
                                cpu->pc += 2;
                            }
                        }
                        break;
                        
                    case 0xA1: // EXA1: 如果按键 V[x] 没被按下，就跳过 (你的报错 E0A1 就在这)
                        {
                            uint8_t key_index = cpu->V[x];
                            if (cpu->key[key_index] == 0) {
                                cpu->pc += 4;
                            } else {
                                cpu->pc += 2;
                            }
                        }
                        break;
                        
                    default:
                        printf("Unknown Opcode: 0x%X\n", opcode);
                        cpu->pc += 2;
                }
            }
            break;

        // === 新增：FX 系列指令 (计时器、内存等) ===
        case 0xF000:
            {
                uint8_t x = (opcode & 0x0F00) >> 8;
                
                // F 系列指令看最后两位 (07, 15, 18, 1E...)
                switch (opcode & 0x00FF) {
                    
                    case 0x07: // FX07: 把计时器的时间读给 VX (你报错的那个 F007)
                        cpu->V[x] = cpu->delay_timer;
                        cpu->pc += 2;
                        break;

                    case 0x15: // FX15: 把 VX 的值设置给 计时器 (Pong 肯定也会用到)
                        cpu->delay_timer = cpu->V[x];
                        cpu->pc += 2;
                        break;
                    
                    case 0x18: // FX18: 设置声音计时器
                        cpu->sound_timer = cpu->V[x];
                        cpu->pc += 2;
                        break;

                    default:
                        printf("Unknown Opcode: 0x%X\n", opcode);
                        cpu->pc += 2;
                }
            }
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

        // === 新增：8XYN 算术与逻辑运算 ===
        case 0x8000:
            {
                uint8_t x = (opcode & 0x0F00) >> 8;
                uint8_t y = (opcode & 0x00F0) >> 4;
                
                // 8 系列还需要看最后一位 (0~E) 来区分具体是加减乘除
                switch (opcode & 0x000F) {
                    
                    case 0x0: // 8XY0: Set Vx = Vy (赋值)
                        cpu->V[x] = cpu->V[y];
                        break; // 记得 break，最后统一 pc+=2

                    case 0x1: // 8XY1: Set Vx = Vx OR Vy (或运算)
                        cpu->V[x] |= cpu->V[y];
                        break;

                    case 0x2: // 8XY2: Set Vx = Vx AND Vy (与运算)
                        cpu->V[x] &= cpu->V[y];
                        break;

                    case 0x3: // 8XY3: Set Vx = Vx XOR Vy (异或)
                        cpu->V[x] ^= cpu->V[y];
                        break;

                    case 0x4: // 8XY4: Set Vx = Vx + Vy (加法，带进位 VF)
                        {
                            // 如果结果溢出 (>255)，VF = 1
                            uint16_t sum = cpu->V[x] + cpu->V[y];
                            if (sum > 255) {
                                cpu->V[0xF] = 1;
                            } else {
                                cpu->V[0xF] = 0;
                            }
                            // 存回 8 位的结果 (自动截断)
                            cpu->V[x] = sum & 0xFF;
                        }
                        break;

                    case 0x5: // 8XY5: Set Vx = Vx - Vy (减法，带借位 VF)
                        {
                            // 如果 Vx > Vy，说明不借位，VF = 1 (这是 CHIP-8 的怪癖)
                            if (cpu->V[x] >= cpu->V[y]) {
                                cpu->V[0xF] = 1;
                            } else {
                                cpu->V[0xF] = 0;
                            }
                            cpu->V[x] -= cpu->V[y];
                        }
                        break;

                    // ... 还有 8XY6, 8XY7, 8XYE 等位移指令，Pong 暂时用不到，先不管 ...

                    default:
                        printf("Unknown Opcode: 0x%X\n", opcode);
                }
                cpu->pc += 2; // 所有的 8 系列指令都要 +2
            }
            break;    

        case 0xA000:
            // 0xANNN: 设置 I = NNN
            cpu->I = opcode & 0x0FFF;
            cpu->pc += 2;
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
                cpu->draw_flag = true; // 告诉主循环：屏幕变了，该画了！
                cpu->pc += 2;
            }
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

int main(int argc, char *argv[]) {
    srand(time(NULL)); // <--- 加这行，初始化随机数种子

    if (argc < 2) { printf("Usage: ./chip8 <rom>\n"); return 1; }

    // === SDL 初始化 ===
    SDL_Init(SDL_INIT_VIDEO);
    // 放大 10 倍显示，方便看清
    SDL_Window *window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 320, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);

    Chip8 cpu;
    init_cpu(&cpu);
    if (!load_rom(&cpu, argv[1])) { printf("Failed to load ROM\n"); return 1; }

    // 屏幕缓冲区 (RGBA格式)
    uint32_t pixels[64 * 32]; 
    int running = 1;
    SDL_Event event;

    // === 主循环 ===
    while (running) {
        // 1. 模拟 CPU 周期 (每帧跑 10 个指令，加速绘制过程)
        for (int i = 0; i < 10; i++) {
            emulate_cycle(&cpu);
        }

        // 2. 处理退出事件和键盘输入
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            
            // 键盘按下
            if (event.type == SDL_KEYDOWN) {
                for (int i = 0; i < 16; ++i) {
                    if (event.key.keysym.sym == keymap[i]) {
                        cpu.key[i] = 1;
                    }
                }
            }
            // 键盘松开
            if (event.type == SDL_KEYUP) {
                for (int i = 0; i < 16; ++i) {
                    if (event.key.keysym.sym == keymap[i]) {
                        cpu.key[i] = 0;
                    }
                }
            }
        }

        // 3. 只有当 draw_flag 为 true 时才更新画面 (节省资源)
        if (cpu.draw_flag) {
            cpu.draw_flag = false;
            
            for (int i = 0; i < 2048; ++i) {
                uint8_t pixel = cpu.gfx[i];
                // 像素为1 -> 白色(FFFFFFFF)，像素为0 -> 黑色(000000FF)
                pixels[i] = (pixel == 1) ? 0xFFFFFFFF : 0x000000FF; 
            }

            SDL_UpdateTexture(texture, NULL, pixels, 64 * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        // 4. 控制帧率 (稍微休眠一下)
        SDL_Delay(16); // 约 60FPS
    }

    // 清理
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
