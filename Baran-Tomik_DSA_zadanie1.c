#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#define FREE 0
#define ALLOC 1

#define FREE_FLAG(size) (size &= ~(1 << 0))								//nastavenie flag na 0
#define ALLOC_FLAG(size) ((size) |= (1 << 0))							//nastavenie flag na 1
#define REAL_SIZE(size) (size & ~(1 << 0))								//size bez 1. bitu 
#define FLAG(size) ((size >> 0) & 1)									//flag zo size (1. bit zo size)

#define BLOCK (char*)total_block + sizeof(BLOCK_HEADER)					// pointer na zaciatok ale posunuty o sizeof(BLOCK_HEADER)
#define LIMIT_ADDRS (char*)total_block + *(char*)total_block			// konecna adresa pola pridelena memrory_init()

void* total_block = NULL;

typedef struct block_header {											//special hlavicka, na zaciatku pola
	unsigned int size;
}BLOCK_HEADER;


typedef struct header {													//hlavicka bloku
	struct header* next;
	struct header* prev;
	unsigned int size;
}HEADER;


//*************************************************************************************************
//inicializuje pamat s ktoru sa bude pracovat, vytvorenie prveho bloku
void memory_init(void* ptr, unsigned int size) {

	if ((int)size % 2) (int)size++;

	if (size <= (sizeof(HEADER) + sizeof(BLOCK_HEADER)) || ptr == NULL) {
		printf("Array must be greater than %d (HEADER + BLOCK_HEADER)\n", (sizeof(HEADER) + sizeof(BLOCK_HEADER)));
		exit(1);
	}

	total_block = ptr;

	BLOCK_HEADER* block_head = (BLOCK_HEADER*)ptr;						//na ulozenie velkosti celeho pola na zaciatok pamate
	block_head->size = (int)size;

	HEADER* head = (char*)ptr + sizeof(BLOCK_HEADER); 					//vytvorenie prveho volneho bloku hned za ^
	head->size = (int)size - sizeof(HEADER) - sizeof(BLOCK_HEADER);
	head->next = NULL;
	head->prev = NULL;
	FREE_FLAG(head->size);

	return;
}

//*********************************************************************************************************************
//urcenuje ci blok je vhodny na alokaciu
char can_allocate(HEADER* free_block, unsigned int size){
	if (FLAG(free_block->size) == ALLOC)
		return 0;

	if (REAL_SIZE(free_block->size) == ((int)size))
		return 1;
	else if (REAL_SIZE(free_block->size) >= ((int)size) + sizeof(HEADER))
		return 1;
	else if ( (REAL_SIZE(free_block->size) < ((int)size) + sizeof(HEADER)) && free_block->prev == NULL )
		return 1;
	else
		return 0;
}

//*********************************************************************************************************************
//najdenie a rozdelenie vhodneho volneho bloku na novy alokovany a novy volny
void* memory_alloc(unsigned int size) {

	if (BLOCK == NULL)
		return NULL;

	if ((int)size % 2) (int)size++;

	HEADER* actuall = BLOCK;
	HEADER* before = NULL;

	while (!can_allocate(actuall, size)) {
		if (actuall->next == NULL) {
			return NULL;
		}
		before = actuall;
		actuall = actuall->next;
	}

	HEADER* new_alloc = actuall;

	//nova hlavicka sa nebude vytvarat pokial je pozadovana velkost rovnaka ako velkost volneho bloku
	if ((REAL_SIZE(new_alloc->size) == (int)size)) {

		new_alloc->size = (int)size;
		ALLOC_FLAG(new_alloc->size);
		return (char*)new_alloc + sizeof(HEADER);
	}
	//pokial je new_alloc prvy a nebolo by miesto na novy volny blok tak sa mu pripise vsetko az po koniec
	else if ((REAL_SIZE(new_alloc->size) < ((int)size) + sizeof(HEADER)) && new_alloc->prev == NULL) {

		ALLOC_FLAG(new_alloc->size);
		return (char*)new_alloc + sizeof(HEADER);
	}
	else {

		int actuall_size = REAL_SIZE(actuall->size);
		(char*)actuall = (char*)actuall + (int)size + sizeof(HEADER);
		HEADER* new_free = actuall;

		new_free->size = actuall_size - (int)size - sizeof(HEADER);		//nova hlavicka pre volny blok
		FREE_FLAG(new_free->size);
		new_free->prev = new_alloc;
		new_free->next = NULL;

		new_alloc->next = actuall;										//aktualizovanie informacii v hlavicke novo-alokovaneho bloku
		new_alloc->size = (int)size;
		ALLOC_FLAG(new_alloc->size);
		if (actuall != BLOCK)
			new_alloc->prev = before;									// ostatne pripady
		else
			new_alloc->prev = NULL;										// pripad ked je na zaciatku


		//BLOCK_HEADER* block_head = (BLOCK_HEADER*)total_block;
		//BLOCK_HEADER* LIMIT = ((char*)block_head + block_head->size);

		//ak sa vytvorila volna hlavicka, ktora ma velkost mensiu ako velkost hlavicky,
		// pripoji sa cely ten blok k predchadzajucemu bloku
		if ( REAL_SIZE(new_free->size) < sizeof(HEADER) &&
			((char*)new_free + (sizeof(HEADER) + REAL_SIZE(new_free->size) + sizeof(HEADER))) > LIMIT_ADDRS) //LIMIT
		{
			new_alloc->next = NULL;
			new_alloc->size = REAL_SIZE(new_alloc->size) + REAL_SIZE(new_free->size) + sizeof(HEADER);
			ALLOC_FLAG(new_alloc->size);
			
			for (int i = sizeof(HEADER); i < (int)(REAL_SIZE(new_alloc->size) + sizeof(HEADER)); i++) {
				*((char*)new_alloc + i) = 0;
			}
		}
		return (char*)new_alloc + sizeof(HEADER);
	}
}

