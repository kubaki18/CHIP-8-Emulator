#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 64
#define HEIGHT 32
#define PIXEL_SIDE 10               // Length of the display's pixel's side
#define PIXEL_ON 225                // Color of a pixel when turned on
#define PIXEL_OFF 15                // Color of a pixel when turned off

#define IPS 700                     // Instructions per second

#define MEMORY_SIZE 4096            // 4KB of memory
#define STARTING_ADDRESS 0x0200     // Initial value of PC
#define STACK_SIZE 16               // How many entries allowed in stack
#define REGISTERS_COUNT 0x10        // How many general purpose registers allowed
#define FONT_ADDRESS 0x0050         // Address at which font is stored

#define IR (memory + ir)            // Macro turning I/ir register to a pointer to the location in memory it points to
#define PC (memory + pc)            // Macro turning pc to a pointer to the location in memory it points to

#define FN (opcode >> 0xC)          // First nibble of opcode
#define X ((opcode >> 0x8) & 0xF)   // Second nibble of opcode, which often points to a general purpose register
#define Y ((opcode >> 0x4) & 0xF)   // Third nibble of opcode, which often points to a general purpose register
#define N (opcode & 0xF)            // Last nibble of opcode
#define NN (opcode & 0xFF)          // Last two nibbles of opcode
#define NNN (opcode & 0xFFF)        // Last three nibbles of opcode

#define DELTA_T (((double)(clock() - last_t))/CLOCKS_PER_SEC)

#ifndef SMALL_ENDIAN
#define DXYN_COND (((pixel_data >> j) & 0x1) == 1) 
#else
#define DXYN_COND (((pixel_data >> (7 - j)) & 0x1) == 1)
#endif


typedef struct {
    SDL_Rect rect;
    bool state;
} Pixel;


uint8_t *memory;
uint16_t pc;                // PC - Program Counter
uint16_t ir;                // IR - Index Register
uint16_t stack[STACK_SIZE];     // Stack to save addresses to, when entering a subroutine
int8_t stack_top = -1;      // The current stack entry index
uint8_t dt = 0;             // DT - Delay Timer
uint8_t st = 0;             // ST - Sound Timer
uint8_t v[REGISTERS_COUNT]; // VR - Variable Registers

Pixel pixels[HEIGHT][WIDTH];    // 2D array of pixels to be displayed on screen
SDL_Window * win;
SDL_Renderer * rend;

bool running = false;
uint16_t opcode;            // A variable storing 2 bytes long instruction for the emulator


void InitializeDisplay();   // Initialize all necessary systems
void InitializePixels();    // Place pixels on the screen, reset values to OFF
void InitializeFont();      // Put font data in memory
void RefreshScreen();       // Clear screen, redraw all pixels, and refresh screen
void PrintMemory();         // Print a hexdump of the memory to the terminal
void FetchOpcode();         // Read an opcode from memory and write it to the 'opcode' variable
void PushStack();           // Push the current address (value of PC) to the stack
void PopStack();            // Set PC to the top value stored in stack and remove it from stack


