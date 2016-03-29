#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "defaults.h"
#include "lru_stack.h"

//function declarations
void l1_cache_init(unsigned long rows, unsigned long columns);
void l2_cache_init(unsigned long rows, unsigned long columns);
void cache_destroy(void);
void init(int argc, char*args[]);

//unsigned long long int** l1_data_cache;
//unsigned long long int** l1_inst_cache;
//unsigned long long int** l2_cache;

struct c_head* l1_data_cache;
struct c_head* l1_inst_cache;
struct c_head* l2_cache;

unsigned int l2_cache_rows;
unsigned int l1_cache_rows;
unsigned long long int l1_index_mask;
unsigned long long int l2_index_mask;
int l1_index_shift;
int l2_index_shift;
unsigned long long int l1_tag_mask;
unsigned long long int l2_tag_mask;
int l1_tag_shift;
int l2_tag_shift;
FILE* fp;

int main(int argc, char* argv[]){
	//initialize cache with arguments
	if(argc < 2){
		printf("Must pass in a filename and optional parameters!");
		exit(1);
	}
	//fp = fopen(argv[1], "r");
	init(argc, argv);
	printf("%d\n",l1_index_shift);
	printf("%d\n",l1_tag_shift);
	cache_destroy();
	exit(0);

	char op;
	unsigned long long int address;
	unsigned int bytesize;
	while(fscanf(fp, "%c %Lx %d\n", &op, &address, &bytesize) == 3){
		unsigned long long int index = (address & l1_index_mask) >> l1_index_shift;
		unsigned long long int tag = (address & l1_tag_mask) >> l1_tag_shift;
	}
	
	fclose(fp);
	cache_destroy();
	return 0;
}

void init(int argc, char* args[]){
	int i, temp;
	unsigned long rows;
	unsigned long columns;
	for(i=2;i<argc;i+=2){
		unsigned long value = strtoul(args[i+1], NULL, 10);
		if(value < 0){
			printf("Couldn't convert number! Exiting.");
			exit(1);
		}
		if(!strcmp(args[i], "l1_bs"))L1_block_size = value;
		else if(!strcmp(args[i], "l1_cs"))L1_cache_size = value;
		else if(!strcmp(args[i], "l1_a"))L1_assoc = value;
		else if(!strcmp(args[i], "l1_hit"))L1_hit_time = value;
		else if(!strcmp(args[i], "l1_miss"))L1_miss_time = value;
		else if(!strcmp(args[i], "l2_bs"))L2_block_size = value;
		else if(!strcmp(args[i], "l2_cs"))L2_cache_size = value;
		else if(!strcmp(args[i], "l2_a"))L2_assoc = value;
		else if(!strcmp(args[i], "l2_hit"))L2_hit_time = value;
		else if(!strcmp(args[i], "l2_miss"))L2_miss_time = value;
		else if(!strcmp(args[i], "l2_t"))L2_transfer_time = value;
		else if(!strcmp(args[i], "l1_bus"))L2_bus_width = value;
		else printf("Unrecognized parameter: %s\n", args[i]);
	}
	l1_index_mask = 0;
	l2_index_mask = 0;
	l1_index_shift = 0;
	l2_index_shift = 0;
	l1_tag_mask = 0;
	l2_tag_mask = 0;
	l1_tag_shift = 0;
	l2_tag_shift = 0;
	temp = L1_block_size;

	rows = L1_cache_size / (L1_block_size*L1_assoc);
	l1_cache_rows = rows;
	columns = L1_assoc;
	l1_cache_init(rows, columns);
	
	while(rows > 1){
		l1_index_mask <<= 1;
		l1_index_mask |= 1;
		rows >>= 1;
		l1_tag_shift += 1;
	}
	l1_tag_mask = ~(l1_index_mask);	
	while(temp > 1){
		l1_index_mask <<= 1;
		l1_tag_mask <<= 1;
		temp >>= 1;
		l1_index_shift +=1;
		l1_tag_shift += 1;
	}

	rows = L2_cache_size / (L2_block_size*L2_assoc);
	l2_cache_rows = rows;
	columns = L2_assoc;
	l2_cache_init(rows, columns); 
	
	temp = L2_block_size;
	while(rows > 1){
		l2_index_mask <<= 1;
		l2_index_mask |= 1;
		rows >>= 1;
		l2_tag_shift += 1;
	}
	l2_tag_mask = ~(l2_index_mask);	
	while(temp > 1){
		l2_index_mask <<= 1;
		l2_tag_mask <<= 1;
		temp >>= 1;
		l2_index_shift +=1;
		l2_tag_shift += 1;
	}

}

