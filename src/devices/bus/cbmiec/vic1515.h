// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    Commodore VIC-1515 Printer emulation

**********************************************************************/

#ifndef MAME_BUS_CBMIEC_VIC1515_H
#define MAME_BUS_CBMIEC_VIC1515_H

#pragma once

#include "cbmiec.h"
#include "cpu/mcs48/mcs48.h"



//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> vic1515_device

class vic1515_device : public device_t, public device_cbm_iec_interface
{
public:
	// construction/destruction
	vic1515_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	void vic1515_io(address_map &map);
	void vic1515_mem(address_map &map);
protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	// optional information overrides
	virtual const tiny_rom_entry *device_rom_region() const override;
	virtual void device_add_mconfig(machine_config &config) override;
	virtual ioport_constructor device_input_ports() const override;

	// device_cbm_iec_interface overrides
	void cbm_iec_atn(int state) override;
	void cbm_iec_data(int state) override;
	void cbm_iec_reset(int state) override;
};


// device type definition
DECLARE_DEVICE_TYPE(VIC1515, vic1515_device)


#endif // MAME_BUS_CBMIEC_VIC1515_H
