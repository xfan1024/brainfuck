#include <stdio.h>
#include <stdint.h>

#define MEM_SIZE (1 << 20)
uint8_t memory[MEM_SIZE];

typedef uint8_t (*readbyte_func)(void *);
typedef void (*writebyte_func)(void *, char);

extern void bfcode(void *ctx, void *memory, readbyte_func readbyte, writebyte_func writebyte);

void bf_writebyte(void *ctx, char c)
{
    putchar(c);
}

uint8_t bf_readbyte(void *ctx)
{
    int ch = getchar();
    if (ch == EOF)
    {
        return 0;
    }
    return (uint8_t)ch;
}

int main()
{
    void *ctx = NULL;
    bfcode(ctx, memory, bf_readbyte, bf_writebyte);

    return 0;
}