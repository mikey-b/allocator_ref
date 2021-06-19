#include <cstdlib>
#include <cstdio>
#include <new>
#include <utility>
#include <cstring>
#include <utility>

enum class operating_system { WINDOWS, OTHER };
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define OS_WINDOWS
#else
#define OS_OTHER
#endif // Define OS

#ifdef NDEBUG
static void assert(bool, const char* = "") {}
#else
static void assert(bool predicate, const char* msg = "") {
    if (!predicate) printf("Error: %s\n", msg);
}
#endif

template<class T> class shared_ref;
template<class T> class weak_ref;
template<class T> class ref;

class alloc_t;

// --- Untyped Ref Type ---
struct blk {
    void* ptr{nullptr};
    size_t m_size{0};

    bool hasData() {
        return (ptr != nullptr);
    }

    void* operator&() {
        return ptr;
    }
};

// --- Global Allocator Interface ---
class alloc_t {
 public:
    virtual blk allocate(size_t size, size_t alignment) = 0;
    virtual void deallocate(blk& resource) = 0;
	virtual void deallocateAll() = 0;

    virtual bool will_free_on_deallocate(blk& resource) = 0;
    virtual blk share(blk& resource) = 0;

    template<class T, class AS = T, typename... Args>
    ref<AS> make(Args&&...);
	virtual ~alloc_t() = default;
};

// --- Typed Ref ---

struct uninitialised{};
struct weak_flag {};

template<class T>
class ref {
 protected:


    blk m_data;
    alloc_t* m_alloc;
    bool m_weak_ref;

 public:
    // --- Constructors ---
    ref(blk& data, alloc_t* alloc): m_data(data), m_alloc(alloc), m_weak_ref(false) {}
    ref(blk& data, alloc_t* alloc, weak_flag): m_data(data), m_alloc(alloc), m_weak_ref(true) {}

    // --- Initialisation Constructors ---

    // --- Assign uninitialised ---
    ref(uninitialised const&) {
        m_data = {nullptr, 0};
        m_alloc = nullptr;
        m_weak_ref = false;
    }

    // --- Copy Constructor ---
    ref(ref const& original) {
		assert(m_data.ptr == nullptr);
        m_weak_ref = false;
        m_alloc = original.m_alloc;
        //printf("copy constructor, m_alloc = %p\n", m_alloc);
        m_data = m_alloc->allocate(sizeof(T), alignof(T));
        assert(m_data.ptr);
        auto org_obj = static_cast<T*>(original.m_data.ptr);
        auto new_obj = static_cast<T*>(m_data.ptr);
        *new_obj = *org_obj;
        //new (m_data.ptr) T(*org_obj);

        //printf("copied [%p] to [%p]\n", original.m_data.ptr, &m_data);
        assert(m_alloc);
    }

    ref& operator=(ref& original) {
        // Clean up the old data we we're holding.
        if ((m_data.hasData()) && (!m_weak_ref)) {
            if (m_alloc->will_free_on_deallocate(m_data)) {
                (static_cast<T*>(m_data.ptr))->~T();
            }
            m_alloc->deallocate(m_data);
        }

        m_weak_ref = false;
        m_alloc = original.m_alloc;
        m_data = m_alloc->allocate(sizeof(T), alignof(T));
        assert(m_data.ptr);
        auto org_obj = static_cast<T*>(original.m_data.ptr);
        auto new_obj = static_cast<T*>(m_data.ptr);
        *new_obj = *org_obj;
        //new (m_data.ptr) T(*org_obj);

        //printf("copied = [%p] to [%p]\n", original.m_data.ptr, &m_data);
        assert(m_alloc);
        return *this;
    }

    ref& operator=(ref&& original) {
        // Clean up the old data we we're holding.
        if ((m_data.hasData()) && (!m_weak_ref)) {
            if (m_alloc->will_free_on_deallocate(m_data)) {
                (static_cast<T*>(m_data.ptr))->~T();
            }
            m_alloc->deallocate(m_data);
        }

        m_weak_ref = original.m_weak_ref;
        m_alloc = original.m_alloc;
        m_data = original.m_data;

        original.m_weak_ref = false;
        original.m_alloc = nullptr;
        original.m_data.ptr = nullptr;
        original.m_data.m_size = 0;
        //printf("shared = [%p] to [%p]\n", original.m_data.ptr, &m_data);
        assert(m_alloc);
        return *this;
    }

    // --- Shared Reference ---
    operator shared_ref<T>() {
        auto res = m_alloc->share(m_data);
        if (!res.hasData()) {
            //puts("Creating a weak reference\n");
            return {m_data, m_alloc, weak_flag{} } ;
        } else {
            return {res, m_alloc};
        }
    }

    ref operator&() {
        // attempt to return a shared_ref. If this fails, return a weak_ref?
        auto res = m_alloc->share(m_data);
        if (!res.hasData()) {
            assert(0, "shared_ref not supported, making weak_ref.\n");
            return { m_data, m_alloc, weak_flag{} };
        }
        return { res, m_alloc };
    }

    // --- Weak Reference ---
    operator weak_ref<T>() {
        return { m_data, m_alloc, weak_flag{} };
    }

