#ifndef LRU_STACK
#define LRU_STACK

struct c_ent {
	struct c_ent* next;
	struct c_ent* prev;
	struct c_ent* lru_next;
	struct c_ent* lru_prev;
};

struct c_head {
	struct c_ent* start;
	struct c_ent* bottom;
};

#endif
