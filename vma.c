#include "vma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

list_t*
dll_create(unsigned int data_size)
{
	list_t* list = malloc(sizeof(*list));
	DIE(!list, "malloc failed\n");
	list->data_size = data_size;
	list->size = 0;
	list->head = NULL;
	return list;
}

unsigned int
get_size(list_t* list)
{
	if (!list)
		return 0;

	return list->size;
}

node_t*
get_nth_node(list_t* list, unsigned int n)
{
	if (!list || !list->size)
		return NULL;
	node_t* node = list->head;
	for (int i = 0; i < n; i++)
		node = node->next;
	return node;
}

void
add_nth_node(list_t* list, unsigned int n, const void* new_data)
{
	node_t* new_node = malloc(sizeof(node_t));
	DIE(!new_node, "New_node malloc failed !");
	new_node->data = malloc(list->data_size);
	DIE(!new_node->data, "New_node->data malloc failed !");
	memcpy(new_node->data, new_data, list->data_size);
	if (list->size == 0)
		n = 0;
	if (n == 0) {
		new_node->prev = NULL;
		new_node->next = list->head;
		if (list->head)
			list->head->prev = new_node;
		list->head = new_node;
		list->size++;
		return;
	}
	else if (n >= list->size) {
		new_node->next = NULL;
		node_t* curr_node = list->head;
		while (curr_node->next)
			curr_node = curr_node->next;
		curr_node->next = new_node;
		new_node->prev = curr_node;
		list->size++;
		return;
	}
	node_t* curr_node = list->head;
	while (--n)
		curr_node = curr_node->next;
	node_t* next_node = curr_node->next;
	new_node->next = curr_node->next;
	new_node->prev = curr_node;
	curr_node->next = new_node;
	next_node->prev = new_node;
	list->size++;
}

node_t*
remove_nth_node(list_t* list, unsigned int n)
{
	if (list->size == 0)
		return NULL;
	if (n >= list->size - 1)
		n = list->size - 1;
	if (n == 0) {
		node_t* removed = list->head;
		if (list->size > 1) {
			list->head = list->head->next;
			list->head->prev = NULL;
		}
		else {
			list->head = NULL;
		}
		list->size--;
		return removed;
	}
	node_t* current = list->head;
	while (n--)
		current = current->next;
	node_t* removed = current;
	if (!current->next) {
		node_t* prev_node = current->prev;
		prev_node->next = NULL;
		list->size--;
		return removed;
	}
	node_t* prev_node = current->prev;
	node_t* next_node = current->next;
	prev_node->next = current->next;
	next_node->prev = current->prev;
	list->size--;
	return removed;
}

void
dll_free(list_t** pp_list)
{
	if ((*pp_list)->size == 0) {
		free((*pp_list));
		return;
	}
	node_t* prev_node = NULL;
	node_t* curr_node = (*pp_list)->head;
	while (curr_node) {
		if (prev_node) {
			free(prev_node->data);
			free(prev_node);
		}
		prev_node = curr_node;
		curr_node = curr_node->next;
	}
	if (prev_node) {
		free(prev_node->data);
		free(prev_node);
	}
	free((*pp_list));
	*pp_list = NULL;
}

arena_t* alloc_arena(const uint64_t size)
{
	arena_t* arena = malloc(sizeof(arena_t));
	DIE(!arena, "malloc failed\n");
	arena->arena_size = size;
	// Initializam lista de alocari cu NULL
	arena->alloc_list = dll_create(sizeof(block_t));
	return arena;
}

void dealloc_arena(arena_t* arena)
{
	if (!arena) {
		return;
	}
	list_t* block_list = arena->alloc_list;
	node_t* current = block_list->head;

	// Eliberam toate listele de miniblocurile
	while (current != NULL) {
		block_t* block = current->data;
		list_t* miniblock_list = block->miniblock_list;
		node_t* current_node = miniblock_list->head;
		while (current_node != NULL) {
			miniblock_t* miniblock = (miniblock_t*)current_node->data;
			if (miniblock->rw_buffer)
				free(miniblock->rw_buffer);
			current_node = current_node->next;
		}
		dll_free(&miniblock_list);
		current = current->next;
	}

	//Eliberam lista de blocuri
	dll_free(&block_list);

	// Eliberam structura arena
	free(arena);
}

