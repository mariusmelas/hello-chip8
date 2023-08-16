// Compile: gcc -o main main.c stack.c `sdl2-config --cflags --libs`

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#include "./stack.h"

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

#define DEBUG_MODE false

/*
    Using one struct for all instructions.
    Which field that are used is dependent on the first.
*/

struct opcode {
    unsigned short opcode;
    unsigned char first;
    unsigned char X;
    unsigned char Y;
    unsigned char N;
    unsigned char NN;
    unsigned short NNN;
};

struct opcode fetch_instruction(short *PC, unsigned char *memory) {
    struct opcode opcode;
    unsigned char a = htonl(memory[*PC]) >> 24;
    unsigned char b = htonl(memory[*PC +1]) >>  24;
    unsigned short instruction = ((short) a << 8) | b;
	
    // Short is 2 bytes - 16bits
    // Instruction is now ABCD 
    opcode.opcode = instruction;
    // First nibblr
    opcode.first = instruction >> 12;
    // Second nibble
    opcode.X = (instruction >> 8) & 0xF;
    // Third nibble
    opcode.Y = (instruction >> 4) & 0xF;
    // Fourth nibble
    opcode.N = (instruction) & 0xF;
    // Second byte (third and fourth nibble)
    opcode.NN = instruction & 0x00FF;
    // Second, third and fourth nibble
    opcode.NNN = instruction & 0x0FFF;

    *PC += 2;
    return opcode;
}


void draw_display(SDL_Renderer *renderer, unsigned char display[DISPLAY_WIDTH][DISPLAY_HEIGHT]) {
    
    #define PIXEL_SIZE 10
    // Clear the renderer before drawing the updated display
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw the updated display
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            if (display[x][y]) {
                SDL_Rect pixelRect = {x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE};
                SDL_RenderFillRect(renderer, &pixelRect);
            }
        }
    }

    SDL_RenderPresent(renderer);

} 

void load_rom(char *pathname, char *memory) {
    printf("from memory: %c\n", *memory);
    FILE *rom;
    long fsize;

    if((rom = fopen(pathname, "rb")) == NULL) {
        printf("Error open rom file\n");
    }

    // Find size of file
    fseek(rom, 0L, SEEK_END);
    fsize = ftell(rom);
    rewind(rom);
    printf("file size: %d\n",fsize);
    // Read entire file into memory
    fread(memory+0x200, fsize, 1,rom);
    fclose(rom);
}

unsigned char vx_temp;
unsigned char vy_temp;

