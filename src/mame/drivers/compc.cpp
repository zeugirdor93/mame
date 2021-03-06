// license:BSD-3-Clause
// copyright-holders:Carl
// Commodore PC 10 / PC 20 / PC 30
#include "emu.h"

#include "cpu/i86/i86.h"
#include "machine/genpc.h"
#include "machine/nvram.h"
#include "machine/pckeybrd.h"

#include "coreutil.h"


class compc_state : public driver_device
{
public:
	compc_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_mb(*this, "mb"),
		m_keyboard(*this, "pc_keyboard")
	{ }

	required_device<cpu_device> m_maincpu;
	required_device<pc_noppi_mb_device> m_mb;
	required_device<pc_keyboard_device> m_keyboard;

	void machine_reset() override;

	DECLARE_WRITE8_MEMBER(pioiii_w);
	DECLARE_READ8_MEMBER(pioiii_r);
	DECLARE_WRITE8_MEMBER(pio_w);
	DECLARE_READ8_MEMBER(pio_r);

	void compc(machine_config &config);
	void pc10iii(machine_config &config);
	void compc_io(address_map &map);
	void compc_map(address_map &map);
	void compciii_io(address_map &map);
private:
	u8 m_portb, m_dips;
};

void compc_state::machine_reset()
{
	m_portb = 0;
	m_dips = 0;
}

WRITE8_MEMBER(compc_state::pio_w)
{
	switch (offset)
	{
		case 1:
			m_portb = data;
			m_mb->m_pit8253->write_gate2(BIT(data, 0));
			m_mb->pc_speaker_set_spkrdata(BIT(data, 1));
			m_keyboard->enable(BIT(data, 6));
			if(data & 0x80)
				m_mb->m_pic8259->ir1_w(0);
			break;
	}
}


READ8_MEMBER(compc_state::pio_r)
{
	int data = 0;
	switch (offset)
	{
		case 0:
			data = m_keyboard->read(space, 0);
			break;
		case 1:
			data = m_portb;
			break;
		case 2:
			if(BIT(m_portb, 3))
			{
				/* read hi nibble of S2 */
				data = (ioport("DSW0")->read() >> 4) & 0x0f;
			}
			else
			{
				/* read lo nibble of S2 */
				data = ioport("DSW0")->read() & 0x0f;
			}
			if(m_mb->pit_out2())
				data |= 0x20;
			break;
	}
	return data;
}

WRITE8_MEMBER(compc_state::pioiii_w)
{
	switch (offset)
	{
		case 1:
			m_portb = data;
			m_mb->m_pit8253->write_gate2(BIT(data, 0));
			m_mb->pc_speaker_set_spkrdata(BIT(data, 1));
			m_keyboard->enable(BIT(data, 6));
			if(data & 0x80)
				m_mb->m_pic8259->ir1_w(0);
			break;
		case 2:
			m_dips = (data & ~0x30) | ioport("DSW0")->read();
			break;
	}
}


READ8_MEMBER(compc_state::pioiii_r)
{
	int data = 0;
	switch (offset)
	{
		case 0:
			data = m_keyboard->read(space, 0);
			break;
		case 1:
			data = m_portb;
			break;
		case 2:
			if(!BIT(m_portb, 2))
				data = (m_dips >> 4) & 0x0f;
			else
				data = m_dips & 0x0f;
			if(m_mb->pit_out2())
				data |= 0x30;
			break;
	}
	return data;
}

static INPUT_PORTS_START(compciii)
	PORT_START("DSW0") /* IN1 */
	PORT_DIPNAME( 0x30, 0x30, "Graphics adapter")
	PORT_DIPSETTING(    0x00, "EGA/VGA" )
	PORT_DIPSETTING(    0x10, "Color 40x25" )
	PORT_DIPSETTING(    0x20, "Color 80x25" )
	PORT_DIPSETTING(    0x30, "Monochrome" )

	PORT_INCLUDE(pc_keyboard)
INPUT_PORTS_END

static INPUT_PORTS_START(compc)
	PORT_START("DSW0") /* IN1 */
	PORT_DIPNAME( 0xc0, 0x40, "Number of floppy drives")
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x40, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0xc0, "4" )
	PORT_DIPNAME( 0x30, 0x30, "Graphics adapter")
	PORT_DIPSETTING(    0x00, "EGA/VGA" )
	PORT_DIPSETTING(    0x10, "Color 40x25" )
	PORT_DIPSETTING(    0x20, "Color 80x25" )
	PORT_DIPSETTING(    0x30, "Monochrome" )
	PORT_DIPNAME( 0x0c, 0x0c, "RAM banks")
	PORT_DIPSETTING(    0x00, "1 - 16/ 64/256K" )
	PORT_DIPSETTING(    0x04, "2 - 32/128/512K" )
	PORT_DIPSETTING(    0x08, "3 - 48/192/576K" )
	PORT_DIPSETTING(    0x0c, "4 - 64/256/640K" )
	PORT_DIPNAME( 0x02, 0x00, "8087 installed")
	PORT_DIPSETTING(    0x00, DEF_STR(No) )
	PORT_DIPSETTING(    0x02, DEF_STR(Yes) )
	PORT_DIPNAME( 0x01, 0x01, "Boot from floppy")
	PORT_DIPSETTING(    0x01, DEF_STR(Yes) )
	PORT_DIPSETTING(    0x00, DEF_STR(No) )

	PORT_INCLUDE(pc_keyboard)