    // --- Accessor ---
    T* operator->() {
        if (!m_data.hasData()) {
            assert(0, "nullptr dereference!");
        }
        return static_cast<T*>(m_data.ptr);
    }

    // --- Deconstructor ---
    ~ref() {
        //printf("testing for dealloc [%d, %d]\n", !m_data, m_weak_ref);
        if (m_weak_ref) return;

        if (m_data.hasData()) {
			//printf("%d\n", m_alloc->will_free_on_deallocate(m_data));
			if (m_alloc->will_free_on_deallocate(m_data)) {
				static_cast<T*>(m_data.ptr)->~T();
			}
			m_alloc->deallocate(m_data);
		}
    }
};

// Theres no such thing as a shared_ref or weak_ref.
// They are just syntactic tools to convey the type
// of ref behaviour the construct to.
template<class T> class shared_ref: public ref<T> {
 public:
    shared_ref(blk data, alloc_t* alloc): ref<T>(data, alloc) {}
};
template<class T> class weak_ref: public ref<T> {
 public:
    weak_ref(blk data, alloc_t* alloc): ref<T>(data, alloc, weak_flag{}) {}
};

// --- Default Allocator Methods ---
template<class T, class AS, typename... Args>
ref<AS> alloc_t::make(Args&&... args)  {
    static_assert(std::is_base_of<AS,T>::value);
    auto blk = this->allocate(sizeof(T), alignof(T));
    new (blk.ptr) T(std::forward<Args>(args)...);
    return {blk, this};
}

class mallocator: public alloc_t {
 public:
    blk allocate(size_t size, size_t alignment) override {
		//auto p = malloc(size);
		#ifdef OS_WINDOWS
			auto p = _aligned_malloc(size, alignment);
		#else
			auto p = aligned_alloc(alignment, size);
		#endif
		//printf("allocated [%p, %zu]\n", p, size);
		return {p, size};
    }
    void deallocate(blk& resource) override {
        //printf("freeing = %p\n", resource.ptr);
        #ifdef OS_WINDOWS
			_aligned_free(resource.ptr);
        #else
			free(resource.ptr);
        #endif // OS_WINDOWS
    }

    bool will_free_on_deallocate(blk&) override { return true; }
    blk share(blk&) override {
        assert(0, "malloc does not support sharing of references.");
        return { };
	}
	void deallocateAll() override {
		assert(0, "malloc does not support deallocateAll");
	}
};

class stack_allocator: public alloc_t {
    enum class byte : unsigned char {};

    byte _data[4*1024];
    size_t _pos{0};
    size_t object_count{0};
 public:
    blk allocate(size_t size, size_t alignment) override {
        //printf("Stack_allocator: allocate(%zu, %zu)\n", size, alignment);
        auto padding = _pos % alignment;

        void* res = &_data[padding + _pos];
        _pos += padding + size;
        object_count+=1;

        return {res, size};
    }

    bool will_free_on_deallocate(blk&) override { return true; }
    blk share(blk&) override {
        assert(0, "stack_allocator does not support sharing of references.");
        return { };
    }
    void deallocate(blk& resource) override {
		object_count-=1;
		if (( ((size_t)resource.ptr) + resource.m_size) == _pos) {
			printf("freeing!\n");
			_pos = (size_t)resource.ptr;
		}
    }

    void deallocateAll() override {
        _pos = 0;
        object_count = 0;
    }

    ~stack_allocator() {
		assert(object_count == 0, "References to data still exist");
		if (object_count == 0) {
			assert(_pos == 0, "References freed in poor order for stack_allocator usage");
		}
    }
};

template<class baseAllocator>
class RefCounted: public baseAllocator {
	size_t object_count{0};
 public:
    blk allocate(size_t size, size_t alignment) override {
        auto total_size = size + sizeof(int);
        auto block = baseAllocator::allocate(total_size, alignment);

        int* count = (int*)((size_t)&block + size);
        *count = 1;
        //printf("count = %d\n", *count);
        block.m_size = size; // Remove the int overhead

		object_count+=1;
        return block;
    }
    bool will_free_on_deallocate(blk& resource) override {
        int* count = (int*)((size_t)&resource + resource.m_size);
        //printf("count = %d\n", *count);
        return (*count == 1);
    }
    blk share(blk& resource) override {
        //puts("making shared\n");
        int* count = (int*)((size_t)&resource + resource.m_size);
        *count+=1;
        //printf("count = %d\n", *count);
        return {resource.ptr, resource.m_size};
    }

    void deallocate(blk& resource) override {
        int* count = (int*)((size_t)&resource + resource.m_size);
        *count-=1;

        resource.m_size += sizeof(int); // We need to re add the reference metadata to the blk size, for the underline allocator.

        if (*count == 0) {
			object_count-=1;
			baseAllocator::deallocate(resource);
        }
    }

    ~RefCounted() {
    	assert(object_count == 0, "Live references still exist");
		if (object_count != 0) baseAllocator::deallocateAll();
    }
};

extern alloc_t* galloc;
alloc_t* galloc;

template<class T, class AS = T, typename... Args>
ref<AS> make(Args&&... args) {
    static_assert(std::is_base_of<AS, T>::value);
	assert(galloc != nullptr);
    return galloc->make<T, AS>(std::forward<Args>(args)...);
}
