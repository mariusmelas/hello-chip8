// Compile: gcc -o main main.c `sdl2-config --cflags --libs`

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include <SDL2/SDL.h>

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

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
    // Stack
    // Will be used to store 12bit addresses
    // Probably enough with 16
    short stack[16];
    load_rom("./ibm.ch8", memory);
	
	

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
                default:
                    break;
            }           
        }   


		// Fetch next expression
        struct opcode opcode = fetch_instruction(&PC, memory);
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
						break;
					default:
						printf("%x is not a valid instructon at PC=%d\n",opcode.opcode, PC);
						exit(-1);
				}
				break;
            case 0x1:
                // 1NNN	Flow	goto NNN;	Jumps to address NNN.
                PC = opcode.NNN;
				//printf("opcode.NNN = %x\n",opcode.NNN);
				//printf("memory: %x %x\n", memory[opcode.NNN], memory[opcode.NNN +1] );
                break;
            case 0x6:
                // 6XNN	Const	Vx = NN	Sets VX to NN.
                registers[opcode.X] = opcode.NN;
                break;
            case 0x7:
                // 7XNN	Const	Vx += NN	Adds NN to VX (carry flag is not changed).
                registers[opcode.X] += opcode.NN;
                break;
            case 0xa:
                //ANNN	MEM	I = NNN	Sets I to the address NNN.
				printf("update I\n");
                I = opcode.NNN;
				break;
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
                        int x = (col + vx) % DISPLAY_WIDTH;
                        int y = (row + vy) % DISPLAY_HEIGHT;
    
                        if (pixelValue && display[x][y]) {
                                registers[0xF] = 1;
                        }
                        display[x][y] ^= pixelValue;
                    }
                }

                draw_display(renderer, display);
        
                break;
			default:		
				printf("%x is not a valid instructon at PC=%d\n",opcode.opcode, PC);
				exit(-1);
        }
		
		Uint64 ticks = SDL_GetTicks64();
		double seconds = (double) (ticks - prev_getticks) / 1000.0;
			
		// Wait until 1/60 seconds
		while(seconds < 1 / 60) {
			ticks = SDL_GetTicks64();
			seconds = (double) (ticks - prev_getticks) / 1000.0;
		}
		
		prev_getticks = ticks;

		//SDL_RenderPresent(renderer);
	}

	SDL_Delay(10);

	// Clean up SDL
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit;


	return 0;

}
