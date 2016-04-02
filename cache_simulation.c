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
void l1_read_instruction(unsigned long long int address, unsigned int bytesize);
void l1_read_data(unsigned long long int address, unsigned int bytesize);
void l1_write_data(unsigned long long int address, unsigned int bytesize);
void adjust_lru(struct c_ent* ptr, struct c_head* head);
void swap_vc(struct c_ent* l1, struct c_ent* vc, unsigned long long int index);

struct c_head* l1_data_cache;
struct c_head* l1_inst_cache;
struct c_head* l2_cache;
struct c_head l1_i_victim;
struct c_head l1_d_victim;
struct c_head l2_victim;

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
int l1_vc_shift;
int l2_vc_shift;

unsigned long l1_i_hits;
unsigned long l1_i_vc_hits;
unsigned long l1_i_misses;
unsigned long l1_i_total;

unsigned long l1_d_hits;
unsigned long l1_d_vc_hits;
unsigned long l1_d_misses;
unsigned long l1_d_total;

int main(int argc, char* argv[]){
	//initialize cache with arguments
	init(argc, argv);
	l1_i_hits = 0;
	l1_i_vc_hits = 0;
	l1_i_misses = 0;
	l1_d_hits = 0;
	l1_d_vc_hits = 0;
	l1_d_misses = 0;

	char op;
	unsigned long long int address;
	unsigned int bytesize;
	while(scanf("%c %Lx %d\n", &op, &address, &bytesize) == 3){
		switch(op){
			case 'I':
				l1_read_instruction(address, bytesize);
				break;
			case 'R':
				l1_read_data(address, bytesize);
				break;
			case 'W':
				l1_write_data(address, bytesize);
				break;
			default:
				printf("Something went wrong when reading the trace file!\n");
		}
	}
	struct c_ent* ptr;
	int i;
	for(i=0;i<l1_cache_rows;i++){
		ptr = l1_inst_cache[i].start;
		if((*ptr).valid)printf("I:%x | V:%d | D:%d | T:%Lx\n", i,(*ptr).valid, (*ptr).dirty, (*ptr).tag);
	}
	printf("**********\n");
	for(ptr=l1_i_victim.start;ptr!=NULL;ptr=(*ptr).next)printf("%Lx ",(*ptr).tag);
	printf("\n");
	printf("L1i Hits: %lu\n", l1_i_hits);
	printf("L1i Misses: %lu\n", l1_i_misses);
	printf("L1i VC Hits: %lu\n", l1_i_vc_hits);
	l1_i_total = l1_i_hits + l1_i_misses;
	printf("L1i Total Requests: %lu\n", l1_i_total);
	printf("\n");
	printf("L1d Hits: %lu\n", l1_d_hits);
	printf("L1d Misses: %lu\n", l1_d_misses);
	printf("L1d VC Hits: %lu\n", l1_d_vc_hits);
	l1_d_total = l1_d_hits + l1_d_misses;
	printf("L1d Total Requests: %lu\n", l1_d_total);

	
	cache_destroy();
	return 0;
}

