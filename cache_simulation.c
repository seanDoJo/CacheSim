#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "defaults.h"
#include "lru_stack.h"
#include <time.h>

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
void swap_vc_2(struct c_ent* l2, struct c_ent* vc, unsigned long long int index);
void l2_read(unsigned long long int address, unsigned long long int* hitter);
void l2_write(unsigned long long int address, unsigned long long int* hitter);

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

unsigned long long int l1_i_hits;
unsigned long long int l1_i_vc_hits;
unsigned long long int l1_i_misses;
unsigned long long int l1_i_total;
unsigned long long int l1_i_kickouts;
unsigned long long int l1_i_dirty_kick;
unsigned long long int l1_i_transfers;

unsigned long long int l1_d_hits;
unsigned long long int l1_d_vc_hits;
unsigned long long int l1_d_misses;
unsigned long long int l1_d_total;
unsigned long long int l1_d_kickouts;
unsigned long long int l1_d_dirty_kick;
unsigned long long int l1_d_transfers;

unsigned long long int l2_d_hits;
unsigned long long int l2_d_vc_hits;
unsigned long long int l2_d_misses;
unsigned long long int l2_d_total;
unsigned long long int l2_kickouts;
unsigned long long int l2_dirty_kick;
unsigned long long int l2_transfers;

unsigned long long int instRefs;
unsigned long long int reads;
unsigned long long int writes;
unsigned long long int read_cycles;
unsigned long long int write_cycles;
unsigned long long int inst_cycles;
unsigned long long int total_cycles;
unsigned long long int ideal_exec;
unsigned long long int m_exec;

unsigned long l1_l2_tt;
unsigned long l2_m_tt;

