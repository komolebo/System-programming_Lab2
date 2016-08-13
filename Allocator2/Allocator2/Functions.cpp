#include "Functions.h"
#include <list>
#include <map>
#include <math.h>

using namespace std;

void cast_to4(size_t &);
void cast_to16(size_t &);
size_t get_space_addr(size_t);
size_t get_space_addr(size_t, size_t);
int get_page_number(size_t addr);
int get_block_number(size_t p_addr, size_t b_addr);

list<list<Block>> Memory;
list<Descriptor> Descriptors;
map<size_t, size_t> Hash_table;
Settings settings;

void previous_load()
{
	for (int i = 0; i < settings.pages_number; ++i)
	{
		list<Block> page;
		Memory.push_back(page);

		Descriptor desc;
		desc.addr = i * settings.page_size;
		desc.state = FREE;
		Descriptors.push_back(desc);
	}
}

void * mem_alloc(size_t size)
{
	cast_to4(size);

	// Find place
	if (size)
	{
		// At first find already parted pages
		for (auto & d : Descriptors)
		{
			if (d.state == PARTED && (int)get_space_addr(size, d.addr) >= 0)
			{
				auto page = Memory.begin();
				advance(page, get_page_number(d.addr));
				
				size_t free_addr = get_space_addr(size, d.addr);
				int N = get_block_number(d.addr, free_addr);
				Hash_table[free_addr] = size;

				list<Block>::iterator b = (*page).begin();
				advance(b, N);

				for (; size; (*b).state = BUSY, b++)
					size >= (*b).size ? size -= (*b).size : size = 0;

				return (size_t *)free_addr;
			}
		}
		// Group 2. Parted pages not found, look for fully free pages.
		if (size > settings.page_size / 2)
		{
			int addr = get_space_addr(size);
			if (addr >= 0)
			{
				list<Descriptor>::iterator page = Descriptors.begin();
				advance(page, get_page_number((size_t)addr));
				Hash_table[(*page).addr] = size;

				for (; size; (*page).state = BUSY, page++)
					size >= settings.page_size ? size -= settings.page_size : size = 0;

				return (size_t *)addr;
			}
			return NULL;
		}

		// Same for group 1
		for (auto & d : Descriptors)
		{
			if (d.state == FREE)
			{
				// Create blocks in page
				auto page = Memory.begin();
				advance(page, get_page_number(d.addr));

				cast_to16(size);
				//size *= 2;

				for (int i = 0; i < (int)(settings.page_size / size); ++i)
				{
					Block b;
					b.addr = d.addr + i * size;
					b.size = size;
					b.state = FREE;

					(*page).push_back(b);
				}
				(*(*page).begin()).state = BUSY;
				Hash_table[d.addr] = size;
				d.block_size = size;
				d.first_free_block = -1; //ToDo
				d.state = PARTED;
				d.first_free_block = d.addr + settings.page_size + size;

				return (size_t *)d.addr;
			}
		}
	}

	return NULL;
}

void * mem_realloc(void * addr, size_t size)
{
	// Find page
	auto page = Memory.begin();
	int p_num = get_page_number((size_t)addr);
	
	if (p_num < 0) return NULL; // Wrong page address
	advance(page, p_num);

	auto desc = Descriptors.begin();
	advance(desc, p_num);

	cast_to4(size);

	// If pages to reallocate
	if (!(*page).size() && Hash_table.find((size_t)addr) != Hash_table.end())
	{
		if (size == 0) 
		{
			int pages_to_realloc = (int)(ceil)((double)Hash_table[(size_t)addr] / settings.page_size);

			while (pages_to_realloc--)
			{
				(*desc).state = FREE;
				desc++;
			}
			return NULL;
		}	
		else if (size < settings.page_size) // Decrease size
		{
			int pages_to_realloc = (int)(ceil)((double)Hash_table[(size_t)addr] / settings.page_size);

			auto tmp = desc;
			while (pages_to_realloc--) (*tmp++).state = FREE;

			size_t realloc_size = settings.page_size - size;
			size_t b_size = size;

			// Choosing new blocks size
			if (fmin(settings.page_size - size, size) > settings.page_size / 4)
				b_size = settings.page_size / 4;
			else  // Blocks must be 2^n
				cast_to16(b_size);
			
			// Adding blocks with calculated size
			Hash_table[(size_t)addr] = size;
			for (size_t i = 0; i < (int)(settings.page_size / b_size); i++)
			{
				Block b;
				b.addr = (*desc).addr + i * b_size;
				b.size = b_size;
				b.state = size ? BUSY : FREE;

				(*page).push_back(b);
				size = ((int)size - (int)b_size > 0) ? size - b_size : 0;
			}
			(*desc).state = PARTED;

			return (size_t *)addr;
		}
		else if (size < Hash_table[(size_t)addr])
		{
			int new_pages_count = (int)(ceil)((double)size / settings.page_size);
			int pages_to_release = (int)(ceil)((double)Hash_table[(size_t)addr] / settings.page_size) - new_pages_count;

			while (new_pages_count--) desc++;
			while (pages_to_release--) (*desc++).state = FREE;

			return (size_t *)addr;
		}
		else if (size > Hash_table[(size_t)addr])
		{
			int pages_to_release = (int)(ceil)((double)Hash_table[(size_t)addr] / settings.page_size);
			while (pages_to_release--) (*desc++).state = FREE;

			return mem_alloc(size);
		}
	}
	// If blocks to reallocate
	else if ((*page).size() && Hash_table.find((size_t)addr) != Hash_table.end())
	{
		auto b = (*page).begin();
		advance(b, get_block_number((*desc).addr, (size_t)addr));

		if (size == 0)  // Destroy block
		{
			int blocks_to_realloc = (int)(ceil)((double)Hash_table[(size_t)addr] / (*b).size);
			while (blocks_to_realloc--) (*b++).state = FREE;
			
			// If all blocks are free
			for (auto & block : (*page))
				if (block.state != FREE)
					return (size_t *)(*b).addr;

			(*desc).state = FREE;
			(*page).clear();
			Hash_table.erase((size_t)addr);
			return NULL;
		}
		else if (size >= (*b).size && size <= Hash_table[(size_t)addr])
		{
			int new_blocks_count = (int)(ceil)((double)size / (*b).size);
			int blocks_to_release = (int)(ceil)((double)Hash_table[(size_t)addr] / (*b).size) - new_blocks_count;

			while (new_blocks_count--) b++;
			while (blocks_to_release--) (*b++).state = FREE;

			Hash_table[(size_t)addr] = size;

			return (size_t *)addr;
		}
		else if (size > Hash_table[(size_t)addr]) // Increase block's content space
		{
			int blocks_to_release = (int)(ceil)((double)Hash_table[(size_t)addr] / (*b).size);
			while (blocks_to_release--) (*b++).state = FREE;

			// If each block is free, then release the page
			for (auto it2 = (*page).begin(); it2 != (*page).end(); ++it2)
				if ((*it2).state != FREE)
					return mem_alloc((size_t)size);

			(*desc).state = FREE;
			(*page).clear();

			return mem_alloc((size_t)size);
		}
	}

	return NULL;
}

