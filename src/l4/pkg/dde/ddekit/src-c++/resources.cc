#include "internal.h"

DDEKit::ResourceManager_impl::ResourceManager_impl()
{
}


DDEKit::ResourceManager_impl::~ResourceManager_impl()
{
}


bool DDEKit::ResourceManager_impl::add_res_set(L4Io::Res_set *rs)
{
	_devices.push_back(rs);
	return true;
}


bool DDEKit::ResourceManager_impl::request_ioports(l4_addr_t start, l4_addr_t count)
{
	return false;
}


bool DDEKit::ResourceManager_impl::release_ioports(l4_addr_t start, l4_addr_t count)
{
	return false;
}


bool DDEKit::ResourceManager_impl::request_iomem(l4_addr_t start, l4_addr_t size, l4_addr_t *vaddr)
{
	return false;
}


bool DDEKit::ResourceManager_impl::release_iomem(l4_addr_t start, l4_addr_t size)
{
	return false;
}


bool DDEKit::ResourceManager_impl::irq_attach(int irq, bool shared, void (*thread_init)(void*),
                                              void (*handler)(void *), void *data)
{
	return false;
}

bool DDEKit::ResourceManager_impl::irq_detach(int irq)
{
	return false;
}

bool DDEKit::ResourceManager_impl::irq_disable(int irq)
{
	return false;
}

bool DDEKit::ResourceManager_impl::irq_enable(int irq)
{
	return false;
}


DDEKit::ResourceManager *_res_man = 0;
void DDEKit::init_rm()
{
	_res_man = new DDEKit::ResourceManager_impl();
}

inline DDEKit::ResourceManager * DDEKit::ResourceManager::resman()
{
	return _res_man;
}
