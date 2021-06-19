# Summary
This is an illustrative demo only. This is an example of a possible implementation of composable allocators, and a reference type that exhibits value semantics by default.

# Allocators
An Allocator interface is provided by `alloc_t`. Which defines the following methods;

|Method|Description|
|--|--|
|***Allocations and Deallocations***||
|`blk allocate(size_t size, size_t alignment)`|Create a data block, uninitialised space with alignment and size specified|
|`void deallocate(blk& resource)`|Reclaims the memory represented by blk|
|`void deallocateAll()`|Reclaims all memory handled by this allocator|
|***Sharing***||
|`bool will_free_on_deallocate(blk& resource)`|Ask the allocator if this memory will be reclaimed if the blk is returned.|
|`blk share(blk& resource)`|Inform the allocator the intention of sharing|
|***Reference Type Construction***
|`template<class T, class AS = T, typename... Args> ref<AS> make(Args&&...)`|All Reference Types are constructed via the make() function.|

#### Allocations and Deallocations
```cpp
struct blk {
	void* ptr{nullptr};
	size_t m_size{0};
	bool hasData() { return (ptr != nullptr); }
	void* operator&() { return ptr; }
};
```
Methods that provide a way to create and destroy `blk` instances. a blk is effectively a replacement of underline implementation of a *Type** or *Type&*. 
*C++ Limitation* If we were able to do something like - `template<typename Type> alias Type* = blk<Type>;` this would have made this much easier.
#### Sharing
These methods provide information to the allocator regarding the usage of a blk or reference type. Having multiple links to a blk/object is common usage. This allows the allocator to track, and/or debug incorrect usages in this regard.
This is not limited to Reference Counting. The stack_allocator implementation uses an object_count to assert that when the allocator is destroyed, no references to the data it was managing still exist.
#### Reference Type Construction
All Reference Types are created with the `make()` function. This would ideally be the new operator, but it is not possible to override the return type of new. 
make() returns a `ref<T>` which is a reference to type T.

##### Quick Example
```cpp
class duck {
  public:
    void quack() {
      puts("Quack!");
    }
};

stack_allocator ducks_in_a_row{};
auto duck_one = make<duck>();
auto duck_two = make<duck>();
duck_one->quack();
duck_two->quack();
```
### Reference Type Handles
Reference handles are represented with the `ref<T>` type. This underline type represents the behaviour of all reference handles. In order to allow the programmer to specify alternative behaviour that is desired (shared, weak), syntax has to be provided.
#### shared_ref< T > 
shared_ref< T > is used to signal that you would like a shared reference. To link the new reference handler to an existing blk instance. 
*C++ Limitation* It would be ideal if we could alter the *&*. E.g. `template<typename Type> alias Type& = shared_ref<Type>`

A shared reference can also be created with `operator&()` on an existing reference handle. E.g.
```cpp
	auto duck_one = make<duck>();
	auto shared_duck = &duck_one;
	// This will make duck_one quack.
	shared_duck->quack();
```
Or you can declare the type as shared_ref< T > 
```cpp
	auto duck_one = make<duck>();
	shared_ref<duck> shared_duck = duck_one;
```
While the programmer might request a shared_ref, the underline allocator may not nessessary provide any runtime mechanisms to ensure that you are using these references correctly with regards to object lifetimes. The underline allocator might return a handle with equivalent behaviour as a weak_ref< T > and warn the programmer of any misusages in a debug build.

Again, shared_ref< T > is only signaling that you desire this reference handle to influence the lifetime of this resource. If you are manually managing memory - It is a programming error if the lifetime of the holding ref< T > is destroyed while shared_ref< T > are still alive.

#### weak_ref< T >
weak_ref< T > is used to signal that this handle does not effect the lifetime of the underline reference or blk.

And as such, It is possible these references can point to non existing blks. Null checks are recommended for weak_refs.
*C++ Limitation* It would be ideal if we could alter the *. E.g. `template<typename Type> alias Type* = weak_ref<Type>`


#### ref< T >
This is the only real class type provided. weak_ref and shared_ref all create a ref< T > under the hood. 

