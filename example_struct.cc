#include "allocator.hh"

struct test {
	int a;
	int b;
};

int main() {
	mallocator alloc{};

	auto t = (test*)&alloc.allocate(sizeof(test),alignof(test));
	t->a = 10;
	t->b = 20;
	blk of_t{t};
	alloc.deallocate(of_t);
}
