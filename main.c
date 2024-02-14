#include "vma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    arena_t* arena;
    uint64_t size_arena;
    uint64_t size_block;
    uint64_t address;
    while (1) {
        int ok = 0;
        char command[16];
        scanf("%s", command);
        if (strncmp(command, "ALLOC_ARENA", 12) == 0) {
            scanf("%lu", &size_arena);
            arena = alloc_arena(size_arena);
            ok = 1;
        }
        if (strncmp(command, "ALLOC_BLOCK", 12) == 0) {
            scanf("%lu %lu", &address, &size_block);
            alloc_block(arena, address, size_block);
            ok = 1;
        }
        if (strncmp(command, "FREE_BLOCK", 11) == 0) {
            scanf("%lu ", &address);
            free_block(arena, address);
            ok = 1;
        }
        if (strncmp(command, "WRITE", 6) == 0) {
            scanf("%lu %lu", &address, &size_block);
            int8_t* data = malloc((size_block + 1) * sizeof(char));
            char c;
            scanf("%c", &c);
            if (c != '\n') {
                scanf("%c", &c);
            }
            scanf("%c", &data[0]);
            for (int i = 1; i <= size_block; i++) {
                scanf("%c", &data[i]);
            }
            data[size_block] = '\0';
            write(arena, address, size_block, data);
            free(data);
            ok = 1;
        }
        if (strncmp(command, "READ", 6) == 0) {
            scanf("%lu %lu", &address, &size_block);
            read(arena, address, size_block);
            ok = 1;
        }
        if (strncmp(command, "PMAP", 5) == 0) {
            pmap(arena);
            ok = 1;
        }
        /*if(strncmp(command, "MPROTECT", 9) == 0) {
            scanf("%lu", &address);
            char permission[10];
            scanf("%s", permission);
            permission[size_block] = '\0';
            mprot(arena, address, permission);
            ok = 1;
        }*/
        if (strncmp(command, "DEALLOC_ARENA", 14) == 0) {
            dealloc_arena(arena);
            ok = 1;
            break;
        }
        if (ok == 0)
            printf("Invalid command. Please try again.\n");
    }
    return 0;
}
