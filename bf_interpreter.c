#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#define BF_MEMORY_SIZE (1 << 20)
#define BF_CODE_SIZE (1 << 20)
#define BF_ADDR_MASK (BF_MEMORY_SIZE - 1)

struct bf_fd
{
    char *filename;
    int fd;
    bool needs_close;
};

struct bf_interpreter
{
    uint8_t memory[BF_MEMORY_SIZE];
    char code[BF_CODE_SIZE];
    unsigned int memory_addr;;
    char *code_ptr;
    struct bf_fd output;
    struct bf_fd input;
};

void bf_fd_open_for_output(struct bf_fd *bfd, const char *filename)
{
    if (!filename)
    {
        bfd->fd = STDOUT_FILENO;
        bfd->needs_close = false;
        bfd->filename = strdup("<stdout>");
    }
    else
    {
        bfd->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (bfd->fd < 0)
        {
            fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
            exit(1);
        }
        bfd->needs_close = true;
        bfd->filename = strdup(filename);
    }
}

void bf_fd_open_for_input(struct bf_fd *bfd, const char *filename)
{
    if (!filename)
    {
        bfd->fd = STDIN_FILENO;
        bfd->needs_close = false;
        bfd->filename = strdup("<stdin>");
    }
    else
    {
        bfd->fd = open(filename, O_RDONLY);
        if (bfd->fd < 0)
        {
            fprintf(stderr, "Error: Could not open file %s for input\n", filename);
            exit(1);
        }
        bfd->needs_close = true;
        bfd->filename = strdup(filename);
    }
}

void bf_fd_close(struct bf_fd *bfd)
{
    if (bfd->needs_close)
    {
        close(bfd->fd);
    }
    free(bfd->filename);
}

void bf_code_read(struct bf_interpreter *bfi, const char *filename)
{
    FILE *code_file = stdin;

    code_file = fopen(filename, "r");
    if (!code_file)
    {
        fprintf(stderr, "Error: Could not open source code %s\n", filename);
        exit(1);
    }

    size_t code_size = fread(bfi->code, 1, BF_CODE_SIZE - 1, code_file);
    if (code_size == 0)
    {
        fprintf(stderr, "Error: empty source code\n");
        exit(1);
    }
    bfi->code[code_size] = '\0';
    fclose(code_file);
}

void bf_interpreter_init(struct bf_interpreter *interpreter, const char *filename_bfcode, const char *filename_in, const char *filename_out)
{
    memset(interpreter->memory, 0, BF_MEMORY_SIZE);
    interpreter->memory_addr = 0;
    bf_code_read(interpreter, filename_bfcode);
    interpreter->code_ptr = interpreter->code;
    bf_fd_open_for_input(&interpreter->input, filename_in);
    bf_fd_open_for_output(&interpreter->output, filename_out);
}

void bf_interpreter_deinit(struct bf_interpreter *bfi)
{
    bf_fd_close(&bfi->input);
    bf_fd_close(&bfi->output);
}

void bf_interpreter_step(struct bf_interpreter *bfi)
{
    char inst = *bfi->code_ptr;
    switch (inst)
    {
    case '>':
        bfi->memory_addr = (bfi->memory_addr + 1) & BF_ADDR_MASK;
        break;
    case '<':
        bfi->memory_addr = (bfi->memory_addr - 1) & BF_ADDR_MASK;
        break;
    case '+':
        bfi->memory[bfi->memory_addr]++;
        break;
    case '-':
        bfi->memory[bfi->memory_addr]--;
        break;
    case '.':
        write(bfi->output.fd, &bfi->memory[bfi->memory_addr], 1);
        break;
    case ',':
        {
            uint8_t c;
            if (read(bfi->input.fd, &c, 1) == 1)
            {
                bfi->memory[bfi->memory_addr] = c;
            }
            else
            {
                bfi->memory[bfi->memory_addr] = 0;
            }
        }
        break;
    case '[':
        if (bfi->memory[bfi->memory_addr] == 0)
        {
            int bracket_count = 1;
            while (bracket_count > 0 && *++bfi->code_ptr != '\0')
            {
                if (*bfi->code_ptr == '[')
                {
                    bracket_count++;
                }
                else if (*bfi->code_ptr == ']')
                {
                    bracket_count--;
                }
            }
            if (bracket_count > 0)
            {
                fprintf(stderr, "Error: Unmatched '[' in source code\n");
                exit(1);
            }
        }
        break;
    case ']':
        if (bfi->memory[bfi->memory_addr] != 0)
        {
            int bracket_count = 1;
            while (bracket_count > 0 && --bfi->code_ptr >= bfi->code)
            {
                if (bfi->code_ptr < bfi->code)
                {
                    fprintf(stderr, "Error: Unmatched ']' in source code\n");
                    exit(1);
                }
                if (*bfi->code_ptr == '[')
                {
                    bracket_count--;
                }
                else if (*bfi->code_ptr == ']')
                {
                    bracket_count++;
                }
            }
            if (bracket_count > 0)
            {
                fprintf(stderr, "Error: Unmatched ']' in source code\n");
                exit(1);
            }
        }
        break;
    case '\0':
        fprintf(stderr, "Error: End of code reached\n");
        exit(1);
    default:
        if (!isspace(inst))
        {
            if (isprint(inst))
            {
                fprintf(stderr, "Error: Invalid instruction '%c' in source code\n", inst);
            }
            else
            {
                fprintf(stderr, "Error: Invalid instruction (0x%02x) in source code\n", inst);
            }
            exit(1);
        }
        break;
    }
}

void bf_interpreter_run(struct bf_interpreter *bfi)
{
    bfi->code_ptr = bfi->code;
    while (1)
    {
        if (*bfi->code_ptr == '\0')
        {
            break;
        }
        bf_interpreter_step(bfi);
        bfi->code_ptr++;
    }
}

uint64_t timespec_diff(const struct timespec *a, const struct timespec *b)
{
    uint64_t res;
    if (a->tv_nsec < b->tv_nsec)
    {
        res = 1000000000 + a->tv_nsec - b->tv_nsec;
        res += 1000000000 * (a->tv_sec - b->tv_sec - 1);
    }
    else
    {
        res = a->tv_nsec - b->tv_nsec;
        res += 1000000000 * (a->tv_sec - b->tv_sec);
    }
    return res;
}

// return microseconds
unsigned int bf_interpreter_run_measure_elapsed(struct bf_interpreter *bfi)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    bf_interpreter_run(bfi);
    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t elapsed = timespec_diff(&end, &start);
    return (unsigned int)(elapsed / 1000);
}

int main(int argc, char *argv[])
{
    const char *bfcode_file = NULL;
    const char *input_file = NULL;
    const char *output_file = NULL;
    if (argc > 1)
    {
        bfcode_file = argv[1];
    }
    else
    {
        fprintf(stderr, "Usage: %s <bf file> [input [output]]\n", argv[0]);
        return 1;
    }
    if (argc > 2)
    {
        input_file = argv[2];
    }
    if (argc > 3)
    {
        output_file = argv[3];
    }
    struct bf_interpreter *bfi = malloc(sizeof(struct bf_interpreter));
    bf_interpreter_init(bfi, bfcode_file, input_file, output_file);
    unsigned int elapsed_ms = bf_interpreter_run_measure_elapsed(bfi);
    bf_interpreter_deinit(bfi);
    printf("Execution time: %u us\n", elapsed_ms);
    free(bfi);
    return 0;
}