//*********************************************************************************************************************
//zrefaktorovany if 
void next_block_free_condition(HEADER* free_header) {
	free_header->size = REAL_SIZE(free_header->size) + REAL_SIZE(free_header->next->size) + sizeof(HEADER);
	if (free_header->next->next == NULL) {
		free_header->next = NULL;
	}
	else {
		free_header->next = free_header->next->next;
		free_header->next->next->prev = free_header;
	}
	FREE_FLAG(free_header->size);
}

//*********************************************************************************************************************
//uvolnenie bloku a pripadne spojenie dokopy s dalsimi volnymi blokmi
int memory_free(void* valid_ptr) {

	if (valid_ptr == NULL) return 1;

	HEADER* v_ptr_head = (char*)valid_ptr - sizeof(HEADER);

	//ak je ten blok na zaciatku alebo na konci (moze byt jeden velky, ktory je aj aj)
	if ( (v_ptr_head->next == NULL) || (v_ptr_head->prev == NULL) ) {
		if ((v_ptr_head->next == NULL) && (v_ptr_head->prev == NULL)) {		//jediny block v poli
			FREE_FLAG(v_ptr_head->size);
		}
		else if (v_ptr_head->prev == NULL) {								//je to posledny blok v poli
			if (FLAG(v_ptr_head->next->size) == FREE) {
				next_block_free_condition(v_ptr_head);
			}
			else {
				FREE_FLAG(v_ptr_head->size);								//nie je s cim spojit
			}
		}else if (v_ptr_head->next == NULL) {								//je to prvy blok v poli
			if (FLAG(v_ptr_head->prev->size) == FREE) { //toto je asi nerealne
				v_ptr_head->prev->next = NULL;
				v_ptr_head->prev->size = REAL_SIZE(v_ptr_head->prev->size) + REAL_SIZE(v_ptr_head->size) + sizeof(HEADER);
				v_ptr_head = v_ptr_head->prev;
				FREE_FLAG(v_ptr_head->size);
			}
			else {
				FREE_FLAG(v_ptr_head->size);								//nie je s cim spojit
			}
		}
		else {
			return 1;
		}

		for (int i = sizeof(HEADER); i < (int)(REAL_SIZE(v_ptr_head->size) + sizeof(HEADER)); i++) {
			*((char*)v_ptr_head + i) = 0;									//vynulovanie vysledneho volneho bloku
		}
		return 0;
	}
	
	if ((FLAG(v_ptr_head->next->size) == FREE) || (FLAG(v_ptr_head->prev->size) == FREE)) {

		if (FLAG(v_ptr_head->next->size) == FREE) {							//nasledujuci blok je volny
			next_block_free_condition(v_ptr_head);
		}
		if (FLAG(v_ptr_head->prev->size) == FREE) {							//predosli blok je volny
			if (v_ptr_head->next == NULL) {
				v_ptr_head->prev->next = NULL;
			}
			else {
				v_ptr_head->prev->next = v_ptr_head->next;
				v_ptr_head->next->prev = v_ptr_head->prev;
			}
			v_ptr_head->prev->size = REAL_SIZE(v_ptr_head->prev->size) + REAL_SIZE(v_ptr_head->size) + sizeof(HEADER);
			v_ptr_head = v_ptr_head->prev;
			FREE_FLAG(v_ptr_head->size);
		}
	}
	else {
		FREE_FLAG(v_ptr_head->size);										//nie je s cim spojit
	}		
																			
	for (int i = sizeof(HEADER); i < (int)(REAL_SIZE(v_ptr_head->size) + sizeof(HEADER)); i++) {
		*((char*)v_ptr_head + i) = 0;										//vynulovanie vysledneho volneho bloku
	}

	return 0;
}

