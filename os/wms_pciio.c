#include <config.h>
#include <wms_proto.h>

int get_device_number(int device)
{
    int	start, end;
    int	id;

#if HOST_BOARD == CHAMELEON
    start = CHAM_PCICFG_SLOT0_V;
    end = CHAM_PCICFG_SLOT2_V;
#else
    start = 6;
    end = 11;
#endif
    for(; start <= end; ++start) {

		id = get_pci_config_reg(start, 0);
	if ((id & 0xffff) == device) return start;
    }
    return -1;
}

int get_sst_device_number(void)
{
    return get_device_number(0x121A);
}

void *get_sst_addr(void)
{
	return (void*)SST_BASE;
}

#ifndef VIRT_TO_PHYS
# define VIRT_TO_PHYS K1_TO_PHYS
#endif

void *pciMapCardMulti(int vendorId, int device_id, int memsize, int *slot, int bdnum)
{
	void	*addr;
	int	slot_num;

	if(bdnum)
	{
		return((void *)0);
	}
	slot_num = get_sst_device_number();
	if(slot_num >= 0)
	{
		addr = get_sst_addr();
		get_pci_config_reg(slot_num, 4);
		put_pci_config_reg(slot_num, 4, VIRT_TO_PHYS(addr) );
	}
	else
	{
		addr = (void *)0;
		slot_num = 0;
	}
	*slot = slot_num;
	return(get_sst_addr());
}
