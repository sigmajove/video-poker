#pragma once
#include<cassert>

class C_Chooser
{
	enum {x_size = 16};
	enum {y_size = 16};
	int table[x_size][y_size];

	int do_choose(int x, int y);

public:

	C_Chooser();

	int choose (int x, int y)
	{
		assert (x >= 0);
		assert (y >= 0);

		return (x < x_size && y < y_size) ? table[x][y] : do_choose(x, y);
	}
};

extern C_Chooser combin;