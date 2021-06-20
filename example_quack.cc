#include "allocator.hh"
#include <cstdio>

class duck {
	char const* m_name;
	int count;
 public:
 	duck(char const* name): m_name(name), count(42) {}

 	void change_name(char const* name) {
		m_name = name;
 	}

	void quack() {
		printf("%s says Quack %d!\n", m_name, count);
		count+=1;
	}
};

static RefCounted<mallocator> g{};
int main() {
	galloc = &g;

	// auto bob = make<duck>(...); is also possible.
	ref<duck> bob = make<duck>("Bob");
	bob->quack();		// Bob says Quack 42!
	bob->quack();		// Bob says Quack 43!

	// Value semantics - complete copy of bob
	auto copy_bob = bob;
	copy_bob->change_name("CBob");

	bob->quack();		// Bob says Quack 44!
	bob->quack();		// Bob says Quack 45!
	copy_bob->quack();	// CBob says Quack 44!

	// Shared/Reference semantics - bob_ptr is a pointer to bob
	auto bob_ptr = &bob;
	bob->quack();		// Bob says Quack 46!

	// Copy semantics test - Bob becomes a new copy created frrom copy_bob;
	bob = copy_bob;
	bob->quack();		// CBob says Quack 45!
	bob->quack();		// CBob says Quack 46!

	copy_bob->quack();	// CBob says Quack 45!

	// Weak or Shared Pointer! Allocator Defined.
	puts("\n");
						// If RefCounted<>
	bob_ptr->quack();	// Bob says Quack 47! (This references the original object created on line 25).

						// If a weak_ref
						// This is an invalid reference. Undefined behaviour! Should really throw an exception.


	auto test = make_unique<duck>("test2");
	auto test2 = galloc->move(test);

	test2->quack();
}