void mem_free(void * addr)
{
	int i = 0;

	for (auto & d : Descriptors)
	{
		if (d.addr <= (size_t)addr && d.addr + settings.page_size > (size_t)addr && d.state != FREE)
		{
			auto page = Memory.begin();
			advance(page, i);

			// If page or several pages to be released
			if (Hash_table.find((size_t)addr) != Hash_table.end() && (*page).size() < 2)
			{
				int pages_to_release = (int)(ceil)((double)(Hash_table[(size_t)addr]) / settings.page_size);
				auto desc = Descriptors.begin();
				advance(desc, i);

				while (pages_to_release--)
				{
					(*page).clear();
					page++;
					(*desc).state = FREE;
					desc++;
				}
				Hash_table.erase((size_t)addr);

				return;
			}
			
			// If block to be released
			if ((size_t)addr >= d.addr && (size_t)addr <= d.addr + settings.page_size && Hash_table.find((size_t)addr) != Hash_table.end())
				for (list<Block>::iterator it = (*page).begin(); it != (*page).end(); ++it)
				{
					if ((*it).addr == (size_t)addr)
					{
						int blocks_to_release = (int)(ceil)((double)Hash_table[(size_t)addr] / (*it).size);
						while (blocks_to_release--)
						{
							(*it).state = FREE;
							it++;
						}
						Hash_table.erase((size_t)addr);

						// If each block is free, then release the page
						for (list<Block>::iterator it2 = (*page).begin(); it2 != (*page).end(); ++it2)
							if ((*it2).state != FREE)
								return;

						d.state = FREE;
						(*page).clear();

						return;
					}
				}
		}
		else if (d.addr + settings.page_size > (size_t)addr) // Without extra iteration
			return;
		i++;
	}

}

void mem_dump()
{
	int i = 0;

	for (auto & d : Descriptors)
	{
		printf("Page %d [%d..%d]  \t\t  \t\t %5s \n", i, d.addr, d.addr + settings.page_size - 1, (d.state == PARTED ? "PARTED" : (d.state == FREE ? "FREE" : "BUSY")));

		auto page = Memory.begin();
		advance(page, get_page_number(d.addr));

		for (auto & b : *page)
			printf("\t%5d..%5d  \t\t\t %6s\n", b.addr, b.addr + b.size - 1, (b.state == FREE) ? "FREE" : "BUSY");

		i++;
	}
	printf("\n");
}

void cast_to4(size_t & size)
{
	size % 4 ? (size = 4 * (1 + (size / 4))) : NULL;
}

void cast_to16(size_t & size)
{
	size_t tmp = (int)fmin(settings.page_size - size, size);

	size = (tmp % 16 == 0) ? 1 : 2;
	while (tmp /= 2) size *= 2;
}

size_t get_space_addr(size_t size)
{
	size_t tmp = 0;

	for (list<Descriptor>::iterator it = Descriptors.begin(); it != Descriptors.end(); ++it)
	{
		auto it2 = it;
		for (; it2 != Descriptors.end() && (*it2).state == FREE; it2++)
			tmp += settings.page_size;

		if (tmp >= size)
			return (*it).addr;
		else
			tmp = 0;
	}

	return -1;
}

size_t get_space_addr(size_t size, size_t addr) // inside the block
{
	// Find page with that address
	size_t tmp = 0;

	auto page = Memory.begin();
	advance(page, get_page_number(addr));

	for (list<Block>::iterator it = (*page).begin(); it != (*page).end(); ++it)
	{
		auto it2 = it;
		for (; it2 != (*page).end() && (*it2).state == FREE; it2++)
			tmp += (*it2).size;

		if (tmp >= size)
			return (*it).addr;
		else
			tmp = 0;
	}

	return -1;
}

int get_page_number(size_t addr)
{
	int i = 0;
	for (auto & d : Descriptors)
	{
		if (d.addr <= addr && d.addr + settings.page_size > addr)
			return i;
		i++;
	}

	return -1;
}

int get_block_number(size_t p_addr, size_t b_addr)
{
	auto page = Memory.begin();
	advance(page, get_page_number(p_addr));

	int i = 0;
	for (auto & b : *page)
	{
		if (b.addr == b_addr)
			return i;
		i++;
	}
	
	return -1;
}