//*********************************************************************************************************************
//zistenie ci nie je pointer mimo inicializovanej pamate  
int within_bounds(void* ptr) {

	//BLOCK_HEADER* block_head = (BLOCK_HEADER*)total_block;
	//BLOCK_HEADER* LIMIT = ((char*)block_head + block_head->size);

	return (
		(ptr >= (BLOCK + sizeof(HEADER))) &&
		(ptr < LIMIT_ADDRS)
		);
}

//*********************************************************************************************************************
//zistenie ci je pointer platny
int memory_check(void* ptr) {

	if ((ptr == NULL) || !(within_bounds(ptr)))
		return 0;

	HEADER* ptr_head = (char*)ptr - sizeof(HEADER);
	return ((FLAG(ptr_head->size) == ALLOC) ? 1 : 0);
}

//*********************************************************************************************************************
int main() {

	char pole[100];
	memory_init(pole, 100);

	char* hejhou1 = (char*)memory_alloc(14);
	char* hejhou2 = (char*)memory_alloc(14);
	char* hejhou3 = (char*)memory_alloc(14);
	int check1 = memory_check(hejhou1);
	int check2 = memory_check(hejhou2);
	int check3 = memory_check(hejhou3);
	int free1 = memory_free(hejhou1);
	int free2 = memory_free(hejhou2);
	int free3 = memory_free(hejhou3);

	return 0;

}


//main na testovanie
/*
//staci zmeit hodnoty v mem_ar, pole a v rand() urcovani
int main() {


	char* hejhou1;
	int check, free;
	int count[] = { 0, 0 }, alloc_bytes_req = 0, actuall_bytes, rand_n[5], rnc = 0;

	int mem_arr = 10000;
	char pole[10000];

	printf("Test - %d\n", mem_arr);
	printf("Range: (8 - 5000) bytes\nMemory size: %d bytes (%d without block_header)\n", mem_arr, (mem_arr - sizeof(BLOCK_HEADER)));

	memory_init(pole, mem_arr);
	for (int i = 0; i < 5; i++) {
		rand_n[i] = (rand() % (5000 - 8 + 1)) + 8;
	}

	while (1) {
		hejhou1 = (char*)memory_alloc(rand_n[rnc]);

		if (hejhou1 == NULL) break;
		//pointers[count[0]] = hejhou1;
		alloc_bytes_req += rand_n[rnc];
		count[0]++;
		HEADER* temp = ((char*)hejhou1 - sizeof(HEADER));
		count[1] += REAL_SIZE(temp->size);
		rnc++;
		if (rnc == 5) rnc = 0;
	}
	;

	printf("5 Random values: ");
	for (int i = 0; i < 5; i++) {
		printf("%d ", rand_n[i]);
	}
	printf("\n");
	printf("Allocated memory blocks: %d\n", count[0]);
	printf("Requested allocated bytes: %d/%d bytes (%.2f%%)\n", alloc_bytes_req, (mem_arr - sizeof(BLOCK_HEADER)), ((float)alloc_bytes_req / (((float)(mem_arr - sizeof(BLOCK_HEADER))) / 100.0)));

	actuall_bytes = count[1] + (count[0] * sizeof(HEADER));

	printf("Actuall allocated bytes: %d/%d bytes (%.2f%%)\n", count[1], (mem_arr - sizeof(BLOCK_HEADER)), ((float)count[1] / (((float)(mem_arr - sizeof(BLOCK_HEADER))) / 100.0)));
	printf("Actuall allocated bytes with headers: %d/%d bytes (%.2f%%)\n", actuall_bytes, mem_arr, actuall_bytes / ((float)mem_arr / 100.0));

	rnc = 0;
	int ideal_blocks = 0, sub = 0;
	mem_arr = mem_arr - rand_n[rnc];
	while (mem_arr >= 0) {
		ideal_blocks++;
		rnc++;
		if (rnc == 5) rnc = 0;
		mem_arr = mem_arr - rand_n[rnc];
	}
	printf("Ideal allocation: %d blocks\n\n", ideal_blocks);
	printf("Success rate of this algorithm: %.1f%%\n", (float)count[0] / ((float)ideal_blocks / 100.0));

	//for (int i = 0; i < count[0]; i++){
	//	check = memory_check(pointers[i]);
	//	printf("%d. pointer -> check = %d\n",(i + 1) , check);
	//}
	//printf("\n");
	//for (int i = 0; i < count[0]; i++) {
	//	free = memory_free(pointers[i]);
	//	printf("%d. pointer -> free = %d\n", (i + 1), free);
	//}

	return 0;
}
*/
