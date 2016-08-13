#include "Functions.h"
#include <iostream>

using namespace std;

void menu();

int main()
{
	previous_load();

	while (!cin.fail())
	{
		mem_dump();
		menu();
	}

	getchar();
	return 0;
}

void menu()
{
	size_t tmp, size, addr;

	cout << endl << "1. Allocate    2. Reallocate    3. Free" << endl;
	cin >> tmp;

	switch (tmp)
	{
	case 1:
		cout << "Size: ";
		cin >> size;

		mem_alloc(size);
		break;
	case 2:
		cout << "Address: ";
		cin >> addr;
		cout << "New size: ";
		cin >> size;

		mem_realloc((size_t *)addr, size);
		break;
	case 3:
		cout << "Address: ";
		cin >> addr;
		
		mem_free((size_t *)addr);
		break;
	default:
		cout << " Error input" << endl;
		break;
	}
}