int main(int argc, char *argv[]) {
    memory = calloc(MEMORY_SIZE, sizeof(uint8_t));
    pc = STARTING_ADDRESS;
    ir = STARTING_ADDRESS; 

    if (argc > 1) {
        FILE *file;
        int16_t c;

        file = fopen(argv[1], "rb");
        if (file == NULL) {
            printf("Error opening the file. Exiting...\n");
            return 1;
        }
        do {
            c = fgetc(file);
            *PC = c;
            pc++;
        } while (c != EOF);
        fclose(file);
        pc = STARTING_ADDRESS;
        PrintMemory();
        //getchar();
        running = true;
    }

    InitializeDisplay();

    clock_t seconds_t, last_t; 
    seconds_t = clock();

    while(running) {
        // Decrement values of timers by 1 every second
        if (((double)(clock() - seconds_t))/CLOCKS_PER_SEC > 1.0) {
            seconds_t += 1.0 * CLOCKS_PER_SEC;
            if (dt > 0) {
                dt--;
            }
            if (st > 0) {
                st--;
            }
        }
        // Make sure to run at a certain frequency of instructions per second
        last_t = clock();
        while (DELTA_T < (1.0/IPS)) {
            continue;
        }
        FetchOpcode();
        /*
        printf("%f", delta_t);
        printf("|%x|\t", pc);
        printf("%04x\n", opcode);
        for (int i = 0; i < 16; i++) {
            printf("| v%x %02x ", i, v[i]);
        }
        printf("\n");
        for (int i = 0; i < 16; i++) {
            printf("| s%x %02x ", i, stack[i]);
        }
        printf("| ir %04x |\n", ir);
        */
        //getchar();
        switch (FN) {
            case 0x0:
                if (NNN == 0x0E0) {
                    for (int i = 0; i < HEIGHT; i++) {
                        for (int j = 0; j < WIDTH; j++) {
                            pixels[i][j].state = false;
                        }
                    }
                    break;
                } 
                if (NNN == 0x0EE) {
                    PopStack();
                    break;
                } 
                running = false;
                break;
            case 0x1: 
                pc = NNN;
                break;
            case 0x2:
                PushStack();
                pc = NNN;
                break;
            case 0x3:
                if (v[X] == NN) {
                    pc += 2;
                }
                break;
            case 0x4:
                if (v[X] != NN) {
                    pc += 2;
                }
                break;
            case 0x5:
                if (N != 0) {
                    running = false;
                }
                if (v[X] == v[Y]) {
                    pc += 2;
                }
                break;
            case 0x6:
                v[X] = NN;
                break;
            case 0x7:
                v[X] = (v[X] + NN) & 0xFF;
                break;
            case 0x8:
                switch (N) {
                    case 0x0:
                        v[X] = v[Y];
                        break;
                    case 0x1:
                        v[X] |= v[Y];
                        break;
                    case 0x2:
                        v[X] &= v[Y];
                        break;
                    case 0x3:
                        v[X] ^= v[Y];
                        break;
                    case 0x4:
                        v[0xF] = ((uint16_t)(v[X] + v[Y]) & 0x100) >> 0x8;
                        v[X] = (v[X] + v[Y]) & 0xFF;
                        break;
                    case 0x5:
                        v[0xF] = 0x0;
                        if (v[X] > v[Y]) {
                            v[0xF] = 0x1;
                        }
                        v[X] = (v[X] - v[Y]) & 0xFF;
                        break;
                    case 0x6:
#ifdef COSMAC 
                        v[X] = v[Y];
#endif
                        v[0xF] = v[X] & 0x1;
                        v[X] >>= 0x1;
                        break;
                    case 0x7:
                        v[0xF] = 0x0;
                        if (v[Y] > v[X]) {
                            v[0xF] = 0x1;
                        }
                        v[X] = v[Y] - v[X] & 0xFF;
                        break;
                    case 0xE:
#ifdef COSMAC
                        v[X] = v[Y];
#endif
                        v[0xF] = (v[X] >> 0x7) & 0x1;
                        v[X] <<= 0x1;
                        break;
                    default:
                        running = false;
                };
                break;
            case 0x9:
                if (N != 0) {
                    running = false;
                }
                if (v[X] != v[Y]) {
                    pc += 2;
                }
                break;
            case 0xA:
                ir = NNN;
                break;
            case 0xB:
                pc = v[0] + NNN;
#ifndef COSMAC
                pc = v[X] + NNN;
#endif
                break;
            case 0xC:
                v[X] = (uint8_t)random() & NN;
                break;
            case 0xD: {
                uint8_t x = v[X] % 64;
                uint8_t y = v[Y] % 32;
                v[0xF] = 0x0;
                for (int i = 0; i < N; i++) {
                    uint8_t pixel_data = *(IR + i);
                    for (int j = 0; j < 8; j++) {
                        if DXYN_COND {
                            if (pixels[y + i][x + j].state == true) {
                                v[0xF] = 0x1;
                            }
                            pixels[y + i][x + j].state = ~pixels[y + i][x + j].state;
                        }
                        if (x + j == WIDTH - 1) {
                            break;
                        }
                    }
                    if (y + i == HEIGHT - 1) {
                        break;
                    }
                }
                break;
            }
            case 0xF:
                switch (NN) {
                    case 0x07:
                        v[X] = dt;
                        break;
                    case 0x15:
                        dt = v[X];
                        break;
                    case 0x18:
                        st = v[X];
                        break;
                    case 0x1E:
                        ir = (ir + v[X]) & 0xFFFF;
#ifndef COSMAC
                        if (ir > 0x1000)
                            v[0xF] = 0x1;
#endif
                        break;
                    case 0x29:
                        ir = FONT_ADDRESS + v[X] * 5;
                        break;
                    case 0x33:
                        *IR = (uint8_t)(v[X]/100);
                        *(IR+1) = (uint8_t)(v[X]/10)%10;
                        *(IR+2) = v[X]%10;
                        break;
                    case 0x55:
#ifndef COSMAC
                        for (int i = 0; i <= X; i++) {
                            *(IR+i) = v[i];
                        }
#else
                        for (int i = 0; i <= X; i++) {
                            *(IR) = v[i];
                            ir++;
                        }
#endif
                        break;
                    case 0x65:
#ifndef COSMAC
                        for (int i = 0; i <= X; i++) {
                            v[i] = *(IR+i);
                        }
#else
                        for (int i = 0; i <= X; i++) {
                            v[i] = *(IR);
                            ir++;
                        }
#endif
                        break;
                    default:
                        running = false;
                }
                break;
            default:
                running = false;
        };
        RefreshScreen();
    }

    getchar();
    // Cleanup
    SDL_Quit();
    free(memory);
    return 0;
}