##### Value Semantics
All reference handles have value semantics by default. Unless you explicitly ask for a shared_ref or weak_ref via the provided mechanisms, a deep-copy will be performed.
```cpp
	auto duck_one = make<duck>();
	auto shared_duck = &duck_one; // A shared reference to duck_one.
	duck_one->quack();
	auto duck_two = duck_one; // A full copy of duck_one is made.
	auto duck_three = shared_duck; // A full copy of duck_one is made.
```

##### RAII 
ref< T > will automatically clean up the reference object, as standard with RAII memory management.

##### Uninitalised ref< T >, shared_ref< T > and weak_ref< T >
The literal value `uninitialised{}` is provided to express an uninitialised reference. E.g.
```cpp
	ref<duck> no_duck_yet = uninitialised{};
```

### Provided Examples
This repository provides examples and benchmarking.
* example_lexer.cc - I quick lexer, also used for benchmarking
* example_quack.cc - Some ducks and examples of the value semantics provided
* example_struct.cc - An example of struct allocation

#### Example example_struct.cc: Allocating a Structural Type
```cpp
struct test {
	int a;
	int b;
};

mallocator alloc{};
auto t =(test*)alloc.allocate(sizeof(test), alignof(test));
t->a = 10;
t->b = 20;
blk of_t{t};
alloc.deallocate(of_t);
```
*Improvements:* Its noted that this is clunky, It could be useful to have `size_t alignment` have a default value of `alignof(max_align_t)`, which would give behaviour equivalent to standard malloc.
There is also no conversions at present to and from a raw pointer. The point of this project is to highlight that the underline type of a struct pointer (*Type**) or reference type ref (*Type&*) should have a different underline representation. if `test*` was actually a `blk` under the hood, all these issues would automatically go away.

#### Quick Example: Configuring the Global Allocator
This example provides 2 concrete allocators, mallocator and stack_allocator. These can be extended with RefCounted<>. In order to select the global allocator, set `galloc = ` to a location of an instance of one of these allocators.
```cpp
mallocator alloc{};
// or RefCounted<mallocator> alloc{}; etc

int main() {
	galloc = &alloc;
	...
};
```
While it should be fine to change galloc during the running of the system, I would recommend only setting this once at startup.
#### Quick Example: Local Allocator Usage
It is possible to create a local specialised allocator, this can either be done for a function, or stored in a class instance for use when allocating the classes components or children.
```cpp
class creates_stuff {
	alloc_t* my_alloc;
  public:
	creates_stuff(alloc_t* passed_in_alloc): my_alloc(passed_in_alloc) {}
};

mallocator alloc{};
int main() {
	galloc = &alloc;
	{
		stack_allocator local_alloc;
		auto local_duck = local_alloc.make<duck>();
		auto stuff = make<creates_stuff>(&local_alloc);
	}
}
```
Ofcourse, We now have a shared reference to an allocator! We really need to allocate an allocator, and get rid of those * and &'s.

```cpp
// Better
mallocator alloc;

struct creates_stuff2 {
	//alloc_t& my_alloc;
	shared_ref<alloc_t> my_alloc{ uninitialised{} };
 public:
	 creates_stuff2(shared_ref<alloc_t> passed_in_alloc): my_alloc(passed_in_alloc) {}
};

int main() {
	galloc = &alloc;
	// Polymorhpism is broken :( We need to pass in the type we want the handle to be in.
	auto custom_allocator = make<stack_allocator, alloc_t>();
	auto stuff2 = make<creates_stuff2>(&custom_allocator);
};
```
### Benchmark
A rough benchmark test was made to see the differences in allocator strategies. I suspect that I have slowed down the Standard Malloc, I am expecting a minor slowdown when reference counting is enabled. Which currently isn't the case on this test.
|Strategy|Time|CPU|Iterations|% over Malloc|RefCount Overhead
|--|--|--|--|--|--|
|Malloc|2422ns|2336ns|6588235|N/A|
|RefCounted Malloc|2307ns|2302ns|6054054|+5%|5% faster
|Aligned Malloc|2402ns|2380ns|5933775|+1%
|RefCounted Aligned Malloc|2502ns|2490ns|5635220|-3%|4% slower
|Stack Allocator|612ns|609ns|22798982|+296%
|RefCounted Stack|674ns|673ns|20885781|+259%|10% slower