int main() {
	printf("Hello chip-8 :)\n");

    // Program counter
    // points at currenct incstruction in memory
    short PC = 0x200;
    // Index register
    // Points at location in memory
    short I = 0x00;
    unsigned char display[DISPLAY_WIDTH][DISPLAY_HEIGHT];
    memset(display,0x00, DISPLAY_WIDTH * DISPLAY_HEIGHT);
    unsigned char memory[4096];
    memset(memory,0x00, 4096);
    unsigned char registers[16];

    // Font as sprite data.
    // The font has 16 hexadecimal characters
    // Each character is 4x5 pixels
    char font[80] = { 0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
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

    // Store the font in interpreters memory.
    // From 0x50 by convention.
    for(int i = 0; i < sizeof(font) / sizeof(font[0]); i++) {
        memory[0x50 + i] = font[i];   
    }

    // Delay and sound timer
    // Will be set to a register.
    short delay_timer = -1;
    short sound_timer = -1;

    // Stack
    // Will be used to store 12bit addresses
    // Probably enough with 16
    struct Stack stack;
    stack.len = STACK_SIZE;
    stack.elements = 0;
    stack.stack = malloc(sizeof(int) * STACK_SIZE);

    // Keypad
    // Will be set to 1 on key down, and 0 on key up.
    int numkeys;
    short keypad[16];
    const short keypad_values[16] = { 0x1, 0x2, 0x3, 0xC,
                                      0x4, 0x5, 0x6, 0xD,
                                      0x7, 0x8, 0x9, 0xE,
                                      0xA, 0x0, 0xB, 0xF};

    // Keeps the index for where the key is in keypad[]
    const short keypad_indexes[16] = {13, 0, 1, 2,4,5,6,8,9,10,12,14,3,7,11,15};

    const Uint8* keyboard;

    load_rom("./roms/keypad_test_hap_2006.ch8", memory);


   
	// Set up SDL
	SDL_Window *window;
	SDL_Renderer *renderer;

	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Error initializing SDL\n");
		return -1;
	}
	
	SDL_CreateWindowAndRenderer(DISPLAY_WIDTH * 10,DISPLAY_HEIGHT * 10, 0, &window, &renderer);
	if(!window)	{
	printf("Failed to create window\n");
	return -1;
	}

	
    //SDL_SetRenderDrawColor(renderer, 255,255,255,255);
	//SDL_RenderClear(renderer);
    //SDL_RenderPresent(renderer);	
	// Set up timer
	Uint64 prev_getticks = 0;



    int run_program  = 1;
	while(run_program) {
      SDL_Event e;
      while(SDL_PollEvent(&e) > 0)
        {   
            switch(e.type) {
                case SDL_QUIT:
                    run_program = 0;
                    break;
                case SDL_KEYDOWN:
                    keyboard = SDL_GetKeyboardState(&numkeys);
                    keypad[0] = keyboard[SDL_SCANCODE_1];
                    keypad[1] = keyboard[SDL_SCANCODE_2];
                    keypad[2] = keyboard[SDL_SCANCODE_3];
                    keypad[3] = keyboard[SDL_SCANCODE_4];
                    keypad[4] = keyboard[SDL_SCANCODE_Q];
                    keypad[5] = keyboard[SDL_SCANCODE_W];
                    keypad[6] = keyboard[SDL_SCANCODE_E];
                    keypad[7] = keyboard[SDL_SCANCODE_R];
                    keypad[8] = keyboard[SDL_SCANCODE_A];
                    keypad[9] = keyboard[SDL_SCANCODE_S];
                    keypad[10] = keyboard[SDL_SCANCODE_D];
                    keypad[11] = keyboard[SDL_SCANCODE_F];
                    keypad[12] = keyboard[SDL_SCANCODE_Z];
                    keypad[13] = keyboard[SDL_SCANCODE_X];
                    keypad[14] = keyboard[SDL_SCANCODE_C];
                    keypad[15] = keyboard[SDL_SCANCODE_V];
                    break;
                case SDL_KEYUP:
                    keyboard = SDL_GetKeyboardState(&numkeys);
                    keypad[0] = keyboard[SDL_SCANCODE_1];
                    keypad[1] = keyboard[SDL_SCANCODE_2];
                    keypad[2] = keyboard[SDL_SCANCODE_3];
                    keypad[3] = keyboard[SDL_SCANCODE_4];
                    keypad[4] = keyboard[SDL_SCANCODE_Q];
                    keypad[5] = keyboard[SDL_SCANCODE_W];
                    keypad[6] = keyboard[SDL_SCANCODE_E];
                    keypad[7] = keyboard[SDL_SCANCODE_R];
                    keypad[8] = keyboard[SDL_SCANCODE_A];
                    keypad[9] = keyboard[SDL_SCANCODE_S];
                    keypad[10] = keyboard[SDL_SCANCODE_D];
                    keypad[11] = keyboard[SDL_SCANCODE_F];
                    keypad[12] = keyboard[SDL_SCANCODE_Z];
                    keypad[13] = keyboard[SDL_SCANCODE_X];
                    keypad[14] = keyboard[SDL_SCANCODE_C];
                    keypad[15] = keyboard[SDL_SCANCODE_V];
                    break;

                default:
                    break;
            }           
        }   

        // Fetch next expression
        struct opcode opcode = fetch_instruction(&PC, memory);
    
        // If debug mode and current PC is breakpoint, wait for stepforward.

        if(DEBUG_MODE) {
            // Print current instruction, and options for what to do next.
            printf("PC=%d\n", PC);
            printf("opcode: %x\n", opcode.opcode);
            printf("Press enter to step forward\n");
            unsigned char c;
            while(!isspace((c = getchar()))) {
                // Consume newline
                getchar();
                switch(c) {
                    case 'a':
                        // Print registers
                        printf("Registers V0 - VF:\n");
                        printf("---------------------\n");
                        for(int i = 0; i < sizeof(registers) / sizeof(registers[0]); i++) {
                            printf("V%1x: %x (Decimal: %d)\n", i, registers[i], registers[i]);
                        }
                        printf("---------------------\n");
                        break;
                    default: 
                        break;
                }

            }
            
        }
  
        // Decode and execute instructions
        switch(opcode.first) {
            case 0x00:
                // 0NNN	Call - Calls machine code routine (RCA 1802 for COSMAC VIP) at address NNN. Not necessary for most ROMs.
				switch(opcode.NN) {
					case 0xE0:
                		//00E0	Display	disp_clear()	Clears the screen.
    					SDL_SetRenderDrawColor(renderer, 1,1,1,1);
						SDL_RenderClear(renderer);
						SDL_RenderPresent(renderer);
						break;
					case 0xEE:		
                		//00EE	Flow	return;	Returns from a subroutine.
                        short return_addr = pop_stack(&stack);
                        PC = return_addr;
						break;
					default:
						printf("%x is not a valid instructon at PC=%d\n",opcode.opcode, PC);
                        //break;
                        //continue;
						exit(-1);
				}
				break;
            case 0x1:
                // 1NNN	Flow	goto NNN;	Jumps to address NNN.
                PC = opcode.NNN;
				//printf("opcode.NNN = %x\n",opcode.NNN);
				//printf("memory: %x %x\n", memory[opcode.NNN], memory[opcode.NNN +1] );
                break;
            case 0x2:
                // 2NNN	Flow	*(0xNNN)()	Calls subroutine at NNN.
                // Push next PC to stack.
                push_stack(&stack, PC);
                PC = opcode.NNN;
                break;
            case 0x3:
                //3XNN	Cond	if (Vx == NN)	Skips the next instruction if VX equals NN (usually the next instruction is a jump to skip a code block).
                if(registers[opcode.X] == opcode.NN) {
                    // Skip next instruction.
                    PC += 2;
                }
                break;
            case 0x4:
                //4XNN	Cond	if (Vx != NN)	Skips the next instruction if VX does not equal NN (usually the next instruction is a jump to skip a code block).
                if(registers[opcode.X] != opcode.NN) {
                    PC += 2;
                }
                break;
            
            case 0x5:
                // 5XY0	Cond	if (Vx == Vy)	Skips the next instruction if VX equals VY (usually the next instruction is a jump to skip a code block).
              
                if(registers[opcode.X] == registers[opcode.Y]) {
                    PC += 2;
                }

                
                break;


            case 0x6:
                // 6XNN	Const	Vx = NN	Sets VX to NN.
                registers[opcode.X] = opcode.NN;
                break;
            case 0x7:     
                // 7XNN	Const	Vx += NN	Adds NN to VX (carry flag is not changed).
                registers[opcode.X] += opcode.NN;
                break;
            case 0x8:
                switch(opcode.N) {
                    case 0x0:
                        // 8XY0	Assig	Vx = Vy	Sets VX to the value of VY.
                        registers[opcode.X] = registers[opcode.Y];
                        break;
                    case 0x1:
                        // 8XY1	BitOp	Vx |= Vy	Sets VX to VX or VY. (bitwise OR operation)
                        registers[opcode.X] = registers[opcode.X] | registers[opcode.Y];
                        break;
                    case 0x2:
                        // 8XY2	BitOp	Vx &= Vy	Sets VX to VX and VY. (bitwise AND operation)
                        registers[opcode.X] &= registers[opcode.Y];
                        break;
                    case 0x3:
                        // 8XY3[a]	BitOp	Vx ^= Vy	Sets VX to VX xor VY.
                        registers[opcode.X] ^= registers[opcode.Y];
                        break;
                    case 0x4:

                        // 8XY4	Math	Vx += Vy	Adds VY to VX. 
                        // VF is set to 1 when there's a carry, and to 0 when there is not.

                        // Vf cannot be used as Vx or Vy 
                        vx_temp = registers[opcode.X];
                        vy_temp = registers[opcode.Y];
                        registers[opcode.X] +=  registers[opcode.Y];

                        // Check if there was a carry
                        if(vx_temp + vy_temp > 0xFF) {
                            registers[0xF] = 1;
                        } else {
                            registers[0xF] = 0;
                        }

                        break;
                    case 0x5:
                        // VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there is not.

                        vx_temp = registers[opcode.X];
                        vy_temp = registers[opcode.Y];
                        //unsigned char result = registers[opcode.X] - registers[opcode.Y];
                        registers[opcode.X] =  registers[opcode.X] - registers[opcode.Y];

                        if(vx_temp < vy_temp) {
                            registers[0xF] = 0;
                        } else {
                            registers[0xF] = 1;
                        }

                        break;
                    case 0x6:
                        // 8XY6[a]	BitOp	Vx >>= 1	
                        // Stores the least significant bit of VX in VF and then shifts VX to the right by 1.
                        // vx = 11110111
                        vx_temp = registers[opcode.X];
                        registers[opcode.X] = registers[opcode.X] >> 1;
                        registers[0xF] = vx_temp & 0x1;

                        break;
                    case 0x7:
                        // 8XY7[a]	Math	Vx = Vy - Vx	Sets VX to VY minus VX. 
                        // VF is set to 0 when there's a borrow, and 1 when there is not.

                        vx_temp = registers[opcode.X];
                        vy_temp = registers[opcode.Y];

                        registers[opcode.X] =  registers[opcode.Y] - registers[opcode.X]; 

                        if(vy_temp < vx_temp) {
                            registers[0xF] = 0;
                        } else {
                            registers[0xF] = 1;
                        }

                        break;
                    case 0xE:
                        // 8XYE[a]	BitOp	Vx <<= 1	
                        // Stores the most significant bit of VX in VF and then shifts VX to the left by 1.[b]
                        vx_temp = registers[opcode.X];
                        registers[opcode.X] = registers[opcode.X] << 1;
                        registers[0xF] = (vx_temp & 0x80) >> 7; 

                        break;
                 }

                break;
            case 0x9:
                // 9XY0	Cond	if (Vx != Vy)	Skips the next instruction if VX does not equal VY.
                // (Usually the next instruction is a jump to skip a code block);
                if(registers[opcode.X] != registers[opcode.Y]) {
                    PC += 2;
                }

                break;

            case 0xa:
                //ANNN	MEM	I = NNN	Sets I to the address NNN.
                I = opcode.NNN;
				break;

            case 0xB:
                // BNNN	Flow	PC = V0 + NNN	Jumps to the address NNN plus V0.
                PC = opcode.NNN + registers[0x0];
                break;
            case 0xC:
                // CXNN	Rand	Vx = rand() & NN	Sets VX to the result of a 
                // bitwise and operation on a random number (Typically: 0 to 255) and NN.
                srand(time(NULL));
                short r = rand() % 256;
                registers[opcode.X] = r & opcode.NN;

            case 0xD:
                
                short pixel_size = 10;
                short vx = registers[opcode.X] % 64;
                short vy = registers[opcode.Y] % 32;
                short width = 8;
                short height = opcode.N;
                int pixel_address = I;
                
                SDL_SetRenderDrawColor(renderer, 255,255,255,255);
                SDL_Rect pixel;
                for (int row = 0; row < height; row++) {
                    char sprite_byte = memory[pixel_address + row];
                    for (int col = 0; col < width; col++) {
                        int pixelValue = (sprite_byte >> (7 - col)) & 0x1;
                        
                        // int x = (col + vx) % DISPLAY_WIDTH;
                        // int y = (row + vy) % DISPLAY_HEIGHT;
                        int x = (col + vx);
                        int y = (row + vy);

                        if(x >= 0 && x <= DISPLAY_WIDTH && y >= 0 && y <= DISPLAY_HEIGHT) {

                            if (pixelValue && display[x][y]) {
                                registers[0xF] = 1;
                            }
                            display[x][y] ^= pixelValue;
                            
                        }



                        
                    }
                }

                draw_display(renderer, display);
        
                break;
            
            case 0xE:
                switch(opcode.NN) {
                    case 0x9E:
                        /* EX9E	KeyOp	if (key() == Vx)
                           Skips the next instruction if the key stored in VX is pressed 
                           (usually the next instruction is a jump to skip a code block).
                        */

                        // Get the keypad index of the key stored in vx from keypad_indexes.
                        // If it is pressed, the key will be set to 1 in keypad[]
                        if(keypad[keypad_indexes[opcode.X]]) {
                            PC += 2;
                        }
                        break;
                    case 0xA1:
                        // EXA1	KeyOp	if (key() != Vx)	
                        // Skips the next instruction if the key stored in VX is not pressed 
                        // (usually the next instruction is a jump to skip a code block).
                        
                        // Get the keypad index of the key stored in vx from keypad_indexes.
                        // If it is not pressed, the key will be set to 0 in keypad[]
                        if(!keypad[keypad_indexes[opcode.X]]) {
                            PC += 2;
                        }
                        break;
                    default: 
                        break;
                }
                break;
            case 0xF:
                switch (opcode.NN)
                {
                case 0x07:
                    // FX07	Timer	Vx = get_delay()	Sets VX to the value of the delay timer.
                    registers[opcode.X] = registers[delay_timer];
                    break;
                case 0x0A:
                    // FX0A	KeyOp	Vx = get_key()	A key press is awaited, and then stored in VX 
                    // (blocking operation, all instruction halted until next key event).

                    short key_pressed = -1;

                    // For now just set it to first pressed key it finds.
                    // Should probably handle multiple key presses.

                    for(short i = 0; i < 16; i++) {
                        if(keypad[i]) {
                            key_pressed = i;
                        }
                    }
                    
                    // If there was no key press, decrement the PC and break
                    if(key_pressed == -1) {
                        PC -= 2;
                        break;
                    }

                    // If there was a key press, store it in VX.
        
                    registers[opcode.X] = keypad_values[key_pressed];
                    break;
                case 0x15:
                    //FX15	Timer	delay_timer(Vx)	Sets the delay timer to VX.
                    delay_timer = opcode.X;
                    break;
                case 0x18:
                    //FX18	Sound	sound_timer(Vx)	Sets the sound timer to VX.
                    sound_timer = opcode.X;
                    break;
                case 0x1E:
                    // FX1E	MEM	I += Vx	Adds VX to I. VF is not affected.
                    I += registers[opcode.X];
                    break;
                case 0x29:
                    // FX29	MEM	I = sprite_addr[Vx]	Sets I to the location of 
                    // the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font.
                    
                    // The font is stored from memory address 0x50
                    printf("Set I to character: %x\n", registers[opcode.X]);
                    I = 0x50 + 5 * registers[opcode.X];
                    break;
                case 0x33:
                    // FX33	BCD	
                    // Stores the binary-coded decimal representation of VX, with the hundreds digit in memory at location in I, 
                    // the tens digit at location I+1, and the ones digit at location I+2.
                    
                    memory[I] = registers[opcode.X] / 100 % 10;
                    memory[I + 1] = registers[opcode.X] / 10 % 10;
                    memory[I + 2] = registers[opcode.X] % 10;

                    break;
                case 0x55:
                    // FX55	MEM	reg_dump(Vx, &I)	Stores from V0 to VX (including VX) in memory, starting at address I. 
                    // The offset from I is increased by 1 for each value written, but I itself is left unmodified.

                    for(int i = 0, offset = 0; i <= opcode.X; i++, offset++) {
                        memory[I+offset] = registers[i];

                    }
                    break;
                case 0x65:
                    // FX65	MEM	reg_load(Vx, &I)	Fills from V0 to VX (including VX) with values from memory, starting at address I. 
                    // The offset from I is increased by 1 for each value read, but I itself is left unmodified.
                    for(int i = 0, offset = 0; i <= opcode.X; i++, offset++) {
                        registers[i] =  memory[I+offset];

                    }
                    break;

                default:
                    break;


                break;
            }

            break;

        default:		
            printf(" - %x is not a valid instructon at PC=%d\n",opcode.opcode, PC);
            continue;
            //exit(-1);
        }
		

        // Check if delay if sound timer is set ( might be unnecessary )
    
        // Delay timer
        if(delay_timer >= 0) {
            // Count down timer if its larger than zero.
            if(registers[delay_timer] > 0)
                registers[delay_timer] -= 1;
        }

        // Sound timer
        // Should also play a beep
        if(sound_timer  >= 0) {
        // Count down timer if its larger than zero.
            if(registers[sound_timer] > 0)
                registers[sound_timer] -= 1;
        }


		Uint64 ticks = SDL_GetTicks64();
		double seconds = (double) (ticks - prev_getticks) / 1000.0;
			
		// Wait until 1/60 seconds
		while(seconds < 1.0 / 60.0) {
			ticks = SDL_GetTicks64();
			seconds = (double) (ticks - prev_getticks) / 1000.0;
		}
		
		prev_getticks = ticks;

		SDL_RenderPresent(renderer);
	}


    free(stack.stack);
	SDL_Delay(10);

	// Clean up SDL
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit;


	return 0;

}
