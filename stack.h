#pragma once
#define STACK_SIZE 16

struct Stack {
    int len;
    int elements;
    short *stack;

};

void push_stack(struct Stack *stack, char value);
void pop_stack(struct Stack *stack);
void print_stack(struct Stack *stack);