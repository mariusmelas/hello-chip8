#include <stdlib.h>
#include <stdio.h>

#include "stack.h"



void push_stack(struct Stack *stack, char value) {
    if((*stack).elements < (*stack).len) {
        (*stack).stack[(*stack).elements++] = value;
    } else {
        // Stack overflow.
        printf("Stack overflow!\n");
        exit(-1);
    }
}

short pop_stack(struct Stack *stack) {
    if((*stack).elements > 0) {  
        short element = (*stack).stack[(*stack).elements - 1];      
        (*stack).stack[(*stack).elements -1] = 0;
        (*stack).elements--;
        return element;
    } else {
        printf("Stack is empty\n");
        exit(-1);
    }
}

void print_stack(struct Stack *stack) {

    for(int i = 0; i < (*stack).len; i++) {
        printf("%d ", (*stack).stack[i]);
    }
    printf("\n");
}