void alloc_block(arena_t* arena, const uint64_t address, const uint64_t size)
{
	if (address >= arena->arena_size) {
		printf("The allocated address is outside the size of arena\n");
		return;
	}

	if (size + address > arena->arena_size) {
		printf("The end address is past the size of the arena\n");
		return;
	}

	// Verificam daca zona a fost deja alocata
	node_t* current_block = arena->alloc_list->head;
	while (current_block != NULL) {
		block_t* block = current_block->data;
		if (address < block->start_address &&
			address + size > block->start_address) {
			printf("This zone was already allocated.\n");
			return;
		}
		if (address < block->start_address + block->size && address >= block->start_address) {
			printf("This zone was already allocated.\n");
			return;
		}
		current_block = current_block->next;
	}

	// Verificam daca exista zone adiacente
	current_block = arena->alloc_list->head;
	node_t* prev_node = NULL, * next_node = NULL;
	int prev_ind = 0, next_ind = 0, curr_ind = 0;
	while (current_block != NULL) {
		block_t* block = current_block->data;
		if (block->start_address + block->size == address) {
			prev_node = current_block;
			prev_ind = curr_ind;
		}
		if (address + size == block->start_address) {
			next_node = current_block;
			next_ind = curr_ind;
		}
		current_block = current_block->next;
		curr_ind++;
	}

	// Alocam noul bloc

	block_t new_block;
	new_block.start_address = address;
	new_block.size = size;
	new_block.miniblock_list = dll_create(sizeof(miniblock_t));
	miniblock_t miniblock;
	miniblock.start_address = address;
	miniblock.size = size;
	miniblock.rw_buffer = malloc(size);
	add_nth_node(new_block.miniblock_list, 0, &miniblock);
	// Inseram noul bloc
	if (prev_node == NULL && next_node == NULL) {
		// Daca noul bloc nu are zone adiacente il adaugam
		add_nth_node(arena->alloc_list, arena->alloc_list->size, &new_block);

	}
	else if (prev_node == NULL && next_node != NULL) {
		// Blocul nou este adiacent cu cel din dreapta
		block_t* next_block = (block_t*)next_node->data;
		//next_block->start_address = address;
		//next_block->size += size;
		node_t* removed = remove_nth_node(arena->alloc_list, next_ind);
		new_block.miniblock_list->head->next = ((block_t*)removed->data)->miniblock_list->head;
		((block_t*)removed->data)->miniblock_list->head->prev = new_block.miniblock_list->head;
		new_block.miniblock_list->size += ((block_t*)removed->data)->miniblock_list->size;
		new_block.size += ((block_t*)removed->data)->size;
		free(((block_t*)removed->data)->miniblock_list);
		free((block_t*)removed->data);
		free(removed);
		add_nth_node(arena->alloc_list, next_ind, &new_block);
	}
	else if (next_node == NULL && prev_node != NULL) {
		// Blocul este adiacent cu cel din stanga
		block_t* prev_block = (block_t*)prev_node->data;
		prev_block->size += size;
		node_t* miniblock_node = prev_block->miniblock_list->head;
		while (miniblock_node->next != NULL) {
			miniblock_node = miniblock_node->next;
		}
		miniblock_node->next = new_block.miniblock_list->head;
		new_block.miniblock_list->head->prev = miniblock_node;
		prev_block->miniblock_list->size++;
		//free new block
		free(new_block.miniblock_list);
	}
	else {
		// Blocul este adiacent cu ambele blocuri
		block_t* next_block = (block_t*)next_node->data;
		block_t* prev_block = (block_t*)prev_node->data;
		prev_block->size += size + next_block->size;
		//prev_block->size += size;
		node_t* miniblock_node = prev_block->miniblock_list->head;
		while (miniblock_node->next != NULL) {
			miniblock_node = miniblock_node->next;
		}
		miniblock_node->next = new_block.miniblock_list->head;
		new_block.miniblock_list->head->prev = miniblock_node;
		prev_block->miniblock_list->size++;


		node_t* removed = remove_nth_node(arena->alloc_list, next_ind);
		new_block.miniblock_list->head->next = ((block_t*)removed->data)->miniblock_list->head;
		((block_t*)removed->data)->miniblock_list->head->prev = new_block.miniblock_list->head;
		//new_block.miniblock_list->size += ((block_t *)removed->data)->miniblock_list->size;
		prev_block->miniblock_list->size += ((block_t*)removed->data)->miniblock_list->size;
		free(new_block.miniblock_list);
		free(((block_t*)removed->data)->miniblock_list);
		free((block_t*)removed->data);
		free(removed);
	}
}

