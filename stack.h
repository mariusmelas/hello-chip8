#pragma once
#define STACK_SIZE 16

struct Stack {
    int len;
    int elements;
    int *stack;

};

void push_stack(struct Stack *stack, int value);
int pop_stack(struct Stack *stack);
void print_stack(struct Stack *stack);