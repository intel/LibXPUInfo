#ifdef XPUINFO_USE_IGCL

#include "LibXPUInfo_EXT_IGCL.h"

namespace XI
{
	// IGCL
	IGCLAdapterProperties::IGCLAdapterProperties() :
		ctl_device_adapter_properties_t{ 0 }
	{
		Size = sizeof(ctl_device_adapter_properties_t);
		pDeviceID = malloc(sizeof(LUID));
		device_id_size = sizeof(LUID);
	};
	IGCLAdapterProperties::~IGCLAdapterProperties()
	{
		if (pDeviceID)
			free(pDeviceID);
	}
	IGCLPciProperties::IGCLPciProperties() :
		ctl_pci_properties_t{ 0 }, InitialPCIState{ 0 }
	{
		Size = sizeof(ctl_pci_properties_t);
		maxSpeed.Size = sizeof(ctl_pci_speed_t);
		InitialPCIState.Size = sizeof(ctl_pci_state_t);
		InitialPCIState.speed.Size = sizeof(ctl_pci_speed_t);
	}
}

#endif // XPUINFO_USE_IGCL
