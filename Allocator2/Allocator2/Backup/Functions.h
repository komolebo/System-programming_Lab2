enum State {FREE, BUSY, PARTED};

struct Descriptor
{
	int free_blocks_count;
	size_t first_free_block;
	size_t addr, block_size;
	State state;
};

struct Block
{
	size_t addr, size;
	State state;
};

struct Settings
{
	size_t page_size = 4096;
	int pages_number = 10;
};

void * mem_alloc(size_t size);
void * mem_realloc(void * addr, size_t size);
void mem_free(void * addr);
void mem_dump();

void previous_load();