block_t* find_block(list_t* block_list, uint64_t start_address) {
	node_t* current = block_list->head;
	while (current != NULL) {
		block_t* block = (block_t*)current->data;
		if (block->start_address >= start_address && block->start_address + block->size - 1 >= start_address) {
			return block;
		}
		current = current->next;
	}
	return NULL;
}

miniblock_t* find_miniblock(block_t* block, uint64_t address) {
	node_t* current = block->miniblock_list->head;
	int index = -1;
	while (current != NULL) {
		miniblock_t* miniblock = (miniblock_t*)current->data;
		index++;
		if (address == miniblock->start_address) {
			return miniblock;
		}
		current = current->next;
	}
	return NULL;
}

void free_block(arena_t* arena, const uint64_t address)
{
	list_t* block_list = arena->alloc_list;
	// Căutăm blocul care conține adresa
	block_list = arena->alloc_list;
	node_t* current_node = block_list->head;
	int min_ind = -1;
	int bl_ind = -1;
	int index1 = -1;
	int count = 0, nr_miniblocks = 0;
	block_t* found_block = NULL;
	node_t* found_miniblock = NULL;
	node_t* found_node = NULL;
	while (current_node != NULL) {
		index1++;
		block_t* block = (block_t*)current_node->data;
		if (block->start_address <= address && block->start_address + block->size - 1 >= address) {
			node_t* current = block->miniblock_list->head;
			int index2 = -1;
			while (current != NULL) {
				miniblock_t* miniblock = (miniblock_t*)current->data;
				index2++;
				count++;
				if (address == miniblock->start_address) {
					min_ind = index2;
					bl_ind = index1;
					found_block = block;
					found_node = current_node;
					found_miniblock = current;
					nr_miniblocks = count;
				}
				current = current->next;
			}
		}
		current_node = current_node->next;
	}
	if (min_ind == -1) {
		printf("Invalid address for free.\n");
		return;
	}
	list_t* miniblock_list = found_block->miniblock_list;
	if (found_miniblock->prev == NULL || found_miniblock->next == NULL) {
		//Eliminam de la inceput sau de la final
		node_t* removed = remove_nth_node(miniblock_list, min_ind);
		if (removed->prev == NULL && removed->next) {
			found_block->start_address = ((miniblock_t*)removed->next->data)->start_address;
		}
		found_block->size -= ((miniblock_t*)removed->data)->size;
		free(((miniblock_t*)removed->data)->rw_buffer);
		free(removed->data);
		free(removed);
		if (found_block->miniblock_list->size == 0) {
			dll_free(&found_block->miniblock_list);
			removed = remove_nth_node(block_list, bl_ind);
			free((block_t*)removed->data);
			free(removed);
		}
	}
	else {
		// Împarte blocul în două
		block_t* first_block = (block_t*)malloc(sizeof(block_t));
		first_block->start_address = found_block->start_address;
		first_block->size = ((miniblock_t*)found_miniblock->data)->start_address - found_block->start_address;
		first_block->miniblock_list = dll_create(sizeof(miniblock_t));
		first_block->miniblock_list->size = nr_miniblocks;
		first_block->miniblock_list->head = found_block->miniblock_list->head;
		block_t* second_block = (block_t*)malloc(sizeof(block_t));
		//second_block->start_address = found_block->start_address + first_block->size + ((miniblock_t *)found_miniblock->data)->size;
		second_block->start_address = ((miniblock_t*)found_miniblock->next->data)->start_address;
		//second_block->size = found_block->size + found_block->start_address - first_block->size - ((miniblock_t *)found_miniblock->data)->size;
		second_block->size = found_block->size - first_block->size - ((miniblock_t*)found_miniblock->data)->size;
		second_block->miniblock_list = dll_create(sizeof(miniblock_t));
		second_block->miniblock_list->size = ((miniblock_t*)found_miniblock->data)->size - nr_miniblocks - 1;
		second_block->miniblock_list->head = found_miniblock->next;
		node_t* removed = remove_nth_node(miniblock_list, min_ind);
		second_block->miniblock_list->head->prev = NULL;
		node_t* last = get_nth_node(miniblock_list, nr_miniblocks - 1);
		last->next = NULL;
		add_nth_node(arena->alloc_list, bl_ind + 1, &second_block);
		found_node->data = first_block;
		free(((miniblock_t*)removed->data)->rw_buffer);
		free(removed->data);
		free(removed);
	}
}