INPUT_PORTS_END

ADDRESS_MAP_START(compc_state::compc_map)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0xf0000, 0xfffff) AM_ROM AM_REGION("bios", 0)
ADDRESS_MAP_END

ADDRESS_MAP_START(compc_state::compc_io)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x00ff) AM_DEVICE("mb", pc_noppi_mb_device, map)
	AM_RANGE(0x0060, 0x0063) AM_READWRITE(pio_r, pio_w)
ADDRESS_MAP_END

ADDRESS_MAP_START(compc_state::compciii_io)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x00ff) AM_DEVICE("mb", pc_noppi_mb_device, map)
	AM_RANGE(0x0060, 0x0063) AM_READWRITE(pioiii_r, pioiii_w)
ADDRESS_MAP_END

MACHINE_CONFIG_START(compc_state::compc)
	MCFG_CPU_ADD("maincpu", I8088, 4772720*2)
	MCFG_CPU_PROGRAM_MAP(compc_map)
	MCFG_CPU_IO_MAP(compc_io)
	MCFG_CPU_IRQ_ACKNOWLEDGE_DEVICE("mb:pic8259", pic8259_device, inta_cb)

	MCFG_PCNOPPI_MOTHERBOARD_ADD("mb", "maincpu")
	MCFG_DEVICE_REMOVE("mb:pit8253")
	MCFG_DEVICE_ADD("mb:pit8253", FE2010_PIT, 0)
	MCFG_PIT8253_CLK0(XTAL(14'318'181)/12.0) /* heartbeat IRQ */
	MCFG_PIT8253_OUT0_HANDLER(DEVWRITELINE("pic8259", pic8259_device, ir0_w))
	MCFG_PIT8253_CLK1(XTAL(14'318'181)/12.0) /* dram refresh */
	MCFG_PIT8253_OUT1_HANDLER(WRITELINE(ibm5160_mb_device, pc_pit8253_out1_changed))
	MCFG_PIT8253_CLK2(XTAL(14'318'181)/12.0) /* pio port c pin 4, and speaker polling enough */
	MCFG_PIT8253_OUT2_HANDLER(WRITELINE(ibm5160_mb_device, pc_pit8253_out2_changed))

	MCFG_ISA8_SLOT_ADD("mb:isa", "isa1", pc_isa8_cards, "mda", false)
	MCFG_ISA8_SLOT_ADD("mb:isa", "isa2", pc_isa8_cards, "lpt", false)
	MCFG_ISA8_SLOT_ADD("mb:isa", "isa3", pc_isa8_cards, "com", false)
	MCFG_ISA8_SLOT_ADD("mb:isa", "isa4", pc_isa8_cards, "fdc_xt", false)

	MCFG_PC_KEYB_ADD("pc_keyboard", DEVWRITELINE("mb:pic8259", pic8259_device, ir1_w))

	/* internal ram */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("256K")
	MCFG_RAM_EXTRA_OPTIONS("512K, 640K")

	/* software lists */
	MCFG_SOFTWARE_LIST_ADD("disk_list", "ibm5150")
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(compc_state::pc10iii, compc)
	MCFG_DEVICE_MODIFY("maincpu")
	MCFG_CPU_IO_MAP(compciii_io)
MACHINE_CONFIG_END

ROM_START(compc10)
	ROM_REGION(0x10000, "bios", 0)
	ROM_DEFAULT_BIOS("v205")
	ROM_SYSTEM_BIOS(0, "v203", "v2.03")
	ROMX_LOAD("380258-03", 0xc000, 0x4000, CRC(fbe53865) SHA1(a6d6433c055d1c328f71403a2ed2fd5908c23d40), ROM_BIOS(1))
	ROM_SYSTEM_BIOS(1, "v205", "v2.05")
	ROMX_LOAD("380258-04", 0xc000, 0x4000, CRC(e61084da) SHA1(dfb360a6ec6cb1250d8a6243f12a0d702e8479cb), ROM_BIOS(2))
ROM_END

