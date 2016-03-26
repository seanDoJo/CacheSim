#ifndef LRU_STACK
#define LRU_STACK

//need to know how dirty bit affects eviction selection

//need to know if we need a stack for each level of associativity -- I believe the stack is only relevant to the associative caches

// have an l1_lru, l2_lru, etc...?

struct c_ent {
	char dirty;
	unsigned long long int tag;
	struct c_ent* next;
	struct c_ent* prev;
}

unsigned int elements;
struct c_ent* stack; //should most likely be a 2d array
struct c_ent* bottom; //would want this for each lru stack

void lru_init(unsigned long cacheSize);

void lru_init(unsigned long cacheSize){
	elements = 0; // would want this for each lru stack being implemented
}

void lru_push(unsigned long long int tag){ //add an index parameter?
	struct c_ent* ptr;
	if(elements == 0){
		ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*ptr).dirty = 0;
		(*ptr).tag = tag;
		(*ptr).next = NULL;
		(*ptr).prev = NULL;
		stack = ptr;
		bottom = ptr;
		elements++;
	}
	else{
		ptr = stack;
		while(ptr != NULL){
			if(tag ^ (*ptr).tag == 0){
				if((*ptr).prev == NULL)return
				(*ptr).prev.next = (*ptr).next;
				if((*ptr).next != NULL)(*ptr).next.prev = (*ptr).prev;
				if((*ptr).next == NULL){
					bottom = (*ptr).prev;
				}
				(*ptr).prev = NULL;

				(*ptr).next = stack;
				(*stack).prev = ptr;
				stack = ptr;
				return;
			}
			else{
				ptr = (*ptr).next;
			}
		}
		ptr = (struct c_ent*)malloc(sizeof(struct c_ent));
		(*ptr).dirty = 0;
		(*ptr).tag = tag;
		(*ptr).prev = NULL;
		(*ptr).next = stack;
		(*stack).prev = ptr;
		stack = ptr;
		elements++;
	}
}

int lru_write(unsigned long long int tag){
	struct c_ent* ptr;
	ptr = stack;
	while(ptr != NULL){
		if(tag ^ (*ptr).tag == 0){
			(*ptr).dirty = 1;
			return 0;
		}
		else{
			ptr = (*ptr).next;
		}
	}
	return -1;
}


void lru_flush(void){
	struct c_ent* ptr;
	bottom = NULL;
	while(stack != NULL){
		ptr = stack;
		stack = (*ptr).next;
		
		if((*ptr).next != NULL)(*ptr).next.prev = NULL;
		(*ptr).next = NULL;
		free(ptr);
	}
}

#endif