void l1_cache_init(unsigned long rows, unsigned long columns){
	int i, v;
	l1_data_cache = (struct c_head*)malloc(sizeof(struct c_head)*rows);
	struct c_ent* ptr;
	struct c_ent* new;
	for(i=0;i<rows;i++){
		ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*ptr).next = NULL;
		(*ptr).prev = NULL;
		(*ptr).lru_next = NULL;
		(*ptr).lru_prev = NULL;

		l1_data_cache[i].start = ptr;
		l1_data_cache[i].bottom = ptr;

		for(v=1; v < L1_assoc; v++){
			new = (struct c_ent*)malloc(sizeof(struct c_ent));
			(*new).next = NULL;
			(*new).lru_next = NULL;

			(*ptr).next = new;
			(*ptr).lru_next = new;
			(*new).prev = ptr;
			(*new).lru_prev = ptr;	

			ptr = new;
		}
	}

	l1_inst_cache = (struct c_head*)malloc(sizeof(struct c_head)*rows);
	for(i=0;i<rows;i++){
		ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*ptr).next = NULL;
		(*ptr).prev = NULL;
		(*ptr).lru_next = NULL;
		(*ptr).lru_prev = NULL;

		l1_inst_cache[i].start = ptr;
		l1_inst_cache[i].bottom = ptr;

		for(v=1; v < L1_assoc; v++){
			new = (struct c_ent*)malloc(sizeof(struct c_ent));
			(*new).next = NULL;
			(*new).lru_next = NULL;

			(*ptr).next = new;
			(*ptr).lru_next = new;
			(*new).prev = ptr;
			(*new).lru_prev = ptr;	

			ptr = new;
		}
	}
	ptr = NULL;
	new = NULL;
}

void l2_cache_init(unsigned long rows, unsigned long columns){
	int i, v;
	l2_cache = (struct c_head*)malloc(sizeof(struct c_head)*rows);
	struct c_ent* ptr;
	struct c_ent* new;
	for(i=0;i<rows;i++){
		ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*ptr).next = NULL;
		(*ptr).prev = NULL;
		(*ptr).lru_next = NULL;
		(*ptr).lru_prev = NULL;

		l2_cache[i].start = ptr;
		l2_cache[i].bottom = ptr;

		for(v=1; v < L2_assoc; v++){
			new = (struct c_ent*)malloc(sizeof(struct c_ent));
			(*new).next = NULL;
			(*new).lru_next = NULL;

			(*ptr).next = new;
			(*ptr).lru_next = new;
			(*new).prev = ptr;
			(*new).lru_prev = ptr;	

			ptr = new;
		}
	}
}

void cache_destroy(void){
	//function for freeing cache memory
	int i;
	struct c_ent* ptr;
	struct c_ent* holder;
	for(i=0;i<l1_cache_rows;i++){
		for(ptr=l1_data_cache[i].start;(*ptr).next != NULL; ptr = (*ptr).next);
		while(ptr != NULL){
			holder = (*ptr).prev;
			(*ptr).next = NULL;
			(*ptr).lru_next = NULL;
			(*ptr).prev = NULL;
			(*ptr).lru_prev = NULL;
			free(ptr);
			ptr = holder;
		}
		for(ptr=l1_inst_cache[i].start;(*ptr).next != NULL; ptr = (*ptr).next);
		while(ptr != NULL){
			holder = (*ptr).prev;
			(*ptr).next = NULL;
			(*ptr).lru_next = NULL;
			(*ptr).prev = NULL;
			(*ptr).lru_prev = NULL;
			free(ptr);
			ptr = holder;
		}
		ptr = NULL;
		holder = NULL;
	}
	free(l1_data_cache);
	free(l1_inst_cache);

	for(i=0;i<l2_cache_rows;i++){
		for(ptr=l2_cache[i].start;(*ptr).next != NULL; ptr = (*ptr).next);
		while(ptr != NULL){
			holder = (*ptr).prev;
			(*ptr).next = NULL;
			(*ptr).lru_next = NULL;
			(*ptr).prev = NULL;
			(*ptr).lru_prev = NULL;
			free(ptr);
			ptr = holder;
		}
		ptr = NULL;
		holder = NULL;
	}
	free(l2_cache);
}
