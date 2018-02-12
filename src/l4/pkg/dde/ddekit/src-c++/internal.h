#pragma once

#include <l4/thread/__usem_wrap.h>
#include <l4/sys/kdebug.h>

#include <l4/io/res_set>
#include <l4/dde/ddekit/resources.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/memory.h>

#include <list>

namespace DDEKit
{

class DDEKitObject
{
	public:
		void * operator new(size_t cbSize)  throw (std::bad_alloc)
		{
			void *r = malloc(cbSize);
			if (!r) throw new std::bad_alloc();
			return r;
		}

		void * operator new[](size_t cbSize)  throw (std::bad_alloc)
		{
			void *r = malloc(cbSize);
			if (!r) throw new std::bad_alloc();
			return r;
		}

		void operator delete(void *ptr) throw()
		{ free(ptr); }

		void operator delete[](void *ptr) throw()
		{ free(ptr); }
};

static inline void _create_usem(l4_u_semaphore_t **sem,
                                l4_cap_idx_t *cap, l4_addr_t *base,
                                int initval = 1)
{
	l4_addr_t m = reinterpret_cast<l4_addr_t>(DDEKit::simple_malloc(
	                                          sizeof(l4_u_semaphore_t) + 
	                                                 2 * sizeof(l4_mword_t)));
	if (!m)
		DDEKit::panic("lock_init failed: memory allocation");

	*base = m;

	m += 2 * sizeof(l4_mword_t);
	m &= (~(2 * sizeof(l4_mword_t) - 1));

	*sem = reinterpret_cast<l4_u_semaphore_t *>(m);

	if (__do_init_lock(*sem, cap, initval) == 0)
		DDEKit::panic("lock init failed: usem init");
}


static inline void _delete_usem(l4_cap_idx_t cap, l4_addr_t base)
{
	__lock_free(cap);
	DDEKit::simple_free(reinterpret_cast<void*>(base));
}


class ResourceManager_impl : public ResourceManager, public DDEKitObject
{
	public:
		explicit ResourceManager_impl();
		virtual ~ResourceManager_impl();

		// Add a resource set to the internal representation
		bool add_res_set(L4Io::Res_set *rs);		
		virtual bool request_ioports(l4_addr_t start, l4_addr_t count);
		virtual bool release_ioports(l4_addr_t start, l4_addr_t count);
		virtual bool request_iomem(l4_addr_t start, l4_addr_t size, l4_addr_t *vaddr);
		virtual bool release_iomem(l4_addr_t start, l4_addr_t size);
		virtual bool irq_attach(int irq, bool shared, void (*thread_init)(void*),
		                        void (*handler)(void *), void *data);
		virtual bool irq_detach(int irq);
		virtual bool irq_disable(int irq);
		virtual bool irq_enable(int irq);

	private:
		std::list<L4Io::Res_set*> _devices;
		ResourceManager_impl(ResourceManager_impl const &rm) { }
		ResourceManager_impl& operator=(ResourceManager_impl const &) { return *this; }
};

void init_rm();
}