void read(arena_t* arena, uint64_t address, uint64_t size)
{
	list_t* block_list = NULL;
	uint8_t* data = malloc((size + 1) * sizeof(char));

	// Căutăm blocul care conține adresa
	uint64_t size_copy = size;
	block_list = arena->alloc_list;
	node_t* current_node = block_list->head;
	int min_ind = -1;
	//int bl_ind = -1;
	int index1 = -1;
	//block_t *found_block = NULL;
	node_t* found_miniblock = NULL;
	while (current_node != NULL) {
		index1++;
		block_t* block = (block_t*)current_node->data;
		if (block->start_address <= address && block->start_address + block->size - 1 >= address) {
			if (block->size + block->start_address - address < size) {
				printf("Warning: size was bigger than the block size. Reading %lu characters.\n", block->size + block->start_address - address);
				size_copy = block->size + block->start_address - address;
			}
			node_t* current = block->miniblock_list->head;
			int index2 = -1;
			while (current != NULL) {
				miniblock_t* miniblock = (miniblock_t*)current->data;
				index2++;
				if (address >= miniblock->start_address) {
					min_ind = index2;
					//bl_ind = index1;
					//found_block = block;
					found_miniblock = current;
					break;
				}
				current = current->next;
			}
		}
		current_node = current_node->next;
	}
	if (min_ind == -1) {
		printf("Invalid address for read.\n");
		return;
	}
	if (((miniblock_t*)found_miniblock->data)->size + ((miniblock_t*)found_miniblock->data)->start_address - address >= size_copy) {
		memcpy(data, ((miniblock_t*)found_miniblock->data)->rw_buffer, size_copy);
		for (int i = 0; i < size; i++) {
			printf("%c", data[i]);
		}
		free(data);
		printf("\n");
		return;
	}
	void* dest = (char*)((miniblock_t*)found_miniblock->data)->rw_buffer + (address - ((miniblock_t*)found_miniblock->data)->start_address);
	memcpy(data, dest, ((miniblock_t*)found_miniblock->data)->size + ((miniblock_t*)found_miniblock->data)->start_address - address);

	int count = 0;
	for (node_t* node = found_miniblock; node != NULL && size_copy > 0; node = node->next) {
		miniblock_t* miniblock_data = (miniblock_t*)node->data;
		if (miniblock_data->size < size_copy) {
			memcpy(data + count, miniblock_data->rw_buffer, miniblock_data->size);
			count += miniblock_data->size;
			size_copy -= miniblock_data->size;
		}
		else {
			memcpy(data + count, miniblock_data->rw_buffer, size_copy);
		}
	}
	for (int i = 0; i < size; i++) {
		printf("%c", data[i]);
	}
	free(data);
	printf("\n");

	/*memcpy(data, miniblock->rw_buffer, size);
	//data = (uint8_t*)miniblock->rw_buffer + (address - miniblock->start_address);
	for(int i = 0 ; i < size ; i++) {
		printf("%c", data[i]);
	}
	free(data);
	printf("\n");*/
}