void l1_read_instruction(unsigned long long int address, unsigned int bytesize){
	struct c_ent* ptr;
	int i;
	unsigned long long int t_addr, tag, index, vc_tag;
	unsigned int byte_grab, last_ref, cont;
	char l1_full = 1; //variables for making faster eviction decision later
	char l1_victim_full = 1;
	for(i=0;i<bytesize;i++){
		cont = 0;
		t_addr = address+i;
		//tag = (t_addr & l1_tag_mask) >> l1_tag_shift;
		tag = t_addr >> l1_tag_shift;
		byte_grab = (t_addr & 4) >> 2;
		if(i == 0 || byte_grab != last_ref){
			last_ref = byte_grab;
			index = (t_addr & l1_index_mask) >> l1_index_shift;
			vc_tag = t_addr >> l1_vc_shift;
			/*printf("%Lx\n\t",tag);
			int x;
			unsigned long long int t = address;
			for(x=0;x<64;x++){
				int a = t & 1;
				printf("%d ", a);
				t >>= 1;
			}
			printf("\n");*/
		
			for(ptr = l1_inst_cache[index].start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ tag) == 0){
					//adjust lru stack
					l1_i_hits ++;
					adjust_lru(ptr, &l1_inst_cache[index]);
					cont = 1;
					break; //cache hit
				}
				else if(l1_full && !((*ptr).valid)){
					l1_full = 0;
				}
			}
			if(cont)continue;
			//didn't find the data, check victim cache
			for(ptr = l1_i_victim.start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ vc_tag) == 0){
					//bring entry to top of stack	
					adjust_lru(ptr, &l1_i_victim);

					//overwrite with l1 evict data
					swap_vc(l1_inst_cache[index].bottom, ptr, index);
					
					l1_i_vc_hits++;
					l1_i_misses++;
					ptr = l1_inst_cache[index].bottom;
					adjust_lru(ptr, &l1_inst_cache[index]);
					cont = 1;
					break;
				}
				else if(l1_victim_full && !((*ptr).valid)){
					l1_victim_full = 0;
				}
			}
			if(cont)continue;
	
			//didn't find the data in l1, need to check l2 and write new data to l1 spot
			//submit read request to l2
			l1_i_misses++;
			struct c_ent* l1_e;
			struct c_ent* l1_ve;
			unsigned long long int new_ve_tag;
			if(l1_full){
				//need to evict an entry into victim
				l1_e = l1_inst_cache[index].bottom;
				new_ve_tag = (*l1_e).tag;
				new_ve_tag = ((new_ve_tag << l1_tag_shift)>>l1_vc_shift)+index;
				if(l1_victim_full){
					//bring to top of stack	
					l1_ve = l1_i_victim.bottom;
					adjust_lru(l1_ve, &l1_i_victim);

					//overwrite data
					(*l1_ve).tag = new_ve_tag;
					(*l1_ve).dirty = (*l1_e).dirty;
				}
				else{
					//find a free spot for the evicted l1 entry in the victim
					for(l1_ve=l1_i_victim.start;(*l1_ve).valid==1;l1_ve=(*l1_ve).next);
					adjust_lru(l1_ve, &l1_i_victim);

					(*l1_ve).tag = new_ve_tag;
					(*l1_ve).dirty = (*l1_e).dirty;
					(*l1_ve).valid = 1;
				}
				//place tag in evicted l1 entry
				adjust_lru(l1_e, &l1_inst_cache[index]);
				(*l1_e).tag = tag;
				(*l1_e).dirty = 0;
			}
			else{
				//find a free spot for the tag in l1
				for(l1_e=l1_inst_cache[index].start;(*l1_e).valid==1;l1_e=(*l1_e).next);
				adjust_lru(l1_e, &l1_inst_cache[index]);
				(*l1_e).tag = tag;
				(*l1_e).dirty = 0;
				(*l1_e).valid = 1;
			}
		}
		
	}
	
}

void l1_read_data(unsigned long long int address, unsigned int bytesize){
	struct c_ent* ptr;
	int i;
	unsigned long long int t_addr, tag, index, vc_tag;
	unsigned int byte_grab, last_ref, cont;
	char l1_full = 1; //variables for making faster eviction decision later
	char l1_victim_full = 1;
	for(i=0;i<bytesize;i++){
		cont = 0;
		t_addr = address+i;
		tag = (t_addr & l1_tag_mask) >> l1_tag_shift;
		byte_grab = (t_addr & 4) >> 2;
		if(i == 0 || byte_grab != last_ref){
			last_ref = byte_grab;
			index = (t_addr & l1_index_mask) >> l1_index_shift;
			/*printf("%Lx\n\t",tag);
			int x;
			unsigned long long int t = address;
			for(x=0;x<64;x++){
				int a = t & 1;
				printf("%d ", a);
				t >>= 1;
			}
			printf("\n");*/
		
			for(ptr = l1_data_cache[index].start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ tag) == 0){
					//adjust lru stack
					l1_d_hits ++;
					adjust_lru(ptr, &l1_data_cache[index]);
					cont = 1;
					break; //cache hit
				}
				else if(l1_full && !((*ptr).valid)){
					l1_full = 0;
				}
			}
			if(cont)continue;
			vc_tag = t_addr >> l1_vc_shift;
			//didn't find the data, check victim cache
			for(ptr = l1_d_victim.start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ vc_tag) == 0){
					//bring entry to top of stack	
					adjust_lru(ptr, &l1_d_victim);

					//overwrite with l1 evict data
					swap_vc(l1_data_cache[index].bottom, ptr, index);
					
					l1_d_vc_hits++;
					l1_d_misses++;
					ptr = l1_data_cache[index].bottom;
					adjust_lru(ptr, &l1_data_cache[index]);
					cont = 1;
					break;
				}
				else if(l1_victim_full && !((*ptr).valid)){
					l1_victim_full = 0;
				}
			}
			if(cont)continue;
	
			//didn't find the data in l1, need to check l2 and write new data to l1 spot
			//submit read request to l2
			l1_d_misses++;
			struct c_ent* l1_e;
			struct c_ent* l1_ve;
			unsigned long long int new_ve_tag;
			if(l1_full){
				//need to evict an entry into victim
				l1_e = l1_data_cache[index].bottom;
				new_ve_tag = (*l1_e).tag;
				new_ve_tag = ((new_ve_tag << l1_tag_shift) >> l1_vc_shift)+index;
				if(l1_victim_full){
					//bring to top of stack	
					l1_ve = l1_d_victim.bottom;
					adjust_lru(l1_ve, &l1_d_victim);

					//overwrite data
					(*l1_ve).tag = new_ve_tag;
					(*l1_ve).dirty = (*l1_e).dirty;
				}
				else{
					//find a free spot for the evicted l1 entry in the victim
					for(l1_ve=l1_d_victim.start;(*l1_ve).valid==1;l1_ve=(*l1_ve).next);
					adjust_lru(l1_ve, &l1_d_victim);

					(*l1_ve).tag = new_ve_tag;
					(*l1_ve).dirty = (*l1_e).dirty;
					(*l1_ve).valid = 1;
				}
				//place tag in evicted l1 entry
				adjust_lru(l1_e, &l1_data_cache[index]);
				(*l1_e).tag = tag;
				(*l1_e).dirty = 0;
			}
			else{
				//find a free spot for the tag in l1
				for(l1_e=l1_data_cache[index].start;(*l1_e).valid==1;l1_e=(*l1_e).next);
				adjust_lru(l1_e, &l1_data_cache[index]);
				(*l1_e).tag = tag;
				(*l1_e).dirty = 0;
				(*l1_e).valid = 1;
			}
		}
		
	}

}

