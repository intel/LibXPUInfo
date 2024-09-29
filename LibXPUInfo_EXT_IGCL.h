// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "LibXPUInfo.h"
#include "igcl_api.h"

namespace XI
{
	// IGCL
	struct IGCLAdapterProperties : ctl_device_adapter_properties_t, NoCopyAssign
	{
		IGCLAdapterProperties();
		~IGCLAdapterProperties();
	};
	struct IGCLPciProperties : ctl_pci_properties_t
	{
		ctl_pci_state_t InitialPCIState;
		IGCLPciProperties();
	};
}