void InitializeDisplay() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        running = false;
        return;
    }

    win = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH * PIXEL_SIDE, HEIGHT * PIXEL_SIDE, 0);
    rend = SDL_CreateRenderer(win, -1, 0);

    InitializePixels();
    InitializeFont();
    RefreshScreen();
}

void InitializePixels() {
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            pixels[i][j].rect.x = j * PIXEL_SIDE;
            pixels[i][j].rect.y = i * PIXEL_SIDE;
            pixels[i][j].rect.h = PIXEL_SIDE;
            pixels[i][j].rect.w = PIXEL_SIDE;
            pixels[i][j].state = false;
        }
    }
}

void InitializeFont() {
    uint8_t font[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    uint8_t * ptr = memory + FONT_ADDRESS;
    for (int i = 0; i < 80; i++) {
        *ptr = font[i];
        ptr++;
    }
}

void RefreshScreen() {
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
    SDL_RenderClear(rend);
    for (int i = 0; i < HEIGHT; i++) {
        for (int j = 0; j < WIDTH; j++) {
            uint8_t color;
            if (pixels[i][j].state) {
                color = PIXEL_ON;
            } else {
                color = PIXEL_OFF;
            }
            SDL_SetRenderDrawColor(rend, color, color, color, 255);
            SDL_RenderDrawRect(rend, &pixels[i][j].rect);
            SDL_RenderFillRect(rend, &pixels[i][j].rect);
        }
    }
    SDL_RenderPresent(rend);
}

void PrintMemory() {
    for (int i = 0x0; i < MEMORY_SIZE; i += 8) {
        printf("%04x\t\t", i);
        for (int j = 0; j < 8; j+=2) {
            printf("%02x%02x\t", *(memory+i+j), *(memory+i+j+1));
        }
        printf("\n");
    }
}

void FetchOpcode() {
    opcode = (*(PC) << 8) | *(PC+1);
    pc += 2;
}

void PushStack() {
    if (stack_top < STACK_SIZE) {
        stack[++stack_top] = pc;
    } else {
        running = false;
    }
}

void PopStack() {
    if (stack_top >= 0) {
        pc = stack[stack_top--];
    } else {
        running = false;
    }
}