// Note: Commodore PC20-III, PC10-III and COLT share the same BIOS
ROM_START(pc10iii)
	ROM_REGION(0x10000, "bios", 0)
	ROM_DEFAULT_BIOS("v441")
	ROM_SYSTEM_BIOS(0, "v435", "v4.35")
	ROM_SYSTEM_BIOS(1, "v435c", "v4.35c")
	ROM_SYSTEM_BIOS(2, "v436", "v4.36")
	ROM_SYSTEM_BIOS(3, "v436b", "v4.36b")
	ROM_SYSTEM_BIOS(4, "v436c", "v4.36c")
	ROM_SYSTEM_BIOS(5, "v438", "v4.38")
	ROM_SYSTEM_BIOS(6, "v439", "v4.39")
	ROM_SYSTEM_BIOS(7, "v440", "v4.40")
	ROM_SYSTEM_BIOS(8, "v441", "v4.41")
	ROMX_LOAD("318085-01.u201", 0x8000, 0x8000, CRC(be752d1e) SHA1(5e5e63cd6d6269816cd691602e4c4d209fe3df67), ROM_BIOS(1))
	ROMX_LOAD("318085-01_pc-3_bios_4.35c.bin", 0x8000, 0x8000, CRC(adbc4ab7) SHA1(c157e5bc115906705849f185e4fe8e42ab28930c), ROM_BIOS(2))
	ROMX_LOAD("318085-02.u201", 0x8000, 0x8000, CRC(db0c9d04) SHA1(1314ce606840f4f78e58e5f78909e8971387f387), ROM_BIOS(3))
	ROMX_LOAD("318085-03.u201", 0x8000, 0x8000, CRC(559e6b76) SHA1(cdfd4781f3520db7f5111469ebb29c10b39ab587), ROM_BIOS(4))
	ROMX_LOAD("318085-04.u201", 0x8000, 0x8000, CRC(f81e67f0) SHA1(b46613cb5c6ac4beb769778bc35f81777ebe02e1), ROM_BIOS(5))
	ROMX_LOAD("318085-05.u201", 0x8000, 0x8000, CRC(ae9e6a31) SHA1(853ee251cf230818c407a8d13ef060a21c90a8c1), ROM_BIOS(6))
	ROMX_LOAD("318085-06.u201", 0x8000, 0x8000, CRC(1901993c) SHA1(f75060c1c442376bd42c61e74fa9eee053351791), ROM_BIOS(7))
	ROMX_LOAD("318085-07.u201", 0x8000, 0x8000, CRC(505d52b0) SHA1(f717c6ab791d51f35e1c38ffbc81a44075b5f2f8), ROM_BIOS(8))
	ROMX_LOAD("318085-08.u201", 0x8000, 0x8000, CRC(7e228dc8) SHA1(958dfdd637bd31c01b949fac729d6973a7e630bc), ROM_BIOS(9))

	ROM_REGION(0x8000, "gfx1", 0)
	ROMX_LOAD("318086-01_p10c_164a.bin", 0x0000, 0x4000, CRC(ee6c27f0) SHA1(e769cc3a49a1d708bd74eb4ac85bb6ea67220d38), ROM_BIOS(1)) // came with ROM 4.35c
	ROMX_LOAD("318086-01_p10c_164a.bin", 0x0000, 0x4000, CRC(ee6c27f0) SHA1(e769cc3a49a1d708bd74eb4ac85bb6ea67220d38), ROM_BIOS(2))
	ROMX_LOAD("318086-02.u607", 0x0000, 0x8000, CRC(b406651c) SHA1(856f58353391a74a06ebb8ec9f8333d7d69e5fd6), ROM_BIOS(3))
	ROMX_LOAD("318086-02.u607", 0x0000, 0x8000, CRC(b406651c) SHA1(856f58353391a74a06ebb8ec9f8333d7d69e5fd6), ROM_BIOS(4))
	ROMX_LOAD("318086-02.u607", 0x0000, 0x8000, CRC(b406651c) SHA1(856f58353391a74a06ebb8ec9f8333d7d69e5fd6), ROM_BIOS(5))
	ROMX_LOAD("318086-02.u607", 0x0000, 0x8000, CRC(b406651c) SHA1(856f58353391a74a06ebb8ec9f8333d7d69e5fd6), ROM_BIOS(6))
	ROMX_LOAD("318086-02.u607", 0x0000, 0x8000, CRC(b406651c) SHA1(856f58353391a74a06ebb8ec9f8333d7d69e5fd6), ROM_BIOS(7))
	ROMX_LOAD("318086-02.u607", 0x0000, 0x8000, CRC(b406651c) SHA1(856f58353391a74a06ebb8ec9f8333d7d69e5fd6), ROM_BIOS(8))
	ROMX_LOAD("318086-02.u607", 0x0000, 0x8000, CRC(b406651c) SHA1(856f58353391a74a06ebb8ec9f8333d7d69e5fd6), ROM_BIOS(9))
ROM_END

//    YEAR    NAME              PARENT      COMPAT      MACHINE         INPUT     STATE     INIT      COMPANY                            FULLNAME                FLAGS
COMP( 1984,   compc10,          ibm5150,    0,          compc,          compc,    compc_state, 0,        "Commodore Business Machines",     "Commodore PC 10",      MACHINE_NOT_WORKING )
COMP( 1987,   pc10iii,          ibm5150,    0,          pc10iii,        compciii, compc_state, 0,        "Commodore Business Machines",     "Commodore PC-10 III",  MACHINE_NOT_WORKING )
