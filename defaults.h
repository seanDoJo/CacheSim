#ifndef CACHE_DEF
#define CACHE_DEF

unsigned long L1_block_size = 32;
unsigned long L1_cache_size = 8192;
unsigned long L1_assoc = 1;
unsigned long L1_hit_time = 1;
unsigned long L1_miss_time = 1;
unsigned long L1_cost = 0;

unsigned long L2_block_size = 64;
unsigned long L2_cache_size = 32768;
unsigned long L2_assoc = 1;
unsigned long L2_hit_time = 8;
unsigned long L2_miss_time = 10;
unsigned long L2_transfer_time = 10;
unsigned long L2_bus_width = 16;
unsigned long L2_cost = 0;

unsigned long mem_sendaddr = 10;
unsigned long mem_ready = 50;
unsigned long mem_chunktime = 15;
unsigned long mem_chunksize = 8;
unsigned long mem_cost = 75;

#endif