void write(arena_t* arena, const uint64_t address, const uint64_t size, int8_t* data)
{
	// Căutăm blocul care conține adresa
	list_t* block_list = NULL;
	uint64_t size_copy = size;

	block_list = arena->alloc_list;
	node_t* current_node = block_list->head;
	int min_ind = -1;
	//int bl_ind = -1;
	int index1 = -1;
	//block_t *found_block = NULL;
	node_t* found_miniblock = NULL;
	while (current_node != NULL) {
		index1++;
		block_t* block = (block_t*)current_node->data;
		if (block->start_address <= address && block->start_address + block->size - 1 >= address) {
			if (block->size + block->start_address - address < size) {
				printf("Warning: size was bigger than the block size. Writing %lu characters.\n", block->size + block->start_address - address);
				size_copy = block->size + block->start_address - address;
			}
			node_t* current = block->miniblock_list->head;
			int index2 = -1;
			while (current != NULL) {
				miniblock_t* miniblock = (miniblock_t*)current->data;
				index2++;
				if (address >= miniblock->start_address) {
					min_ind = index2;
					//bl_ind = index1;
					//found_block = block;
					found_miniblock = current;
					break;
				}
				current = current->next;
			}
		}
		current_node = current_node->next;
	}
	if (min_ind == -1) {
		printf("Invalid address for write.\n");
		return;
	}
	if (((miniblock_t*)found_miniblock->data)->size + ((miniblock_t*)found_miniblock->data)->start_address - address >= size_copy) {
		memcpy(((miniblock_t*)found_miniblock->data)->rw_buffer, data, size_copy);
		return;
	}
	void* dest = (char*)((miniblock_t*)found_miniblock->data)->rw_buffer + (address - ((miniblock_t*)found_miniblock->data)->start_address);
	memcpy(dest, data, ((miniblock_t*)found_miniblock->data)->size + ((miniblock_t*)found_miniblock->data)->start_address - address);

	int count = 0;
	for (node_t* node = found_miniblock; node != NULL && size_copy > 0; node = node->next) {
		miniblock_t* miniblock_data = (miniblock_t*)node->data;
		if (miniblock_data->size < size_copy) {
			memcpy(miniblock_data->rw_buffer, data + count, miniblock_data->size);
			count += miniblock_data->size;
			size_copy -= miniblock_data->size;
		}
		else {
			memcpy(miniblock_data->rw_buffer, data + count, size_copy);
		}
	}
	/*miniblock_t* miniblock = (miniblock_t*)found_miniblock->data;
	size_t available_size = miniblock->size - (address - miniblock->start_address);
	if (size > available_size) {
		printf("Warning: size was bigger than the block size. Writing %lu characters.\n", available_size);
		memcpy(miniblock->rw_buffer, data, available_size);
	}

	// Scriem datele în buffer
	void* dest = (char*)miniblock->rw_buffer + (address - miniblock->start_address);
	memcpy(dest, data, size);*/

}

void pmap(const arena_t* arena)
{
	list_t* block_list = arena->alloc_list;
	list_t* miniblock_list = NULL;
	node_t* current = block_list->head;
	size_t total_memory = 0;
	unsigned int nr_miniblocks = 0;
	while (current != NULL) {
		block_t* block = (block_t*)current->data;
		total_memory += block->size;
		miniblock_list = block->miniblock_list;
		nr_miniblocks += get_size(miniblock_list);
		current = current->next;
	}
	printf("Total memory: 0x%lX bytes\n", arena->arena_size);
	printf("Free memory: 0x%lX bytes\n", arena->arena_size - total_memory);
	printf("Number of allocated blocks: %u\n", get_size(block_list));
	printf("Number of allocated miniblocks: %u\n", nr_miniblocks);
	if (arena->alloc_list->head == 0) {
		return;
	}
	printf("\n");
	current = block_list->head;
	for (int i = 0; i < block_list->size; i++) {
		block_t* block = (block_t*)current->data;
		printf("Block %d begin\n", i + 1);
		printf("Zone: 0x%lX - 0x%lX\n", block->start_address, block->start_address + block->size);
		miniblock_list = block->miniblock_list;
		node_t* current_node = miniblock_list->head;
		for (int j = 0; j < miniblock_list->size; j++) {
			printf("Miniblock %d:", j + 1);
			miniblock_t* miniblock = (miniblock_t*)current_node->data;
			printf("\t\t0x%lX\t\t-\t\t0x%lX\t\t", miniblock->start_address, miniblock->start_address + miniblock->size);
			printf("| RW-\n");
			current_node = current_node->next;
		}
		printf("Block %d end\n", i + 1);
		current = current->next;
		if (current == 0) {
			return;
		}
		printf("\n");
	}
}