void l1_write_data(unsigned long long int address, unsigned int bytesize){
	struct c_ent* ptr;
	int i;
	unsigned long long int t_addr, tag, index, vc_tag;
	unsigned int byte_grab, last_ref, cont;
	char l1_full = 1; //variables for making faster eviction decision later
	char l1_victim_full = 1;
	for(i=0;i<bytesize;i++){
		cont = 0;
		t_addr = address+i;
		tag = (t_addr & l1_tag_mask) >> l1_tag_shift;
		byte_grab = (t_addr & 4) >> 2;
		if(i == 0 || byte_grab != last_ref){
			last_ref = byte_grab;
			index = (t_addr & l1_index_mask) >> l1_index_shift;
		
			for(ptr = l1_data_cache[index].start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ tag) == 0){
					//adjust lru stack
					l1_d_hits ++;
					adjust_lru(ptr, &l1_data_cache[index]);
					(*ptr).dirty = 1;
					cont = 1;
					break; //cache hit
				}
				else if(l1_full && !((*ptr).valid)){
					l1_full = 0;
				}
			}
			if(cont)continue;
			vc_tag = t_addr >> l1_vc_shift;
			//didn't find the data, check victim cache
			for(ptr = l1_d_victim.start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ vc_tag) == 0){
					//bring entry to top of stack	
					adjust_lru(ptr, &l1_d_victim);

					//overwrite with l1 evict data
					swap_vc(l1_data_cache[index].bottom, ptr, index);
					
					l1_d_vc_hits++;
					l1_d_misses++;
					ptr = l1_data_cache[index].bottom;
					adjust_lru(ptr, &l1_data_cache[index]);
					(*ptr).dirty = 1;
					cont = 1;
					break;
				}
				else if(l1_victim_full && !((*ptr).valid)){
					l1_victim_full = 0;
				}
			}
			if(cont)continue;
	
			//didn't find the data in l1, need to check l2 and write new data to l1 spot
			//submit read request to l2
			l1_d_misses++;
			struct c_ent* l1_e;
			struct c_ent* l1_ve;
			unsigned long long int new_ve_tag;
			if(l1_full){
				//need to evict an entry into victim
				l1_e = l1_data_cache[index].bottom;
				new_ve_tag = (*l1_e).tag;
				new_ve_tag = ((new_ve_tag << l1_tag_shift)>>l1_vc_shift)+index;
				if(l1_victim_full){
					//bring to top of stack	
					l1_ve = l1_d_victim.bottom;
					adjust_lru(l1_ve, &l1_d_victim);

					//overwrite data
					(*l1_ve).tag = new_ve_tag;
					(*l1_ve).dirty = (*l1_e).dirty;
				}
				else{
					//find a free spot for the evicted l1 entry in the victim
					for(l1_ve=l1_d_victim.start;(*l1_ve).valid==1;l1_ve=(*l1_ve).next);
					adjust_lru(l1_ve, &l1_d_victim);

					(*l1_ve).tag = new_ve_tag;
					(*l1_ve).dirty = (*l1_e).dirty;
					(*l1_ve).valid = 1;
				}
				//place tag in evicted l1 entry
				adjust_lru(l1_e, &l1_data_cache[index]);
				(*l1_e).tag = tag;
				(*l1_e).dirty = 1;
			}
			else{
				//find a free spot for the tag in l1
				for(l1_e=l1_data_cache[index].start;(*l1_e).valid==1;l1_e=(*l1_e).next);
				adjust_lru(l1_e, &l1_data_cache[index]);
				(*l1_e).tag = tag;
				(*l1_e).dirty = 1;
				(*l1_e).valid = 1;
			}
		}
		
	}
}