time_t timer;
struct tm* tm_info;
int main(int argc, char* argv[]){
	//initialize cache with arguments
	init(argc, argv);
	l1_i_hits = 0;
	l1_i_vc_hits = 0;
	l1_i_misses = 0;
	l1_i_kickouts = 0;
	l1_i_dirty_kick = 0;
	l1_i_transfers = 0;

	l1_d_hits = 0;
	l1_d_vc_hits = 0;
	l1_d_misses = 0;
	l1_d_kickouts = 0;
	l1_d_dirty_kick = 0;
	l1_d_transfers = 0;

	l2_d_hits = 0;
	l2_d_vc_hits = 0;
	l2_d_misses = 0;
	l2_kickouts = 0;
	l2_dirty_kick = 0;
	l2_transfers = 0;

	instRefs = 0;
	reads = 0;
	writes = 0;
	read_cycles = 0;
	write_cycles = 0;
	inst_cycles = 0;
	ideal_exec = 0;
	m_exec = 0;
	l1_l2_tt = L2_transfer_time*(L1_block_size / L2_bus_width);
	l2_m_tt = mem_sendaddr + mem_ready + (mem_chunktime*(L2_block_size/mem_chunksize));

	char op;
	unsigned long long int address;
	unsigned int bytesize;
	char buf1[26];
	time(&timer);
	tm_info = localtime(&timer);
	strftime(buf1, 26, "%Y:%m:%d %H:%M:%S", tm_info);
	puts(buf1);
	while(scanf("%c %Lx %d\n", &op, &address, &bytesize) == 3){
		switch(op){
			case 'I':
				l1_read_instruction(address, bytesize);
				inst_cycles++;//time to execute an instruction
				instRefs++;
				ideal_exec += 2;
				m_exec += 1;
				break;
			case 'R':
				l1_read_data(address, bytesize);
				reads++;
				ideal_exec++;
				break;
			case 'W':
				l1_write_data(address, bytesize);
				writes++;
				ideal_exec++;
				break;
			default:
				printf("Something went wrong when reading the trace file!\n");
		}
	}
	char buf2[26];
	time(&timer);
	tm_info = localtime(&timer);
	strftime(buf2, 26, "%Y:%m:%d %H:%M:%S", tm_info);
	puts(buf2);

	/*struct c_ent* ptr;
	int i;
	for(i=0;i<l2_cache_rows;i++){
		for(ptr = l2_cache[i].lru_start; ptr != NULL; ptr=(*ptr).lru_next){
			if((*ptr).valid)printf("I:%x | V:%d | D:%d | T:%Lx    ", i,(*ptr).valid, (*ptr).dirty, (*ptr).tag);
		}
		printf("\n");
	}
	printf("\n**********\n");
	for(ptr=l2_victim.lru_start;ptr!=NULL;ptr=(*ptr).lru_next){
		unsigned long long int addr = (*ptr).tag << l2_vc_shift;
		printf("%Lx ",addr);
	}
	printf("\n");*/
	unsigned long long int total = reads+writes+instRefs;
	printf("Reads: %llu\t[ %.1f%% ]\n", reads, (((double)reads)/((double)total))*100);
	printf("Writes: %llu\t[ %.1f%% ]\n", writes, (((double)writes)/((double)total))*100);
	printf("Inst. : %llu\t[ %.1f%% ]\n", instRefs, (((double)instRefs)/((double)total))*100);
	printf("Total: %llu\n", total);
	printf("\n");	
	total_cycles = read_cycles + write_cycles + inst_cycles;
	printf("Read Cycles: %llu\t[ %.1f%% ]\n", read_cycles, (((double)read_cycles)/((double)total_cycles))*100);
	printf("Write Cycles: %llu\t[ %.1f%% ]\n", write_cycles, (((double)write_cycles)/((double)total_cycles))*100);
	printf("Inst. Cycles: %llu\t[ %.1f%% ]\n", inst_cycles, (((double)inst_cycles)/((double)total_cycles))*100);
	printf("Total Cycles: %llu\n", total_cycles);
	printf("\n");
	double raw_cpi = ((double)total_cycles) / ((double)instRefs);
	double ideal_cpi = ((double)ideal_exec) / ((double)instRefs);
	double align_cpi = ((double)m_exec) / ((double)instRefs);
	printf("CPI: %.1f\n", raw_cpi);
	printf("Ideal Exec: %llu\n", ideal_exec);
	printf("Ideal CPI: %.1f\n", ideal_cpi);
	printf("Misaligned Exec: %llu\n", m_exec);
	printf("Misaligned CPI: %.1f\n", align_cpi);
	printf("\n");
	printf("L1i Hits: %llu\n", l1_i_hits);
	printf("L1i Misses: %llu\n", l1_i_misses);
	l1_i_total = l1_i_hits + l1_i_misses;
	printf("L1i Total Requests: %llu\n", l1_i_total);
	printf("L1i Kickouts: %llu\n", l1_i_kickouts);
	printf("L1i Dirty Kickouts: %llu\n", l1_i_dirty_kick);
	printf("L1i Transfers: %llu\n", l1_i_transfers);
	printf("L1i VC Hits: %llu\n", l1_i_vc_hits);
	printf("\n");
	printf("L1d Hits: %llu\n", l1_d_hits);
	printf("L1d Misses: %llu\n", l1_d_misses);
	l1_d_total = l1_d_hits + l1_d_misses;
	printf("L1d Total Requests: %llu\n", l1_d_total);
	printf("L1d Kickouts: %llu\n", l1_d_kickouts);
	printf("L1d Dirty Kickouts: %llu\n", l1_d_dirty_kick);
	printf("L1d Transfers: %llu\n", l1_d_transfers);
	printf("L1d VC Hits: %llu\n", l1_d_vc_hits);
	printf("\n");
	printf("L2 Hits: %llu\n", l2_d_hits);
	printf("L2 Misses: %llu\n", l2_d_misses);
	l2_d_total = l2_d_hits + l2_d_misses;
	printf("L2 Total Requests: %llu\n", l2_d_total);
	printf("L2 Kickouts: %llu\n", l2_kickouts);
	printf("L2 Dirty Kickouts: %llu\n", l2_dirty_kick);
	printf("L2 Transfers: %llu\n", l2_transfers);
	printf("L2 VC Hits: %llu\n", l2_d_vc_hits);
	printf("\n");
	unsigned long l_total = L1_cost + L1_cost;
	printf("L1 Cost: (Icache $%lu) + (Dcache $%lu) = $%lu\n", L1_cost, L1_cost, l_total);
	printf("L2 Cost: $%lu\n",L2_cost);
	printf("Mem cost: $%lu\n", mem_cost);
	l_total = L1_cost + L1_cost + L2_cost + mem_cost;
	printf("Total Cost: $%lu\n", l_total);
	printf("\n");

	
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
	last_ref = 0;
	for(i=0;i<bytesize;i++){
		cont = 0;
		t_addr = address+i;
		//tag = (t_addr & l1_tag_mask) >> l1_tag_shift;
		tag = t_addr >> l1_tag_shift;
		byte_grab = (t_addr & 4) >> 2;
		if(i == 0 || byte_grab != last_ref){
			m_exec += L1_hit_time;
			last_ref = byte_grab;
			index = (t_addr & l1_index_mask) >> l1_index_shift;
			vc_tag = t_addr >> l1_vc_shift;
		
			for(ptr = l1_inst_cache[index].start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ tag) == 0){
					//adjust lru stack
					l1_i_hits ++;
					inst_cycles += L1_hit_time;
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
					inst_cycles += L1_miss_time;
					inst_cycles += L1_hit_time;
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
			l2_read(t_addr, &inst_cycles);
			inst_cycles += L1_miss_time;
			inst_cycles += l1_l2_tt;
			l1_i_misses++;
			struct c_ent* l1_e;
			struct c_ent* l1_ve;
			unsigned long long int new_ve_tag;
			l1_i_transfers++;
			if(l1_full){
				//need to evict an entry into victim
				l1_e = l1_inst_cache[index].bottom;
				new_ve_tag = (*l1_e).tag;
				new_ve_tag = ((new_ve_tag << l1_tag_shift)>>l1_vc_shift)+index;
				if(l1_victim_full){
					//bring to top of stack	
					l1_ve = l1_i_victim.bottom;
					adjust_lru(l1_ve, &l1_i_victim);
					if((*l1_ve).dirty)l1_i_dirty_kick++;

					//overwrite data
					(*l1_ve).tag = new_ve_tag;
					(*l1_ve).dirty = (*l1_e).dirty;
					l1_i_kickouts++;
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
			inst_cycles += L1_hit_time;
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
	last_ref = 0;
	for(i=0;i<bytesize;i++){
		cont = 0;
		t_addr = address+i;
		tag = (t_addr & l1_tag_mask) >> l1_tag_shift;
		byte_grab = (t_addr & 4) >> 2;
		if(i == 0 || byte_grab != last_ref){
			m_exec += L1_hit_time;
			last_ref = byte_grab;
			index = (t_addr & l1_index_mask) >> l1_index_shift;
		
			for(ptr = l1_data_cache[index].start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ tag) == 0){
					//adjust lru stack
					l1_d_hits ++;
					read_cycles += L1_hit_time;
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
					read_cycles += L1_miss_time;
					read_cycles += L1_hit_time;
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
			//l2_read(t_addr);
			l1_d_misses++;
			struct c_ent* l1_e;
			struct c_ent* l1_ve;
			l1_d_transfers++;
			read_cycles += L1_miss_time;
			read_cycles += l1_l2_tt;
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
					l1_d_kickouts++;
					if((*l1_ve).dirty){
						unsigned long long int evict = (*l1_ve).tag << l1_vc_shift;
						l2_write(evict, &read_cycles);
						l1_d_dirty_kick++;
						read_cycles += l1_l2_tt;
					}

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
			l2_read(t_addr, &read_cycles);
			read_cycles += L1_hit_time;
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
	last_ref = 0;	
	for(i=0;i<bytesize;i++){
		cont = 0;
		t_addr = address+i;
		tag = (t_addr & l1_tag_mask) >> l1_tag_shift;
		byte_grab = (t_addr & 4) >> 2;
		if(i == 0 || byte_grab != last_ref){
			m_exec += L1_hit_time;
			last_ref = byte_grab;
			index = (t_addr & l1_index_mask) >> l1_index_shift;
		
			for(ptr = l1_data_cache[index].start;ptr!=NULL;ptr=(*ptr).next){
				if((*ptr).valid == 1 && ((*ptr).tag ^ tag) == 0){
					//adjust lru stack
					l1_d_hits ++;
					write_cycles += L1_hit_time;
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
					write_cycles += L1_miss_time;
					write_cycles += L1_hit_time;
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
			//l2_read(t_addr);
			struct c_ent* l1_e;
			struct c_ent* l1_ve;
			unsigned long long int new_ve_tag;
			l1_d_transfers++;
			write_cycles += L1_miss_time;
			write_cycles += l1_l2_tt;
			if(l1_full){
				//need to evict an entry into victim
				l1_e = l1_data_cache[index].bottom;
				new_ve_tag = (*l1_e).tag;
				new_ve_tag = ((new_ve_tag << l1_tag_shift)>>l1_vc_shift)+index;
				if(l1_victim_full){
					//bring to top of stack	
					l1_ve = l1_d_victim.bottom;
					adjust_lru(l1_ve, &l1_d_victim);
					l1_d_kickouts++;
					if((*l1_ve).dirty){
						unsigned long long int evict = (*l1_ve).tag << l1_vc_shift;
						l2_write(evict, &write_cycles);
						l1_d_dirty_kick++;
						write_cycles += l1_l2_tt;
					}

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
			
			l2_read(t_addr, &write_cycles);
			write_cycles += L1_hit_time;

		}
		
	}
}

void l2_read(unsigned long long int address, unsigned long long int* hitter){
	struct c_ent* ptr;
	unsigned long long int t_addr, tag, index, vc_tag;
	char l2_full = 1; //variables for making faster eviction decision later
	char l2_victim_full = 1;
	t_addr = address;
	tag = (t_addr & l2_tag_mask) >> l2_tag_shift;
	index = (t_addr & l2_index_mask) >> l2_index_shift;

	for(ptr = l2_cache[index].start;ptr!=NULL;ptr=(*ptr).next){
		if((*ptr).valid == 1 && ((*ptr).tag ^ tag) == 0){
			//adjust lru stack
			l2_d_hits ++;
			(*hitter) += L2_hit_time;
			adjust_lru(ptr, &l2_cache[index]);
			return; //cache hit
		}
		else if(l2_full && !((*ptr).valid)){
			l2_full = 0;
		}
	}
	vc_tag = t_addr >> l2_vc_shift;
	//didn't find the data, check victim cache
	for(ptr = l2_victim.start;ptr!=NULL;ptr=(*ptr).next){
		if((*ptr).valid == 1 && ((*ptr).tag ^ vc_tag) == 0){
			//bring entry to top of stack	
			adjust_lru(ptr, &l2_victim);

			//overwrite with l2 evict data
			swap_vc_2(l2_cache[index].bottom, ptr, index);
			
			l2_d_vc_hits++;
			l2_d_misses++;
			(*hitter) += L2_miss_time;
			(*hitter) += L2_hit_time;
			ptr = l2_cache[index].bottom;
			adjust_lru(ptr, &l2_cache[index]);
			return;
		}
		else if(l2_victim_full && !((*ptr).valid)){
			l2_victim_full = 0;
		}
	}

	//didn't find the data in l2, need to check mem and write new data to l2 spot
	//submit read request to l2
	l2_d_misses++;
	struct c_ent* l2_e;
	struct c_ent* l2_ve;
	unsigned long long int new_ve_tag;
	l2_transfers++;
	(*hitter) += L2_miss_time;
	(*hitter) += l2_m_tt;
	if(l2_full){
		//need to evict an entry into victim
		l2_e = l2_cache[index].bottom;
		new_ve_tag = (*l2_e).tag;
		new_ve_tag = ((new_ve_tag << l2_tag_shift) >> l2_vc_shift)+index;
		if(l2_victim_full){
			//bring to top of stack	
			l2_ve = l2_victim.bottom;
			adjust_lru(l2_ve, &l2_victim);
			l2_kickouts++;
			if((*l2_ve).dirty){
				//unsigned long long int evict = (*l2_ve).tag << l2_vc_shift;
				//write to memory
				l2_dirty_kick++;
				(*hitter) += l2_m_tt;//time to write to memory
			}

			//overwrite data
			(*l2_ve).tag = new_ve_tag;
			(*l2_ve).dirty = (*l2_e).dirty;
		}
		else{
			//find a free spot for the evicted l2 entry in the victim
			for(l2_ve=l2_victim.start;(*l2_ve).valid==1;l2_ve=(*l2_ve).next);
			adjust_lru(l2_ve, &l2_victim);

			(*l2_ve).tag = new_ve_tag;
			(*l2_ve).dirty = (*l2_e).dirty;
			(*l2_ve).valid = 1;
		}
		//place tag in evicted l2 entry
		adjust_lru(l2_e, &l2_cache[index]);
		(*l2_e).tag = tag;
		(*l2_e).dirty = 0;
	}
	else{
		//find a free spot for the tag in l2
		for(l2_e=l2_cache[index].start;(*l2_e).valid==1;l2_e=(*l2_e).next);
		adjust_lru(l2_e, &l2_cache[index]);
		(*l2_e).tag = tag;
		(*l2_e).dirty = 0;
		(*l2_e).valid = 1;
	}
	(*hitter) += L2_hit_time;

}
void l2_write(unsigned long long int address, unsigned long long int* hitter){
	struct c_ent* ptr;
	unsigned long long int t_addr, tag, index, vc_tag;
	char l2_full = 1; //variables for making faster eviction decision later
	char l2_victim_full = 1;
	t_addr = address;
	tag = (t_addr & l2_tag_mask) >> l2_tag_shift;
	index = (t_addr & l2_index_mask) >> l2_index_shift;

	for(ptr = l2_cache[index].start;ptr!=NULL;ptr=(*ptr).next){
		if((*ptr).valid == 1 && ((*ptr).tag ^ tag) == 0){
			//adjust lru stack
			l2_d_hits ++;
			adjust_lru(ptr, &l2_cache[index]);
			(*hitter) += L2_hit_time;
			(*ptr).dirty = 1;
			return; //cache hit
		}
		else if(l2_full && !((*ptr).valid)){
			l2_full = 0;
		}
	}
	vc_tag = t_addr >> l2_vc_shift;
	//didn't find the data, check victim cache
	for(ptr = l2_victim.start;ptr!=NULL;ptr=(*ptr).next){
		if((*ptr).valid == 1 && ((*ptr).tag ^ vc_tag) == 0){
			//bring entry to top of stack	
			adjust_lru(ptr, &l2_victim);

			//overwrite with l2 evict data
			swap_vc_2(l2_cache[index].bottom, ptr, index);
			
			l2_d_vc_hits++;
			l2_d_misses++;
			(*hitter) += L2_miss_time;
			(*hitter) += L2_hit_time;
			ptr = l2_cache[index].bottom;
			adjust_lru(ptr, &l2_cache[index]);
			(*ptr).dirty = 1;
			return;
		}
		else if(l2_victim_full && !((*ptr).valid)){
			l2_victim_full = 0;
		}
	}

	//didn't find the data in l2, need to pull from memory and write new data to l2 spot
	//submit read request to l2
	l2_d_misses++;
	struct c_ent* l2_e;
	struct c_ent* l2_ve;
	unsigned long long int new_ve_tag;
	l2_transfers++;
	(*hitter) += L2_miss_time;
	(*hitter) += l2_m_tt;
	if(l2_full){
		//need to evict an entry into victim
		l2_e = l2_cache[index].bottom;
		new_ve_tag = (*l2_e).tag;
		new_ve_tag = ((new_ve_tag << l2_tag_shift)>>l2_vc_shift)+index;
		if(l2_victim_full){
			//bring to top of stack	
			l2_ve = l2_victim.bottom;
			adjust_lru(l2_ve, &l2_victim);
			l2_kickouts++;
			if((*l2_ve).dirty){
				//unsigned long long int evict = (*l2_ve).tag << l2_vc_shift;
				//write to memory
				l2_dirty_kick++;
				(*hitter) += l2_m_tt;
			}

			//overwrite data
			(*l2_ve).tag = new_ve_tag;
			(*l2_ve).dirty = (*l2_e).dirty;
		}
		else{
			//find a free spot for the evicted l2 entry in the victim
			for(l2_ve=l2_victim.start;(*l2_ve).valid==1;l2_ve=(*l2_ve).next);
			adjust_lru(l2_ve, &l2_victim);

			(*l2_ve).tag = new_ve_tag;
			(*l2_ve).dirty = (*l2_e).dirty;
			(*l2_ve).valid = 1;
		}
		//place tag in evicted l2 entry
		adjust_lru(l2_e, &l2_cache[index]);
		(*l2_e).tag = tag;
		(*l2_e).dirty = 1;
	}
	else{
		//find a free spot for the tag in l2
		for(l2_e=l2_cache[index].start;(*l2_e).valid==1;l2_e=(*l2_e).next);
		adjust_lru(l2_e, &l2_cache[index]);
		(*l2_e).tag = tag;
		(*l2_e).dirty = 1;
		(*l2_e).valid = 1;
	}
	(*hitter) += L2_hit_time;
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
void swap_vc_2(struct c_ent* l2, struct c_ent* vc, unsigned long long int index){
	unsigned long long int vc_tag = (*vc).tag;
	unsigned long long int l2_tag = (*l2).tag;
	unsigned long long int new_l2 = ((vc_tag << l2_vc_shift) & l2_tag_mask) >> l2_tag_shift;
	unsigned long long int new_vc = ((l2_tag << l2_tag_shift) >> l2_vc_shift) + index;
	char vc_d = (*vc).dirty;
	(*vc).tag = new_vc;
	(*vc).dirty = (*l2).dirty;
	(*l2).tag = new_l2;
	(*l2).dirty = vc_d;
}


void init(int argc, char* args[]){
	int i, temp;
	unsigned long rows;
	unsigned long columns;
	struct c_ent* ptr;
	struct c_ent* new;
	unsigned long mem_mod = 1;
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
		else if(!strcmp(args[i], "m_bw")){
			mem_chunksize *= value;
			mem_mod = value;
		}
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
	unsigned long blks = L1_cache_size >> 12;
	L1_cost += (blks*100);
	unsigned long t = L1_assoc;
	while(t > 1){
		L1_cost += (blks*100);
		t >>= 1;
	}
	blks = L2_cache_size >> 14;
	L2_cost += (blks*50);
	t = L2_assoc;
	while(t > 1){
		L2_cost += (blks*50);
		t >>= 1;
	}
	t = mem_mod;
	while(t > 1){
		mem_cost += 100;
		t >>= 1;
	}

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