void adjust_lru(struct c_ent* ptr, struct c_head* head){
	if((*ptr).lru_prev != NULL){					
		if((*ptr).lru_next == NULL)(*head).bottom = (*ptr).lru_prev;
		else (*ptr).lru_next -> lru_prev = (*ptr).lru_prev;
		(*ptr).lru_prev -> lru_next = (*ptr).lru_next;
		(*ptr).lru_next = (*head).lru_start;
		(*ptr).lru_next -> lru_prev = ptr;
		(*ptr).lru_prev = NULL;
		(*head).lru_start = ptr;
	}
}

void swap_vc(struct c_ent* l1, struct c_ent* vc, unsigned long long int index){
	unsigned long long int vc_tag = (*vc).tag;
	unsigned long long int l1_tag = (*l1).tag;
	unsigned long long int new_l1 = ((vc_tag << l1_vc_shift) & l1_tag_mask) >> l1_tag_shift;
	unsigned long long int new_vc = ((l1_tag << l1_tag_shift) >> l1_vc_shift) + index;
	char vc_d = (*vc).dirty;
	(*vc).tag = new_vc;
	(*vc).dirty = (*l1).dirty;
	(*l1).tag = new_l1;
	(*l1).dirty = vc_d;
}

void init(int argc, char* args[]){
	int i, temp;
	unsigned long rows;
	unsigned long columns;
	struct c_ent* ptr;
	struct c_ent* new;
	for(i=1;i<argc;i+=2){
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
		else{
			printf("Unrecognized parameter: %s\n", args[i]);
			exit(1);
		}
	}
	l1_index_mask = 0;
	l2_index_mask = 0;
	l1_index_shift = 0;
	l2_index_shift = 0;
	l1_tag_mask = 0;
	l2_tag_mask = 0;
	l1_tag_shift = 0;
	l2_tag_shift = 0;
	l1_vc_shift = 0;
	l2_vc_shift = 0;
	temp = L1_block_size;

	rows = L1_cache_size / (L1_block_size*L1_assoc);
	l1_cache_rows = rows;
	columns = L1_assoc;
	l1_cache_init(rows, columns);

	ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
	(*ptr).next = NULL;
	(*ptr).lru_next = NULL;
	(*ptr).prev = NULL;
	(*ptr).lru_prev = NULL;
	(*ptr).valid = 0;

	l1_i_victim.start = ptr;
	l1_i_victim.lru_start = ptr;
	for(i=0;i<7;i++){
		new = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*new).prev = ptr;
		(*new).lru_prev = ptr;
		(*new).next = NULL;
		(*new).lru_next = NULL;
		(*new).valid = 0;
		
		(*ptr).next = new;
		(*ptr).lru_next = new;
		
		ptr = new;
	}
	for(ptr=l1_i_victim.start;(*ptr).lru_next!=NULL;ptr=(*ptr).lru_next);	
	l1_i_victim.bottom = ptr;

	ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
	(*ptr).next = NULL;
	(*ptr).lru_next = NULL;
	(*ptr).prev = NULL;
	(*ptr).lru_prev = NULL;
	(*ptr).valid = 0;

	l1_d_victim.start = ptr;
	l1_d_victim.lru_start = ptr;
	for(i=0;i<7;i++){
		new = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*new).prev = ptr;
		(*new).lru_prev = ptr;
		(*new).next = NULL;
		(*new).lru_next = NULL;
		(*new).valid = 0;
		
		(*ptr).next = new;
		(*ptr).lru_next = new;
		
		ptr = new;
	}
	for(ptr=l1_d_victim.start;(*ptr).lru_next!=NULL;ptr=(*ptr).lru_next);
	l1_d_victim.bottom = ptr;

	
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
		l1_vc_shift += 1;
	}

	rows = L2_cache_size / (L2_block_size*L2_assoc);
	l2_cache_rows = rows;
	columns = L2_assoc;
	l2_cache_init(rows, columns); 
	ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
	(*ptr).next = NULL;
	(*ptr).lru_next = NULL;
	(*ptr).prev = NULL;
	(*ptr).lru_prev = NULL;
	(*ptr).valid = 0;

	l2_victim.start = ptr;
	l2_victim.lru_start = ptr;
	for(i=0;i<7;i++){
		new = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*new).prev = ptr;
		(*new).lru_prev = ptr;
		(*new).next = NULL;
		(*new).lru_next = NULL;
		(*new).valid = 0;
		
		(*ptr).next = new;
		(*ptr).lru_next = new;
		
		ptr = new;
	}
	for(ptr=l2_victim.start;(*ptr).lru_next!=NULL;ptr=(*ptr).lru_next);
	l2_victim.bottom = ptr;

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
		l2_vc_shift += 1;
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
		(*ptr).valid = 0;

		l1_data_cache[i].start = ptr;
		l1_data_cache[i].lru_start = ptr;

		for(v=1; v < L1_assoc; v++){
			new = (struct c_ent*)malloc(sizeof(struct c_ent));
			(*new).next = NULL;
			(*new).lru_next = NULL;
			(*new).valid = 0;

			(*ptr).next = new;
			(*ptr).lru_next = new;
			(*new).prev = ptr;
			(*new).lru_prev = ptr;	

			ptr = new;
		}
		for(ptr=l1_data_cache[i].start;(*ptr).lru_next!=NULL;ptr=(*ptr).lru_next);
		l1_data_cache[i].bottom = ptr;
	}

	l1_inst_cache = (struct c_head*)malloc(sizeof(struct c_head)*rows);
	for(i=0;i<rows;i++){
		ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*ptr).next = NULL;
		(*ptr).prev = NULL;
		(*ptr).lru_next = NULL;
		(*ptr).lru_prev = NULL;
		(*ptr).valid = 0;

		l1_inst_cache[i].start = ptr;
		l1_inst_cache[i].lru_start = ptr;

		for(v=1; v < L1_assoc; v++){
			new = (struct c_ent*)malloc(sizeof(struct c_ent));
			(*new).next = NULL;
			(*new).lru_next = NULL;
			(*new).valid = 0;

			(*ptr).next = new;
			(*ptr).lru_next = new;
			(*new).prev = ptr;
			(*new).lru_prev = ptr;	

			ptr = new;
		}
		for(ptr=l1_inst_cache[i].start;(*ptr).lru_next!=NULL;ptr=(*ptr).lru_next);
		l1_inst_cache[i].bottom = ptr;
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
		(*ptr).valid = 0;

		l2_cache[i].start = ptr;
		l2_cache[i].lru_start = ptr;

		for(v=1; v < L2_assoc; v++){
			new = (struct c_ent*)malloc(sizeof(struct c_ent));
			(*new).next = NULL;
			(*new).lru_next = NULL;
			(*new).valid = 0;

			(*ptr).next = new;
			(*ptr).lru_next = new;
			(*new).prev = ptr;
			(*new).lru_prev = ptr;	

			ptr = new;
		}
		for(ptr=l2_cache[i].start;(*ptr).lru_next!=NULL;ptr=(*ptr).lru_next);
		l2_cache[i].bottom = ptr;
	}
}

void cache_destroy(void){
	//function for freeing cache memory
	int i;
	struct c_ent* ptr;
	struct c_ent* holder;

	for(ptr=l1_i_victim.start;(*ptr).next != NULL; ptr=(*ptr).next);
	while(ptr != NULL){
			holder = (*ptr).prev;
			(*ptr).next = NULL;
			(*ptr).lru_next = NULL;
			(*ptr).prev = NULL;
			(*ptr).lru_prev = NULL;
			free(ptr);
			ptr = holder;
	}
	for(ptr=l1_d_victim.start;(*ptr).next != NULL; ptr=(*ptr).next);
	while(ptr != NULL){
			holder = (*ptr).prev;
			(*ptr).next = NULL;
			(*ptr).lru_next = NULL;
			(*ptr).prev = NULL;
			(*ptr).lru_prev = NULL;
			free(ptr);
			ptr = holder;
	}
	for(ptr=l2_victim.start;(*ptr).next != NULL; ptr=(*ptr).next);
	while(ptr != NULL){
			holder = (*ptr).prev;
			(*ptr).next = NULL;
			(*ptr).lru_next = NULL;
			(*ptr).prev = NULL;
			(*ptr).lru_prev = NULL;
			free(ptr);
			ptr = holder;
	}
	
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
