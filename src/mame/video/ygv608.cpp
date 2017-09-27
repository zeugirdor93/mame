// license:BSD-3-Clause
// copyright-holders:Mark McDougall, Angelo Salese

/*
 *    Yamaha YGV608 - PVDC2 Pattern mode Video Display Controller 2
 *    - Mark McDougall
 *
 *    Notes:
 *    ======
 *
 *    This implementation is far from complete.
 *    There's enough here to emulate Namco ND-1 games.
 *    Some functionality is missing, some is incomplete.
 *    Also missing for ND-1 is rotation and scaling (cosmetic only).
 *
 *    It could also do with some optimisation for speed!
 *
 *    (Still lots of debugging info/options in here!)
 *
 *    TODO: soon
 *    create tilemaps at vh_start time and switch between at runtime
 *    mark tiles dirty when VRAM is written to instead of dirtying entire
 *       screen each frame
 *
 *    T.B.D. (not critical to ND-1)
 *    ======
 *
 *    Rotation
 *    Scaling
 *    Split-screen scrolling by row (by column supported) (see test mode)
 *    Everything else! :)
 *
 *    TODO (2017 edition):
 *    - move ports into device_address_map (done);
 *    - add registers into own space, improve naming and variable usage;
 *    - remove code repetition in tilemap drawing functions;
 *    - add crtc section (done partially);
 *    - fix garbage tiles in Mappy Arrange (done)
 *    - fix tile encryption for Abnormal Check (sets extra bit in cuskey);
 *      nopping bit 0 writes to 0x40081e makes gfxs to draw better!?
 *    - fix attract mode garbage for Namco Collection Vol. 2 (either transparent or page banking select registers) (done);
 *    - fix tilemap dirty flags, move tilemap data in own space prolly helps;
 *    - DMA from/to ROM;
 *    - color palette accessors presumably accesses an internal RAMDAC with controllable auto-increment, convert to that;
 *    - fix char getting cut off from GAME SELECT msg in NCV2 (done, sprite wraparound for sx & sy);
 *    - clean-ups & documentation;
 *
 *
 */
 
 
#include "emu.h"
#include "video/ygv608.h"
#include "screen.h"


// Fundamental constants
enum {
	p5_rwai     = 0x80,     // increment register select on reads
	p5_rrai     = 0x40,     // increment register select on writes
	p5_rn       = 0x3f,     // register select

//  p6_res6     = 0xe0,
	p6_fp       = 0x10,     // position detection flag
	p6_fv       = 0x08,     // vertical border interval flag
	p6_fc       = 0x04,
	p6_hb       = 0x02,     // horizontal blanking flag?
	p6_vb       = 0x01,     // vertical blanking flag?

//  p7_res7     = 0xc0,
	p7_tr       = 0x20,
	p7_tc       = 0x10,
	p7_tl       = 0x08,
	p7_ts       = 0x04,
	p7_tn       = 0x02,
	p7_sr       = 0x01,

	r0_pnya     = 0x80,     // if set, increment name table address after reads
	r0_b_a      = 0x40,     // if set, we're reading from the second plane of tile data
	r0_pny      = 0x3f,     // y-coordinate of current name table address

	r1_pnxa     = 0x80,     // if set, increment name table address after read
	r1_res1     = 0x40,
	r1_pnx      = 0x3f,     // x-coordinate of current name table address

	r2_cpaw     = 0x80,     // if set, increment color palette address after writes
	r2_cpar     = 0x40,     // if set, increment color palette address after reads
//  r2_res2     = 0x20,
	r2_b_a      = 0x10,
	r2_scaw     = 0x08,     // if set, increment scroll address after writes
	r2_scar     = 0x04,     // if set, increment scroll address after reads
	r2_saaw     = 0x02,     // if set, increment sprite address after writes
	r2_saar     = 0x01,     // if set, increment sprite address after reads

	r7_dckm     = 0x80,
	r7_flip     = 0x40,
//  r7_res7     = 0x30,
	r7_zron     = 0x08,     // if set, roz plane is active
	r7_md       = 0x06,     // determines 1 of 4 possible video modes
	r7_dspe     = 0x01,     // if set, display is enabled

	r8_hds      = 0xc0,
	r8_vds      = 0x30,
	r8_rlrt     = 0x08,
	r8_rlsc     = 0x04,
//  r8_res8     = 0x02,
	r8_pgs      = 0x01,

	r9_pts      = 0xc0,
	r9_slh      = 0x38,
	r9_slv      = 0x07,

	r10_spa     = 0xc0,     // misc global sprite attributes (x/y flip or sprite size)
	r10_spas    = 0x20,     // if set, spa controls sprite flip, if clear, controls sprite size
	r10_sprd    = 0x10,     // if set, sprites are disabled
	r10_mcb     = 0x0c,
	r10_mca     = 0x03,

	r11_scm     = 0xc0,
	r11_yse     = 0x20,
	r11_cbdr    = 0x10,
	r11_prm     = 0x0c,     // determines one of 4 priority settings
	r11_ctpb    = 0x02,
	r11_ctpa    = 0x01,

	r12_spf     = 0xc0,
	r12_bpf     = 0x38,
	r12_apf     = 0x07,

//  r14_res14   = 0xfc,
	r14_iep     = 0x02,     // if set, generate IRQ on position detection (?)
	r14_iev     = 0x01,     // if set, generate IRQ on vertical border interval draw

	// these 4 are currently unimplemented by us
	r16_fpm     = 0x80,
	r16_il8     = 0x40,
	r16_res16   = 0x20,
	r16_ih      = 0x1f,

	r17_ba1     = 0x70,
	r17_ba0     = 0x07,
	r18_ba3     = 0x70,
	r18_ba2     = 0x07,
	r19_ba5     = 0x70,
	r19_ba4     = 0x07,
	r20_ba7     = 0x70,
	r20_ba6     = 0x07,
	r21_bb1     = 0x70,
	r21_bb0     = 0x07,
	r22_bb3     = 0x70,
	r22_bb2     = 0x07,
	r23_bb5     = 0x70,
	r23_bb4     = 0x07,
	r24_bb7     = 0x70,
	r24_bb6     = 0x07,

	r39_hsw     = 0xe0,
	r39_hbw     = 0x1f,

	r40_htl89   = 0xc0,
	r40_hdw     = 0x3f,

	r43_vsw     = 0xe0,
	r43_vbw     = 0x1f,

	r44_vsls    = 0x40,
	r44_vdw     = 0x1f,

	r45_vtl8    = 0x80,
	r45_tres    = 0x40,
	r45_vdp     = 0x1f
};


// TODO: move these into enums
// R#7(md)
#define MD_2PLANE_8BIT      0x00
#define MD_2PLANE_16BIT     0x01
#define MD_1PLANE_16COLOUR  0x02
#define MD_1PLANE_256COLOUR 0x03
#define MD_1PLANE           (MD_1PLANE_16COLOUR & MD_1PLANE_256COLOUR)
#define MD_SHIFT            0
#define MD_MASK             0x03

// R#8
#define PGS_64X32         0x0
#define PGS_32X64         0x1
#define PGS_SHIFT         0
#define PGS_MASK          0x01

// R#9
#define SLV_SCREEN        0x00
#define SLV_8             0x04
#define SLV_16            0x05
#define SLV_32            0x06
#define SLV_64            0x07
#define SLH_SCREEN        0x00
#define SLH_8             0x04
#define SLH_16            0x05
#define SLH_32            0x06
#define SLH_64            0x07
#define PTS_8X8           0x00
#define PTS_16X16         0x01
#define PTS_32X32         0x02
#define PTS_64X64         0x03

// R#10
#define SPAS_SPRITESIZE    false
#define SPAS_SPRITEREVERSE true

// R#10(spas)=1
#define SZ_8X8            0x00
#define SZ_16X16          0x01
#define SZ_32X32          0x02
#define SZ_64X64          0x03

// R#10(spas)=0
#define SZ_NOREVERSE      0x00
#define SZ_VERTREVERSE    0x01
#define SZ_HORIZREVERSE   0x02
#define SZ_BOTHREVERSE    0x03

// R#11(prm)
#define PRM_SABDEX        0x00
#define PRM_ASBDEX        0x01
#define PRM_SEABDX        0x02
#define PRM_ASEBDX        0x03

// R#40
#define HDW_SHIFT         0
#define HDW_MASK          0x3f

// R#44
#define VDW_SHIFT         0
#define VDW_MASK          0x3f

#define _ENABLE_SPRITES
#define _ENABLE_SCROLLX
#define _ENABLE_SCROLLY
//#define _ENABLE_SCREEN_RESIZE
//#define _SHOW_VIDEO_DEBUG

#define GFX_8X8_4BIT    0
#define GFX_16X16_4BIT  1
#define GFX_32X32_4BIT  2
#define GFX_64X64_4BIT  3
#define GFX_8X8_8BIT    4
#define GFX_16X16_8BIT  5



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

// device type definition
DEFINE_DEVICE_TYPE(YGV608, ygv608_device, "ygv608", "YGV608 VDP")

/* text-layer characters */

static const uint32_t pts_4bits_layout_xoffset[64] =
{
	STEP8( 0*256, 4 ), STEP8( 1*256, 4 ), STEP8( 4*256, 4 ), STEP8( 5*256, 4 ),
	STEP8( 16*256, 4 ), STEP8( 17*256, 4 ), STEP8( 20*256, 4 ), STEP8( 21*256, 4 )
};

static const uint32_t pts_4bits_layout_yoffset[64] =
{
	STEP8( 0*256, 8*4 ), STEP8( 2*256, 8*4 ), STEP8( 8*256, 8*4 ), STEP8( 10*256, 8*4 ),
	STEP8( 32*256, 8*4 ), STEP8( 34*256, 8*4 ), STEP8( 40*256, 8*4 ), STEP8( 42*256, 8*4 )
};

static const gfx_layout pts_8x8_4bits_layout =
{
	8,8,          /* 8*8 pixels */
	RGN_FRAC(1,1),        /* 65536 patterns */
	4,            /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	EXTENDED_XOFFS,
	EXTENDED_YOFFS,
	8*8*4,
	pts_4bits_layout_xoffset,
	pts_4bits_layout_yoffset
};

static const gfx_layout pts_16x16_4bits_layout =
{
	16,16,        /* 16*16 pixels */
	RGN_FRAC(1,1),        /* 16384 patterns */
	4,            /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	EXTENDED_XOFFS,
	EXTENDED_YOFFS,
	16*16*4,
	pts_4bits_layout_xoffset,
	pts_4bits_layout_yoffset
};

static const gfx_layout pts_32x32_4bits_layout =
{
	32,32,        /* 32*32 pixels */
	RGN_FRAC(1,1),         /* 4096 patterns */
	4,            /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	EXTENDED_XOFFS,
	EXTENDED_YOFFS,
	32*32*4,
	pts_4bits_layout_xoffset,
	pts_4bits_layout_yoffset
};

static const gfx_layout pts_64x64_4bits_layout =
{
	64,64,        /* 32*32 pixels */
	RGN_FRAC(1,1),         /* 1024 patterns */
	4,            /* 4 bits per pixel */
	{ 0, 1, 2, 3 },
	EXTENDED_XOFFS,
	EXTENDED_YOFFS,
	64*64*4,
	pts_4bits_layout_xoffset,
	pts_4bits_layout_yoffset
};


static const gfx_layout pts_8x8_8bits_layout =
{
	8,8,          /* 8*8 pixels */
	RGN_FRAC(1,1),        /* 32768 patterns */
	8,            /* 8 bits per pixel */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ STEP8( 0*512, 8 ) },
	{ STEP8( 0*512, 8*8 ) },
	8*8*8
};

static const gfx_layout pts_16x16_8bits_layout =
{
	16,16,        /* 16*16 pixels */
	RGN_FRAC(1,1),         /* 8192 patterns */
	8,            /* 8 bits per pixel */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ STEP8( 0*512, 8 ), STEP8( 1*512, 8 ) },
	{ STEP8( 0*512, 8*8 ), STEP8( 2*512, 8*8 ) },
	16*16*8
};

static GFXDECODE_START( ygv608 )
	GFXDECODE_DEVICE( DEVICE_SELF, 0x00000000, pts_8x8_4bits_layout,    0,  16 )
	GFXDECODE_DEVICE( DEVICE_SELF, 0x00000000, pts_16x16_4bits_layout,  0,  16 )
	GFXDECODE_DEVICE( DEVICE_SELF, 0x00000000, pts_32x32_4bits_layout,  0,  16 )
	GFXDECODE_DEVICE( DEVICE_SELF, 0x00000000, pts_64x64_4bits_layout,  0,  16 )
	GFXDECODE_DEVICE( DEVICE_SELF, 0x00000000, pts_8x8_8bits_layout,    0,   1 )
	GFXDECODE_DEVICE( DEVICE_SELF, 0x00000000, pts_16x16_8bits_layout,  0,   1 )
GFXDECODE_END

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

/***************************************
 *
 * Internal I/O register structure
 *
 ***************************************/

 // we use decimals here to match documentation
static ADDRESS_MAP_START( regs_map, AS_IO, 8, ygv608_device )

	// address pointers
	AM_RANGE( 0,  0) AM_READWRITE(pattern_name_table_y_r,pattern_name_table_y_w)
	AM_RANGE( 1,  1) AM_READWRITE(pattern_name_table_x_r,pattern_name_table_x_w)

	AM_RANGE( 2,  2) AM_READWRITE(ram_access_ctrl_r,ram_access_ctrl_w)
	
	AM_RANGE( 3,  3) AM_READWRITE(sprite_address_r,sprite_address_w)
	AM_RANGE( 4,  4) AM_READWRITE(scroll_address_r,scroll_address_w)
	AM_RANGE( 5,  5) AM_READWRITE(palette_address_r,palette_address_w)
	AM_RANGE( 6,  6) AM_READWRITE(sprite_bank_r,sprite_bank_w)
	
	// screen control
	AM_RANGE( 7,  7) AM_READWRITE(screen_ctrl_7_r,  screen_ctrl_7_w)
	AM_RANGE( 8,  8) AM_READWRITE(screen_ctrl_8_r,  screen_ctrl_8_w)
	AM_RANGE( 9,  9) AM_READWRITE(screen_ctrl_9_r,  screen_ctrl_9_w)
	AM_RANGE(10, 10) AM_READWRITE(screen_ctrl_10_r, screen_ctrl_10_w)
	AM_RANGE(11, 11) AM_READWRITE(screen_ctrl_11_r, screen_ctrl_11_w)
	AM_RANGE(12, 12) AM_READWRITE(screen_ctrl_12_r, screen_ctrl_12_w)

	AM_RANGE(13, 13) AM_WRITE(border_color_w)
	// interrupt section
	AM_RANGE(14, 14) AM_READWRITE(irq_mask_r,irq_mask_w)
	AM_RANGE(15, 16) AM_READWRITE(irq_ctrl_r,irq_ctrl_w)
	// base address
	AM_RANGE(17, 24) AM_WRITE(base_address_w)

	// ROZ parameters
	AM_RANGE(25, 27) AM_WRITE(roz_ax_w)
	AM_RANGE(28, 29) AM_WRITE(roz_dx_w)
	AM_RANGE(30, 31) AM_WRITE(roz_dxy_w)
	AM_RANGE(32, 34) AM_WRITE(roz_ay_w)
	AM_RANGE(35, 36) AM_WRITE(roz_dy_w)
	AM_RANGE(37, 38) AM_WRITE(roz_dyx_w)

	// CRTC
	AM_RANGE(39, 46) AM_WRITE(crtc_w)
ADDRESS_MAP_END

/***************************************
 *
 * Port Interface map
 *
 ***************************************/

DEVICE_ADDRESS_MAP_START( port_map, 8, ygv608_device )
	AM_RANGE(0x00, 0x00) AM_READWRITE(pattern_name_table_r,pattern_name_table_w)
	AM_RANGE(0x01, 0x01) AM_READWRITE(sprite_data_r,sprite_data_w)
	AM_RANGE(0x02, 0x02) AM_READWRITE(scroll_data_r,scroll_data_w)
	AM_RANGE(0x03, 0x03) AM_READWRITE(palette_data_r,palette_data_w)
	AM_RANGE(0x04, 0x04) AM_READWRITE(register_data_r,register_data_w)
	AM_RANGE(0x05, 0x05) AM_WRITE(register_select_w)
	AM_RANGE(0x06, 0x06) AM_READWRITE(status_port_r,status_port_w)
	AM_RANGE(0x07, 0x07) AM_READWRITE(system_control_r,system_control_w)
ADDRESS_MAP_END


//-------------------------------------------------
//  ygv608_device - constructor
//-------------------------------------------------

ygv608_device::ygv608_device( const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock )
	: device_t(mconfig, YGV608, tag, owner, clock),
	  device_gfx_interface(mconfig, *this, GFXDECODE_NAME(ygv608)),
	  device_memory_interface(mconfig, *this),
	  m_io_space_config("io", ENDIANNESS_BIG, 8, 6, 0, *ADDRESS_MAP_NAME(regs_map)),
	  m_vblank_handler(*this),
	  m_raster_handler(*this)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------
void ygv608_device::device_start()
{
	memset(&m_ports, 0, sizeof(m_ports));
	memset(&m_regs, 0, sizeof(m_regs));
	memset(&m_pattern_name_table, 0, sizeof(m_pattern_name_table));
	memset(&m_sprite_attribute_table, 0, sizeof(m_sprite_attribute_table));

	memset(&m_scroll_data_table, 0, sizeof(m_scroll_data_table));
	memset(&m_colour_palette, 0, sizeof(m_colour_palette));

	m_bits16 = 0;
	m_page_x = 0;
	m_page_y = 0;
	m_pny_shift = 0;
	m_na8_mask = 0;
	m_col_shift = 0;

	m_ax = 0; m_dx = 0; m_dxy = 0; m_ay = 0; m_dy = 0; m_dyx = 0;

	memset(&m_base_addr, 0, sizeof(m_base_addr));
	m_base_y_shift = 0;

	// flag rebuild of the tilemaps
	m_screen_resize = 1;
	m_tilemap_resize = 1;
	m_namcond1_gfxbank = 0;
	save_item(NAME(m_namcond1_gfxbank));

	/* create tilemaps of all sizes and combinations */
	m_tilemap_A_cache_8[0] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_A_8),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  8,8, 32,32);
	m_tilemap_A_cache_8[1] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_A_8),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  8,8, 64,32);
	m_tilemap_A_cache_8[2] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_A_8),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  8,8, 32,64);

	m_tilemap_A_cache_16[0] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_A_16),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  16,16, 32,32);
	m_tilemap_A_cache_16[1] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_A_16),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  16,16, 64,32);
	m_tilemap_A_cache_16[2] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_A_16),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  16,16, 32,64);

	m_tilemap_B_cache_8[0] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_B_8),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  8,8, 32,32);
	m_tilemap_B_cache_8[1] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_B_8),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  8,8, 64,32);
	m_tilemap_B_cache_8[2] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_B_8),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  8,8, 32,64);

	m_tilemap_B_cache_16[0] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_B_16),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  16,16, 32,32);
	m_tilemap_B_cache_16[1] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_B_16),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  16,16, 64,32);
	m_tilemap_B_cache_16[2] = &machine().tilemap().create(*this, tilemap_get_info_delegate(FUNC(ygv608_device::get_tile_info_B_16),this), tilemap_mapper_delegate(FUNC(ygv608_device::get_tile_offset),this),  16,16, 32,64);

	m_tilemap_A = nullptr;
	m_tilemap_B = nullptr;

	m_iospace = &space(AS_IO);

	// TODO: tagging configuration
	m_screen = downcast<screen_device *>(machine().device("screen"));
	m_vblank_handler.resolve();
	m_raster_handler.resolve();
	m_vblank_timer = timer_alloc(VBLANK_TIMER);
	m_raster_timer = timer_alloc(RASTER_TIMER);

	register_state_save();
}

//-------------------------------------------------
//  memory_space_config - return a description of
//  any address spaces owned by this device
//-------------------------------------------------

device_memory_interface::space_config_vector ygv608_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(AS_IO, &m_io_space_config)
	};
}

inline void ygv608_device::vblank_irq_check()
{
	if(m_vblank_irq_mask == true && m_screen_status & 8)
		m_vblank_handler(ASSERT_LINE);
}

inline void ygv608_device::raster_irq_check()
{
	if(m_raster_irq_mask == true && m_screen_status & 0x10)
		m_raster_handler(ASSERT_LINE);
}


void ygv608_device::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	switch(id)
	{
		case VBLANK_TIMER:
		{
			m_screen_status |= 8; // FV
			vblank_irq_check();
			break;
		}
		case RASTER_TIMER:
		{
			m_screen_status |= 0x10; // FP
			raster_irq_check();

			// adjust for next one shot
			m_raster_timer->reset();
			m_raster_timer->adjust(raster_sync_offset(), 0);
			break;
		}
	}
}


void ygv608_device::set_gfxbank(uint8_t gfxbank)
{
	m_namcond1_gfxbank = gfxbank;
	m_tilemap_resize = 1;
}

inline int ygv608_device::get_col_division(int raw_col)
{
	if((m_v_div_size & 4) == 0)
		return 0;
	
	return ((raw_col >> m_col_shift) * 2) & 0x7f;
}


TILEMAP_MAPPER_MEMBER( ygv608_device::get_tile_offset )
{
	// this optimisation is not much good to us,
	// since we really need row,col in the get_tile_info() routines
	// - so just pack them into a uint32_t

	return( ( col << 6 ) | row );
}

#define layout_total(x) \
(gfx(x)->elements())

TILE_GET_INFO_MEMBER( ygv608_device::get_tile_info_A_8 )
{
	// extract row,col packed into tile_index
	int             col = tile_index >> 6;
	int             row = tile_index & 0x3f;
	int translated_column = get_col_division(col);
	
	uint8_t   attr = 0;
	int             pattern_name_base = 0;
	int             set = (m_md == MD_1PLANE_256COLOUR
						? GFX_8X8_8BIT : GFX_8X8_4BIT );
	int             base = row >> m_base_y_shift;

	if( col >= m_page_x )
	{
		SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else if( row >= m_page_y )
	{
		SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else
	{
		int sx, sy, page;
		int i = pattern_name_base + (((row << m_pny_shift) + col) << m_bits16);
		int j = m_pattern_name_table[i];
		int f = 0;

		if( m_bits16 )
		{
			j += ((int)(m_pattern_name_table[i+1] & m_na8_mask )) << 8;
			// attribute only valid in 16 color mode
			if (set == GFX_8X8_4BIT)
				attr = m_pattern_name_table[i+1] >> 4;

			if (m_flip == true)
			{
				if (m_pattern_name_table[i+1] & (1<<3)) f |= TILE_FLIPX;
				if (m_pattern_name_table[i+1] & (1<<2)) f |= TILE_FLIPY;
			}
		}

		/* calculate page according to scroll data */
		/* - assuming full-screen scroll only for now... */
		sy = (int)m_scroll_data_table[0][translated_column] +
			(((int)m_scroll_data_table[0][translated_column+1] & 0x0f ) << 8);
		sx = (int)m_scroll_data_table[0][0x80] +
			(((int)m_scroll_data_table[0][0x81] & 0x0f ) << 8);

		if (m_md == MD_2PLANE_16BIT)
		{
			page = ( ( sx + col * 8 ) % 1024 ) / 256;
			page += ( ( ( sy + row * 8 ) % 2048 ) / 256 ) * 4;
		}
		else if (m_page_size)
		{
			page = ( ( sx + col * 8 ) % 2048 ) / 512;
			page += ( ( ( sy + row * 8 ) % 2048 ) / 256 ) * 4;
		}
		else
		{
			page = ( ( sx + col * 8 ) % 2048 ) / 256;
			page += ( ( ( sy + row * 8 ) % 2048 ) / 512 ) * 8;
		}

		page &= 0x1f;
		
		/* add page, base address to pattern name */
		j += ( (int)m_scroll_data_table[0][0xc0+page] << 10 );
		j += ( m_base_addr[0][base] << 8 );

		if( j >= layout_total(set) )
		{
			logerror( "A_8X8: tilemap=%d\n", j );
			j = 0;
		}
		if (m_planeA_color_fetch != 0)
		{
			// attribute only valid in 16 color mode
			if( set == GFX_8X8_4BIT )
				attr = ( j >> ( (m_planeA_color_fetch - 1 ) * 2 ) ) & 0x0f;
		}
		// banking
		if (set == GFX_8X8_4BIT)
		{
			j += m_namcond1_gfxbank * 0x10000;
		}
		else // 8x8x8
		{
			j += m_namcond1_gfxbank * 0x8000;
		}

		SET_TILE_INFO_MEMBER(set, j, attr & 0x0F, f );
	}
}

TILE_GET_INFO_MEMBER( ygv608_device::get_tile_info_B_8 )
{
	// extract row,col packed into tile_index
	int             col = tile_index >> 6;
	int             row = tile_index & 0x3f;
	int translated_column = get_col_division(col);

	uint8_t   attr = 0;
	int             pattern_name_base = ( ( m_page_y << m_pny_shift )
						<< m_bits16 );
	int             set = GFX_8X8_4BIT;
	int             base = row >> m_base_y_shift;

	if (m_md & MD_1PLANE )
	{
		SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else if (col >= m_page_x)
	{
		SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else if (row >= m_page_y)
	{
		SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else
	{
		int sx, sy, page;
		int i = pattern_name_base + (((row << m_pny_shift) + col) << m_bits16);
		int j = m_pattern_name_table[i];
		int f = 0;

		if (m_bits16)
		{
			j += ((int)(m_pattern_name_table[i+1] & m_na8_mask )) << 8;
			attr = m_pattern_name_table[i+1] >> 4; /*& 0x00; 0xf0;*/

			if (m_flip == true)
			{
				if (m_pattern_name_table[i+1] & (1<<3)) f |= TILE_FLIPX;
				if (m_pattern_name_table[i+1] & (1<<2)) f |= TILE_FLIPY;
			}
		}

		/* calculate page according to scroll data */
		/* - assuming full-screen scroll only for now... */
		sy = (int)m_scroll_data_table[1][translated_column] +
			(((int)m_scroll_data_table[1][translated_column+1] & 0x0f ) << 8);
		sx = (int)m_scroll_data_table[1][0x80] +
			(((int)m_scroll_data_table[1][0x81] & 0x0f ) << 8);

		if (m_md == MD_2PLANE_16BIT )
		{
			page = ( ( sx + col * 8 ) % 1024 ) / 256;
			page += ( ( ( sy + row * 8 ) % 2048 ) / 256 ) * 4;
		}
		else if (m_page_size)
		{
			page = ( ( sx + col * 8 ) % 2048 ) / 512;
			page += ( ( ( sy + row * 8 ) % 2048 ) / 256 ) * 4;
		}
		else
		{
			page = ( ( sx + col * 8 ) % 2048 ) / 256;
			page += ( ( ( sy + row * 8 ) % 2048 ) / 512 ) * 8;
		}

		page &= 0x1f;

		/* add page, base address to pattern name */
		j += ( (int)m_scroll_data_table[1][0xc0+page] << 10 );
		j += ( m_base_addr[1][base] << 8 );

		if( j >= layout_total(set) )
		{
			logerror( "B_8X8: tilemap=%d\n", j );
			j = 0;
		}
		if (m_planeB_color_fetch != 0)
		{
			uint8_t color = (m_planeB_color_fetch);

			/* assume 16 colour mode for now... */
			attr = ( j >> ( (color - 1 ) * 2 ) ) & 0x0f;
		}

		// banking
		if (set == GFX_8X8_4BIT)
		{
			j += m_namcond1_gfxbank * 0x10000;
		}
		else // 8x8x8
		{
			j += m_namcond1_gfxbank * 0x8000;
		}

		SET_TILE_INFO_MEMBER(set, j, attr, f );
	}
}

TILE_GET_INFO_MEMBER( ygv608_device::get_tile_info_A_16 )
{
	// extract row,col packed into tile_index
	int             col = tile_index >> 6;
	int             row = tile_index & 0x3f;
	int translated_column = get_col_division(col);

	uint8_t   attr = 0;
	int             pattern_name_base = 0;
	int             set = (m_md == MD_1PLANE_256COLOUR
						? GFX_16X16_8BIT : GFX_16X16_4BIT );
	int             base = row >> m_base_y_shift;

	if( col >= m_page_x ) {
	SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else if( row >= m_page_y ) {
	SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else {
	int sx, sy, page;
	int j;
	int i = ( ( ( row << m_pny_shift ) + col ) << m_bits16 );
	int f = 0;
	i += pattern_name_base;

	j = m_pattern_name_table[i];
	if( m_bits16 ) {
		j += ((int)(m_pattern_name_table[i+1] & m_na8_mask )) << 8;
		// attribute only valid in 16 color mode
		if( set == GFX_16X16_4BIT )
			attr = m_pattern_name_table[i+1] >> 4;

		if (m_flip == true)
		{
		if (m_pattern_name_table[i+1] & (1<<3)) f |= TILE_FLIPX;
		if (m_pattern_name_table[i+1] & (1<<2)) f |= TILE_FLIPY;
		}
	}

	/* calculate page according to scroll data */
	/* - assuming full-screen scroll only for now... */
	sy = (int)m_scroll_data_table[0][translated_column] +
			(((int)m_scroll_data_table[0][translated_column+1] & 0x0f ) << 8);
	sx = (int)m_scroll_data_table[0][0x80] +
			(((int)m_scroll_data_table[0][0x81] & 0x0f ) << 8);
	if(m_md == MD_2PLANE_16BIT ) {
		page = ( ( sx + col * 16 ) % 2048 ) / 512;
		page += ( ( sy + row * 16 ) / 512 ) * 4;
	}
	else if (m_page_size) {
		page = ( sx + col * 16 ) / 512;
		page += ( ( sy + row * 16 ) / 1024 ) * 8;
	}
	else {
		page = ( sx + col * 16 ) / 1024;
		page += ( ( sy + row * 16 ) / 512 ) * 4;
	}

	page &= 0x1f;

	/* add page, base address to pattern name */
	j += ( (int)m_scroll_data_table[0][0xc0+page] << 8 );
	j += ( m_base_addr[0][base] << 8 );

	if( j >= layout_total(set) ) {
	logerror( "A_16X16: tilemap=%d\n", j );
		j = 0;
	}

	if (m_planeA_color_fetch != 0)
	{
		// attribute only valid in 16 color mode
		if( set == GFX_16X16_4BIT )
			attr = ( j >> ( m_planeA_color_fetch * 2 ) ) & 0x0f;
	}

	// banking
	if (set == GFX_16X16_4BIT)
	{
		j += m_namcond1_gfxbank * 0x4000;
	}
	else // 8x8x8
	{
		j += m_namcond1_gfxbank * 0x2000;
	}


	SET_TILE_INFO_MEMBER(set, j, attr, f );
	}
}

TILE_GET_INFO_MEMBER( ygv608_device::get_tile_info_B_16 )
{
	// extract row,col packed into tile_index
	int             col = tile_index >> 6;
	int             row = tile_index & 0x3f;
	int translated_column = get_col_division(col);

	uint8_t   attr = 0;
	int             pattern_name_base = ( ( m_page_y << m_pny_shift )
					<< m_bits16 );
	int             set = GFX_16X16_4BIT;
	int             base = row >> m_base_y_shift;

	if(m_md & MD_1PLANE ) {
	SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	if( col >= m_page_x ) {
	SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else if( row >= m_page_y ) {
	SET_TILE_INFO_MEMBER(set, 0, 0, 0 );
	}
	else {
	int sx, sy, page;
	int j;
	int i = ( ( ( row << m_pny_shift ) + col ) << m_bits16 );
	int f = 0;
	i += pattern_name_base;

	j = m_pattern_name_table[i];
	if( m_bits16 ) {
		j += ((int)(m_pattern_name_table[i+1] & m_na8_mask )) << 8;
		attr = m_pattern_name_table[i+1] >> 4; /*& 0x00; 0xf0;*/

		if (m_flip == true)
		{
			if (m_pattern_name_table[i+1] & (1<<3)) f |= TILE_FLIPX;
			if (m_pattern_name_table[i+1] & (1<<2)) f |= TILE_FLIPY;
		}
	}

	/* calculate page according to scroll data */
	/* - assuming full-screen scroll only for now... */
	sy = (int)m_scroll_data_table[1][translated_column] +
			(((int)m_scroll_data_table[1][translated_column+1] & 0x0f ) << 8);
	sx = (int)m_scroll_data_table[1][0x80] +
			(((int)m_scroll_data_table[1][0x81] & 0x0f ) << 8);
	if(m_md == MD_2PLANE_16BIT ) {
		page = ( ( sx + col * 16 ) % 2048 ) / 512;
		page += ( ( sy + row * 16 ) / 512 ) * 4;
	}
	else if (m_page_size) {
		page = ( sx + col * 16 ) / 512;
		page += ( ( sy + row * 16 ) / 1024 ) * 8;
	}
	else {
		page = ( sx + col * 16 ) / 1024;
		page += ( ( sy + row * 16 ) / 512 ) * 4;
	}

	page &= 0x1f;

	/* add page, base address to pattern name */
	j += ( (int)m_scroll_data_table[1][0xc0+page] << 8 );
	j += ( m_base_addr[1][base] << 8 );

	if( j >= layout_total(set) ) {
	logerror( "B_16X16: tilemap=%d\n", j );
		j = 0;
	}

	if (m_planeB_color_fetch != 0)
	{
		uint8_t color = (m_planeB_color_fetch);

		/* assume 16 colour mode for now... */
		attr = ( j >> (color * 2)) & 0x0f;
	}

	// banking
	if (set == GFX_16X16_4BIT)
	{
		j += m_namcond1_gfxbank * 0x4000;
	}
	else // 8x8x8
	{
		j += m_namcond1_gfxbank * 0x2000;
	}

	SET_TILE_INFO_MEMBER(set, j, attr, f );
	}
}

void ygv608_device::postload()
{
	m_screen_resize = 1;
	m_tilemap_resize = 1;
}

void ygv608_device::register_state_save()
{
	save_item(NAME(m_ports.b));
	save_item(NAME(m_regs.b));
	save_item(NAME(m_pattern_name_table));
	save_item(NAME(m_sprite_attribute_table.b));
	save_item(NAME(m_scroll_data_table));
	save_item(NAME(m_colour_palette));
//  save_item(NAME(register_state_save));
	save_item(NAME(m_color_state_r));
	save_item(NAME(m_color_state_w));
	// TODO: register save for the newly added variables
	
	machine().save().register_postload(save_prepost_delegate(FUNC(ygv608_device::postload), this));
}


void ygv608_device::draw_sprites(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
#ifdef _ENABLE_SPRITES
	const int sprite_limits[4] = { 512-8, 512-16, 512-32, 512-64 };
	const uint32_t spritebank_size[4] = { 0x10000, 0x4000, 0x1000, 0x400 };
	const int sprite_shift[4] = { 8, 6, 4, 2 };
	const int sprite_mask[4] = { 0xff, 0xfc, 0xf0, 0xc0 };
	const int spf_shift[4] = { -1, 0, +1, +2 };
	// sprites are always clipped to 512x512
	// - regardless of the visible display dimensions
	rectangle spriteClip(0, 512, 0, 512);

	SPRITE_ATTR *sa;
	int flipx = 0, flipy = 0;
	int i;

	/* ensure that sprites are enabled */
	if( (m_dspe == false ) || (m_sprite_disable == true) )
		return;

	/* draw sprites */
	spriteClip &= cliprect;
	sa = &m_sprite_attribute_table.s[MAX_SPRITES-1];
	for( i=0; i<MAX_SPRITES; i++, sa-- )
	{
		int code, color, sx, sy, size, attr, g_attr, spf;

		color = (sa->attr >> 4) & 0x0f;
		sx = ( (int)(sa->attr & 0x02) << 7 ) | (int)sa->sx;
		sy = ( ( ( (int)(sa->attr & 0x01) << 8 ) | (int)sa->sy ) + 1 ) & 0x1ff;
		attr = (sa->attr & 0x0c) >> 2;
		g_attr = m_sprite_aux_reg & 3;
		spf = m_sprite_color_fetch;

		if (m_sprite_aux_mode == SPAS_SPRITESIZE )
		{
			size = g_attr;
			flipx = (attr & SZ_HORIZREVERSE) != 0;
			flipy = (attr & SZ_VERTREVERSE) != 0;
		}
		else
		{
			size = attr;
			flipx = (g_attr & SZ_HORIZREVERSE) != 0;
			flipy = (g_attr & SZ_VERTREVERSE) != 0;
		}

		// calculate code and apply sprite base address 
		code = ( (int)(m_sprite_bank & sprite_mask[size]) << sprite_shift[size] ) | (int)sa->sn;
		// apply spf to color (invalidates individual attribute bits for color)
		if (spf != 0)
			color = ( code >> ( (spf + spf_shift[size]) * 2 ) ) & 0x0f;
		// check code boundary (TODO: do we really need this?)
		if( code >= layout_total(size) ) 
		{
			logerror( "SZ_%d: sprite=%d\n", size, code );
			code = 0;
		}
		// draw the sprite
		gfx(size)->transpen(bitmap,spriteClip,
			code+m_namcond1_gfxbank*spritebank_size[size],
			color,
			flipx,flipy,
			sx,sy,0x00);
		// draw with wraparound
		if(sx > sprite_limits[size] || sy > sprite_limits[size] )
		{
			gfx(size)->transpen(bitmap,spriteClip,
				code+m_namcond1_gfxbank*spritebank_size[size],
				color,
				flipx,flipy,
				sx-512,sy,0x00);
			gfx(size)->transpen(bitmap,spriteClip,
				code+m_namcond1_gfxbank*spritebank_size[size],
				color,
				flipx,flipy,
				sx,sy-512,0x00);
			gfx(size)->transpen(bitmap,spriteClip,
				code+m_namcond1_gfxbank*spritebank_size[size],
				color,
				flipx,flipy,
				sx-512,sy-512,0x00);
		}
		
	}

#endif
}

#ifdef _SHOW_VIDEO_DEBUG
static const char *const mode[] = {
	"2PLANE_8BIT",
	"2PLANE_16BIT",
	"1PLANE_16COLORS",
	"1PLANE_256COLORS"
};

static const char *const psize[] = { "8x8", "16x16", "32x32", "64x64" };
#endif

inline void ygv608_device::draw_layer_roz(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, tilemap_t *source_tilemap)
{
	//int xc, yc;
	//double r, alpha, sin_theta, cos_theta;
	//const rectangle &visarea = screen.visible_area();

	if( m_zron == true )
	{
		// old code, for reference.
		//xc = m_ax >> 16;
		//yc = m_ay >> 16;
		//r = sqrt( (double)( xc * xc + yc * yc ) );
		//alpha = atan( (double)xc / (double)yc );
		//sin_theta = (double)m_dyx / (double)0x10000;
		//cos_theta = (double)m_dx / (double)0x10000;

		source_tilemap->draw_roz(screen, bitmap, cliprect,
				m_ax, m_ay,
				m_dx, m_dxy, m_dyx, m_dy, m_roz_wrap_disable == false, 0, 0 );
	}
	else
		source_tilemap->draw(screen, bitmap, cliprect, 0, 0 );
}


uint32_t ygv608_device::update_screen(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
#ifdef _SHOW_VIDEO_DEBUG
	char buffer[64];
#endif
#ifdef _ENABLE_SCROLLY
	int col;
#endif
	rectangle finalclip;
	const rectangle &visarea = screen.visible_area();

	// clip to the current bitmap
	finalclip.set(0, screen.width() - 1, 0, screen.height() - 1);
	finalclip &= cliprect;
	// TODO: black/transparent pen if CBDR is 1 and border color is 0
	bitmap.fill(m_border_color, visarea );

	// punt if not initialized
	if (m_page_x == 0 || m_page_y == 0)
	{
		return 0;
	}

	if( m_screen_resize )
	{
		m_work_bitmap.resize(screen.width(), screen.height());

		// reset resize flag
		m_screen_resize = 0;
	}

	if( m_tilemap_resize )
	{
		int index;

		/* based on the page sizes, pick an index */
		if (m_page_x == 64)
			index = 1;
		else if (m_page_y == 64)
			index = 2;
		else
			index = 0;

		if (m_pattern_size == PTS_8X8 )
			m_tilemap_A = m_tilemap_A_cache_8[index];
		else
			m_tilemap_A = m_tilemap_A_cache_16[index];
		m_tilemap_A->mark_all_dirty();

		m_tilemap_A->set_transparent_pen(0);
		// for NCV1 it's sufficient to scroll only columns
		m_tilemap_A->set_scroll_cols(m_page_x);

		if (m_pattern_size == PTS_8X8 )
			m_tilemap_B = m_tilemap_B_cache_8[index];
		else
			m_tilemap_B = m_tilemap_B_cache_16[index];
		m_tilemap_B->mark_all_dirty();

		// for NCV1 it's sufficient to scroll only columns
		m_tilemap_B->set_scroll_cols(m_page_x);

		// now clear the screen in case we change to 1-plane mode
		m_work_bitmap.fill(0, finalclip );

		// reset resize flag
		m_tilemap_resize = 0;
	}

#ifdef _ENABLE_SCROLLY

	for( col=0; col<m_page_x; col++ )
	{
		int translated_column = get_col_division(col);
		
		m_tilemap_B->set_scrolly(col,
			( (int)m_scroll_data_table[1][translated_column] +
			( (int)m_scroll_data_table[1][translated_column+1] << 8 ) ) );

		m_tilemap_A->set_scrolly(col,
			( (int)m_scroll_data_table[0][translated_column] +
			( (int)m_scroll_data_table[0][translated_column+1] << 8 ) ) );
	}

#endif

#ifdef _ENABLE_SCROLLX

	m_tilemap_B->set_scrollx(0,
				( (int)m_scroll_data_table[1][0x80] +
					( (int)m_scroll_data_table[1][0x81] << 8 ) ) );

	m_tilemap_A->set_scrollx(0,
				( (int)m_scroll_data_table[0][0x80] +
				( (int)m_scroll_data_table[0][0x81] << 8 ) ) );

#endif

	m_tilemap_A->enable(m_dspe == true);
	if(m_md & MD_1PLANE )
		m_tilemap_B->enable(0);
	else
		m_tilemap_B->enable(m_dspe == true);

	m_tilemap_A->mark_all_dirty();
	m_tilemap_B->mark_all_dirty();


	/*
	 *    now we can render the screen
	 */

	// LBO - need to implement proper pen marking for sprites as well as set aside a non-transparent
	// pen to be used for background fills when plane B is disabled.
	if (m_md & MD_1PLANE)
	{
		// If the background tilemap is disabled, we need to clear the bitmap to black
		m_work_bitmap.fill(0, finalclip);
//      m_work_bitmap.fill(1, *visarea);
	}
	else
	{
		draw_layer_roz(screen, m_work_bitmap, finalclip, m_tilemap_B);

		if(m_planeB_trans_enable == true)
			copybitmap_trans( bitmap, m_work_bitmap, 0, 0, 0, 0, finalclip, 0);
		else
			copybitmap( bitmap, m_work_bitmap, 0, 0, 0, 0, finalclip);
	}

	// for some reason we can't use an opaque m_tilemap_A
	// so use a transparent but clear the work bitmap first
	// - look at why this is the case?!?
	m_work_bitmap.fill(0, visarea );

	if (m_priority_mode == PRM_ASBDEX ||
		m_priority_mode == PRM_ASEBDX )
		draw_sprites(bitmap, finalclip);

	draw_layer_roz(screen, m_work_bitmap, finalclip, m_tilemap_A);
	
	if(m_planeA_trans_enable == true)
		copybitmap_trans( bitmap, m_work_bitmap, 0, 0, 0, 0, finalclip, 0);
	else
		copybitmap( bitmap, m_work_bitmap, 0, 0, 0, 0, finalclip);

	if (m_priority_mode == PRM_SABDEX ||
		m_priority_mode == PRM_SEABDX)
		draw_sprites(bitmap,finalclip );


#ifdef _SHOW_VIDEO_DEBUG
	/* show screen control information */
	ui_draw_text( mode[m_md], 0, 0 );
	sprintf( buffer, "%02ux%02u", m_page_x, m_page_y );
	ui_draw_text( buffer, 0, 16 );
	ui_draw_text( psize[m_pattern_size], 0, 32 );
	sprintf( buffer, "A: SX:%d SY:%d",
		(int)m_scroll_data_table[0][0x80] +
		( ( (int)m_scroll_data_table[0][0x81] & 0x0f ) << 8 ),
		(int)m_scroll_data_table[0][0x00] +
		( ( (int)m_scroll_data_table[0][0x01] & 0x0f ) << 8 ) );
	ui_draw_text( buffer, 0, 48 );
	sprintf( buffer, "B: SX:%d SY:%d",
		(int)m_scroll_data_table[1][0x80] +
		( ( (int)m_scroll_data_table[1][0x81] & 0x0f ) << 8 ),
		(int)m_scroll_data_table[1][0x00] +
		( ( (int)m_scroll_data_table[1][0x01] & 0x0f ) << 8 ) );
	ui_draw_text( buffer, 0, 64 );
#endif
	return 0;
}

/***************************************
 *
 * Port Interface routines
 *
 ****************************************/

 // P#0R - pattern name table data port
READ8_MEMBER( ygv608_device::pattern_name_table_r )
{
	int pn = 0;

	switch (p0_state_r)
	{
		case 0:
			/* Are we reading from plane B? */
			if (!(m_md & MD_1PLANE) && (m_plane_select_access == true))
				pattern_name_base_r = ((m_page_y << m_pny_shift) << m_bits16);

			/* read character from ram */
			pn = pattern_name_base_r + (((m_ytile_ptr << m_pny_shift) + m_xtile_ptr) << m_bits16);
			break;

		case 1:
			/* read character from ram */
			pn = pattern_name_base_r + (((m_ytile_ptr << m_pny_shift) + m_xtile_ptr) << m_bits16) + 1;
			break;
	}

	if (pn > 4095)
	{
		logerror( "attempt (%d) to read pattern name %d\n"
			"mode = %d, pgs = %d (%dx%d)\n"
			"pattern_name_base_r = %d\n"
			"pnx = %d, pny = %d, pny_shift = %d, bits16 = %d\n",
			p0_state_r,
			pn, m_md, m_page_size,
			m_page_x, m_page_y,
			pattern_name_base_r,
			m_xtile_ptr, m_ytile_ptr, m_pny_shift,
			m_bits16 );
		pn = 0;
	}

	p0_state_r++;
	if (m_md == MD_2PLANE_8BIT )
		p0_state_r++;

	if (p0_state_r == 2)
	{
		pattern_name_autoinc_check();
		p0_state_r = 0;
		pattern_name_base_r = 0;
	}

	return m_pattern_name_table[pn];
}

// P#1R - sprite data port
READ8_MEMBER( ygv608_device::sprite_data_r )
{
	uint8_t res = m_sprite_attribute_table.b[m_sprite_address];

	if (m_saar == true)
		m_sprite_address++;

	return res;
}

// P#2R - scroll data port
READ8_MEMBER( ygv608_device::scroll_data_r )
{
	uint8_t res = m_scroll_data_table[m_ba_plane_select][m_scroll_address];

	if (m_scar == true)
	{
		m_scroll_address++;
		/* handle wrap to next plane */
		if (m_scroll_address == 0)
			m_ba_plane_select ^= 1;
	}

	return res;
}

// P#3 - color palette data port
READ8_MEMBER( ygv608_device::palette_data_r )
{
	uint8_t res = m_colour_palette[m_palette_address][m_color_state_r];

	if( ++m_color_state_r == 3 )
	{
		m_color_state_r = 0;

		if(m_cpar == true)
			m_palette_address++;
	}

	return res;
}

// P#4R - register data port
READ8_MEMBER(ygv608_device::register_data_r)
{
	int regNum = m_register_address & 0x3f;
	uint8_t res = m_regs.b[regNum];

	//m_iospace->read_byte(regNum);


	if (m_register_autoinc_r == true)
	{
		m_register_address ++;
		m_register_address &= 0x3f;
		#if 0
		// we'll catch this in the logerror anyway
		if (regNum == 50)
		{
			regNum = 0;
			logerror( "warning: rn=50 after read increment\n" );
		}
		#endif
	}

	return res;
}

// P#6R - status port
/***
 * ---x ---- FP Specified display position flag (R#15 & 16), reset by writing '1'
 * ---- x--- FV Vertical border interval start, reset by writing '1'
 * ---- -x-- FC Sprite collision flag, reset by writing '1'
 * ---- --x- HB 1 when horizontal border or retrace is in progress (read only)
 * ---- ---x VB 1 when vertical border or retrace is in progress (read only)
 ***/
READ8_MEMBER( ygv608_device::status_port_r )
{
	// TODO: we need to use h/vpos in case of border support instead due of how MAME framework works here.
	return (m_screen_status & 0x1c) | (m_screen->hblank()<<1) | m_screen->vblank();
}

// P#7R - system control port
READ8_MEMBER( ygv608_device::system_control_r )
{
	return (uint8_t)(m_ports.b[7]);
}

// P#0W - pattern name table data write
WRITE8_MEMBER(ygv608_device::pattern_name_table_w)
{
	int pn = 0;

	switch (p0_state_w)
	{
		case 0:
			/* Are we reading from plane B? */
			if (!(m_md & MD_1PLANE) && (m_plane_select_access == true))
				pattern_name_base_w = ((m_page_y << m_pny_shift) << m_bits16);

			/* read character from ram */
			pn = pattern_name_base_w + (((m_ytile_ptr << m_pny_shift) + m_xtile_ptr) << m_bits16);
			break;

		case 1:
			/* read character from ram */
			pn = pattern_name_base_w + (((m_ytile_ptr << m_pny_shift) + m_xtile_ptr) << m_bits16) + 1;
			break;
	}

	if (pn > 4095)
	{
		logerror( "attempt (%d) to read pattern name %d\n"
			"mode = %d, pgs = %d (%dx%d)\n"
			"pattern_name_base_w = %d\n"
			"pnx = %d, pny = %d, pny_shift = %d, bits16 = %d\n",
			p0_state_w,
			pn, m_md, m_page_size,
			m_page_x, m_page_y,
			pattern_name_base_w,
			m_xtile_ptr, m_ytile_ptr, m_pny_shift,
			m_bits16 );
		pn = 0;
	}

	m_pattern_name_table[pn] = data;

	p0_state_w++;
	if (m_md == MD_2PLANE_8BIT )
		p0_state_w++;

	if (p0_state_w == 2)
	{
		pattern_name_autoinc_check();
		p0_state_w = 0;
		pattern_name_base_w = 0;
	}
}

inline void ygv608_device::pattern_name_autoinc_check()
{
	uint8_t xTile = m_xtile_ptr;
	uint8_t yTile = m_ytile_ptr;

	if (m_ytile_autoinc == true)
	{
		// we are incrementing in Y direction
		if (yTile++ == (m_page_y - 1))
		{
			yTile = 0;
			if (xTile++ == (m_page_x - 1))
			{
				xTile = 0;
				m_plane_select_access ^= 1; // flip A/B plane
			}
		}
		m_ytile_ptr = yTile;
		m_xtile_ptr = xTile;
	}
	else if (m_xtile_autoinc == true)
	{
		// we are incrementing in X direction
		if (xTile++ == (m_page_x - 1))
		{
			xTile = 0;
			if (yTile++ == (m_page_y - 1))
			{
				yTile = 0;
				m_plane_select_access ^= 1; // flip A/B plane
			}
		}
		m_ytile_ptr = yTile;
		m_xtile_ptr = xTile;
	}
}

// P#1W - sprite data port
WRITE8_MEMBER( ygv608_device::sprite_data_w )
{
	m_sprite_attribute_table.b[m_sprite_address] = data;

	if( m_saaw == true)
		m_sprite_address++;
}

// P#2W - scroll data port
WRITE8_MEMBER( ygv608_device::scroll_data_w )
{
	m_scroll_data_table[m_ba_plane_select][m_scroll_address] = data;
	
	if (m_scaw == true)
	{
		m_scroll_address++;
		/* handle wrap to next plane */
		if (m_scroll_address == 0)
			m_ba_plane_select ^= 1;
	}
}

// P#3W - colour palette data port
WRITE8_MEMBER( ygv608_device::palette_data_w )
{
	m_colour_palette[m_palette_address][m_color_state_w] = data;
	if (++m_color_state_w == 3)
	{
		m_color_state_w = 0;
//      if(m_colour_palette[m_palette_address][0] & 0x80) // Transparency designation, none of the Namco games enables it?

		palette().set_pen_color(m_palette_address,
			pal6bit( m_colour_palette[m_palette_address][0] ),
			pal6bit( m_colour_palette[m_palette_address][1] ),
			pal6bit( m_colour_palette[m_palette_address][2] ));

		if(m_cpaw == true)
			m_palette_address++;
	}
}

// P#4W - register data port
WRITE8_MEMBER( ygv608_device::register_data_w )
{
	uint8_t regNum = m_register_address & 0x3f;
	//logerror( "R#%d = $%02X\n", regNum, data );

	m_regs.b[regNum] = data;
	m_iospace->write_byte(regNum, data);

	if (m_register_autoinc_w == true)
	{
		m_register_address ++;
		m_register_address &= 0x3f;

		#if 0
		// we'll catch this in the logerror anyway
		if (regNum == 50)
		{
			regNum = 0;
			logerror( "warning: rn=50 after write increment\n" );
		}
		#endif
	}
}

// P#5W - register select port
WRITE8_MEMBER( ygv608_device::register_select_w )
{
	m_register_address = data & 0x3f;
	m_register_autoinc_r = BIT(data,6);
	m_register_autoinc_w = BIT(data,7);
}

// P#6W - status port
WRITE8_MEMBER( ygv608_device::status_port_w )
{
	/* writing a '1' resets that bit */
	m_screen_status &= ~data;

	// send an irq ack to the delegates accordingly
	if(data & 8)
		m_vblank_handler(CLEAR_LINE);
	if(data & 0x10)
		m_raster_handler(CLEAR_LINE);
}

// P#7W - system control port
WRITE8_MEMBER( ygv608_device::system_control_w )
{
	m_ports.b[7] = data;
	if (m_ports.b[7] & 0x3e)
		HandleRomTransfers(data & 0x3e);
	if (m_ports.b[7] & 0x01)
		HandleYGV608Reset();
}


// TODO: actual timing of this
void ygv608_device::HandleYGV608Reset()
{
	/* Clear ports #0-7 */
	memset( &m_ports.b[0], 0, 8 );

	/* Clear registers #0-38, #47-49 */
	memset( &m_regs.b[0], 0, 39 );
	memset( &m_regs.b[47], 0, 3 );

	/* Clear internal ram */
	memset( m_pattern_name_table, 0, 4096 );
	memset( m_sprite_attribute_table.b, 0,
			SPRITE_ATTR_TABLE_SIZE );
	memset( m_scroll_data_table, 0, 2*256 );
	memset( m_colour_palette, 0, 256*3 );
}

/*
    The YGV608 has a function to block-move data from the rom into
    internal tables. This function is not used in NCV1, but I used
    it for testing trojan ROM software.
    - So leave it in!
 */
void ygv608_device::HandleRomTransfers(uint8_t type)
{
	popmessage("ROM DMA used %02x",type);

#if 0
	static uint8_t *sdt = (uint8_t *)m_scroll_data_table;
	static uint8_t *sat = (uint8_t *)m_sprite_attribute_table.b;

	/* fudge copy from sprite data for now... */
	uint8_t *RAM = machine.memory_region[0];
	int i;

	int src = ( ( (int)m_regs.s.tb13 << 8 ) +
			(int)m_regs.s.tb5 ) << 5;
	int bytes = (int)m_regs.s.tn4 << 4;

	logerror( "Transferring data from rom...\n" );

	/* pattern name table */
	if( m_ports.s.tn ) {
	}

	/* scroll table */
	if( m_ports.s.tl ) {
	int dest = (int)m_regs.s.sca;
	if( m_regs.s.p2_b_a )
		dest += 0x100;

	/* fudge a transfer for now... */
	for( i=0; i<bytes; i++ ) {
		sdt[(dest+i)%512] = RAM[src+(i^0x01)];

	}

	/* flag as finished */
	m_ports.s.tl = 0;
	}

	/* sprite attribute table */
	if( m_ports.s.ts ) {
	int dest = (int)m_sprite_address;

	/* fudge a transfer for now... */
	for( i=0; i<bytes; i++ ) {
		sat[(dest+i)%256] = RAM[src+(i^0x01)];

	}

	/* flag as finished */
	m_ports.s.ts = 0;
	}
#endif
}

/***************************************
 *
 * Register Interface routines
 *
 ****************************************/

 // R#0R - Pattern Name Table Access pointer Y
READ8_MEMBER( ygv608_device::pattern_name_table_y_r )
{
	return ((m_xtile_autoinc == true) << 7) | ((m_plane_select_access == true) << 6) | m_xtile_ptr;
}

 // R#0W - Pattern Name Table Access pointer Y
WRITE8_MEMBER( ygv608_device::pattern_name_table_y_w )
{
	m_ytile_ptr = data & 0x3f;
	//if (yTile >= m_page_y)
	//  logerror ("%s:setting pny(%d) >= page_y(%d)\n", machine().describe_context(),
	//      yTile, m_page_y );
	m_ytile_ptr &= m_page_y -1;
	m_ytile_autoinc = BIT(data,7);
	m_plane_select_access = BIT(data,6);
	if(m_ytile_autoinc == true && m_xtile_autoinc == true)
		logerror("%s: Warning both X/Y Tiles autoinc enabled!\n",this->tag());
}

 // R#1R - Pattern Name Table Access pointer X
READ8_MEMBER( ygv608_device::pattern_name_table_x_r )
{
	return ((m_xtile_autoinc == true) << 7) | m_xtile_ptr;
}

 // R#1W - Pattern Name Table Access pointer X
WRITE8_MEMBER( ygv608_device::pattern_name_table_x_w )
{
	m_xtile_ptr = data & 0x3f;
	//if (xTile >= m_page_x)
	//  logerror ("%s:setting pnx(%d) >= page_x(%d)\n", machine().describe_context(),
	//      xTile, m_page_x );
	m_xtile_ptr &= m_page_x -1;
	m_xtile_autoinc = BIT(data,7);
	if(m_ytile_autoinc == true && m_xtile_autoinc == true)
		logerror("%s: Warning both X/Y Tiles autoinc enabled!\n",this->tag());
}

// R#2R - Built in RAM access control
/***
 * x--- ---- CPAW Address autoincrements after color palette write
 * -x-- ---- CPAR Address autoincrements after color palette read
 * ---x ---- B/(A) P#2 plane access select (1=B Plane)
 * ---- x--- SCAW Address autoincrements after scroll data write
 * ---- -x-- SCAR Address autoincrements after scroll data read
 * ---- --x- SAAW Address autoincrements after sprite attribute table write
 * ---- ---x SAAR Address autoincrements after sprite attribute table read 
 ***/
READ8_MEMBER( ygv608_device::ram_access_ctrl_r )
{
	return (m_cpaw<<7) | (m_cpar<<6) |
	       (m_ba_plane_select<<4) | 
		   (m_scaw<<3) | (m_scar<<2) | (m_saaw<<1) | (m_saar<<0);
}

// R#2W - Built in RAM access control
WRITE8_MEMBER( ygv608_device::ram_access_ctrl_w )
{
	m_saar = BIT(data,0);
	m_saaw = BIT(data,1);
	m_scar = BIT(data,2);
	m_scaw = BIT(data,3);
	m_ba_plane_select = BIT(data,4);
	m_cpar = BIT(data,6);
	m_cpaw = BIT(data,7);
}


// R#3R - sprite attribute table access pointer
READ8_MEMBER( ygv608_device::sprite_address_r )
{
	return m_sprite_address;
}

// R#3W - sprite attribute table access pointer
WRITE8_MEMBER( ygv608_device::sprite_address_w )
{
	m_sprite_address = data;
}


 // R#4R - scroll table access pointer
READ8_MEMBER( ygv608_device::scroll_address_r )
{
	return m_scroll_address;
}

 // R#4W - scroll table access pointer
WRITE8_MEMBER( ygv608_device::scroll_address_w )
{
	m_scroll_address = data;
}

 // R#5R - color palette access pointer
READ8_MEMBER( ygv608_device::palette_address_r )
{
	return m_palette_address;
}

 // R#5W - color palette access pointer
WRITE8_MEMBER( ygv608_device::palette_address_w )
{
	m_palette_address = data;
}

// R#6R - sprite generator base address
READ8_MEMBER( ygv608_device::sprite_bank_r )
{
	return m_sprite_bank;
}

// R#6W - sprite generator base address
WRITE8_MEMBER( ygv608_device::sprite_bank_w )
{
	m_sprite_bank = data;
}

// R#7R - screen control 7
/***
 * x--- ---- DCKM dot clock frequency (0 = 1/2 1 = 1/4)
 * -x-- ---- FLIP reverse display in pattern name
 * ---- x--- ZRON enables ROZ features
 * ---- -xx- MDx  planes display mode
 * ---- -11-      1 plane/256 colors (16 bits)
 * ---- -10-      1 plane/16 colors (16 bits)
 * ---- -01-      2 planes/16 bits
 * ---- -00-      2 planes/8 bits
 * ---- ---x DSPE display permission of pattern planes (screen blanked if 0)
 ***/
READ8_MEMBER( ygv608_device::screen_ctrl_7_r )
{
	return (m_dckm<<7)|(m_flip<<6)|
	       (m_zron<<3)|((m_md & 3)<<1)|(m_dspe<<0);
}

// R#7W - screen control 7
WRITE8_MEMBER( ygv608_device::screen_ctrl_7_w )
{
	uint8_t new_md = (data >> 1) & 3;
	if( new_md != m_md)
		m_tilemap_resize = 1;

	m_dckm = BIT(data,7);
	m_flip = BIT(data,6);
	m_zron = BIT(data,3);
	m_md = new_md;
	m_dspe = BIT(data,0);
	
	m_na8_mask = ((m_flip == true) ? 0x03 : 0x0f );
	
	// changing mode resets the pattern name table states (Mappy Arrange)
	p0_state_w = 0;
	p0_state_r = 0;
	pattern_mode_setup();
// TODO: add dot clock into CRTC
//	screen_configure();
}

inline void ygv608_device::pattern_mode_setup()
{	
	m_bits16 = (m_md == MD_2PLANE_8BIT ? 0 : 1 );
	
	if(m_md == MD_2PLANE_16BIT )
		m_page_x = m_page_y = 32;
	else 
	{
		if (m_page_size == false ) 
		{
			m_page_x = 64;
			m_page_y = 32;
		}
		else 
		{
			m_page_x = 32;
			m_page_y = 64;
		}
	}
	m_pny_shift = ( m_page_x == 32 ? 5 : 6 );

	/* bits to shift pattern y coordinate to extract base */
	m_base_y_shift = ( m_page_y == 32 ? 2 : 3 );
}

// R#8R - screen control 8
/***
 * xx-- ---- HDS horizontal display domain size (0=4096, 3=512)
 * --xx ---- VDS vertical display domain size (0=4096, 3=512)
 * ---- x--- RLRT ROZ wraparound disable
 * ---- -x-- RLSC scroll wraparound disable
 * ---- ---x PGS page size (0=64x32, 1=32x64; Mode 2=32x32)
 ***/
READ8_MEMBER( ygv608_device::screen_ctrl_8_r )
{
	return (m_h_display_size<<6)|(m_v_display_size<<4)|
           (m_roz_wrap_disable<<3)|(m_scroll_wrap_disable<<2)|
		   (m_page_size<<0);
}

// R#8W - screen control 8
WRITE8_MEMBER( ygv608_device::screen_ctrl_8_w )
{
	if( (data & 1) != m_page_size)
		m_tilemap_resize = 1;
	
/**/m_h_display_size = (data >> 6) & 3; 
/**/m_v_display_size = (data >> 4) & 3; 
	m_roz_wrap_disable = BIT(data,3);
/**/m_scroll_wrap_disable = BIT(data,2);
	m_page_size = BIT(data,0);
	
	pattern_mode_setup();
}

// R#9R - screen control 9
/***
 * xx-- ---- PTS: pattern size in pattern planes (8x8, 16x16, 32x32, 64x64)
 * --xx x--- SLH: size of horizontal division in screen division scrolling
 * ---- -xxx SLV: size of vertical division in screen division scrolling
 * ---- -111 64 dots division
 * ---- -110 32 dots division
 * ---- -101 16 dots division
 * ---- -100 8 dots division
 * ---- -000 entire screen
 ***/
READ8_MEMBER( ygv608_device::screen_ctrl_9_r )
{
	return (m_pattern_size<<6)|
	       (m_h_div_size<<3)|(m_v_div_size<<0);
}

WRITE8_MEMBER( ygv608_device::screen_ctrl_9_w )
{
	uint8_t new_pts = (data >> 6) & 3;
	
	if(new_pts != m_pattern_size)
		m_tilemap_resize = 1;
	
	m_pattern_size = new_pts;
/**/m_h_div_size = (data >> 3) & 7;
	m_v_div_size = (data >> 0) & 7;

	//popmessage("%02x %02x",m_h_div_size,m_v_div_size);
	
	// TODO: this code is garbage ...
	if(m_v_div_size == 0)
		m_col_shift = 8;
	else
	{
		if (m_pattern_size == PTS_8X8 )
			m_col_shift = (m_v_div_size) - 4;
		else
			m_col_shift = (m_v_div_size) - 5;
		if( m_col_shift < 0 )
		{
			// we can't handle certain conditions
			logerror( "Unhandled slv condition (pts=$%X,slv=$%X)\n",
						m_pattern_size, m_v_div_size);
			m_col_shift = 8;
		}
	}
}
 
// R#10R - screen control 10: mosaic & sprite
/***
 * xx-- ---- SPAx: Auxiliary bits of sprite attribute table (0=8x8 or no flip, 1=16x16 or flipy, 2=32x32 or flipx, 3=64x64 or flipx & y )
 * --x- ---- SPAS: Auxiliary function select (0=SPAx selects sprite size, 1=SPAx selects flipping)
 * ---x ---- SPRD: Sprite display disable
 * ---- xx-- MCBx: Mosaic enable on plane B
 * ---- --xx MCAx: Mosaic enable on plane A
 ***/
READ8_MEMBER( ygv608_device::screen_ctrl_10_r )
{
	return (m_sprite_aux_reg << 6) | ((m_sprite_aux_mode == true) << 5) | ((m_sprite_disable == true) << 4)
								   | (m_mosaic_bplane << 2) | (m_mosaic_aplane & 3);
}

// R#10W - screen control: mosaic & sprite
WRITE8_MEMBER( ygv608_device::screen_ctrl_10_w )
{
	m_sprite_aux_reg = (data & 0xc0) >> 6;
	m_sprite_aux_mode = BIT(data, 5);
	m_sprite_disable = BIT(data, 4);

	// check mosaic
	m_mosaic_bplane = (data & 0xc) >> 2;
	m_mosaic_aplane = data & 3;
	if(m_mosaic_aplane || m_mosaic_bplane)
		popmessage("Mosaic effect %02x %02x",m_mosaic_aplane,m_mosaic_bplane);
}

// R#11R - screen control 11
READ8_MEMBER( ygv608_device::screen_ctrl_11_r )
{
	return (m_scm<<6)|(m_yse<<5)|(m_cbdr<<4)|
           (m_priority_mode<<2)|(m_planeB_trans_enable<<1)|(m_planeA_trans_enable<<0);
}

// R#11W - screen control 11
WRITE8_MEMBER( ygv608_device::screen_ctrl_11_w )
{
/**/m_scm = (data >> 6) & 3;
/**/m_yse = BIT(data,5);
/**/m_cbdr = BIT(data,4);
	m_priority_mode = (data >> 2) & 3;
	m_planeB_trans_enable = BIT(data,1);
	m_planeA_trans_enable = BIT(data,0);
}

// R#12R - screen control 12: color fetch modes
READ8_MEMBER( ygv608_device::screen_ctrl_12_r )
{
	return (m_sprite_color_fetch<<6)|(m_planeB_color_fetch<<3)|(m_planeA_color_fetch<<0);
}

// R#12W - screen control 12: color fetch modes
WRITE8_MEMBER( ygv608_device::screen_ctrl_12_w )
{
	m_sprite_color_fetch = (data >> 6) & 3;
	m_planeB_color_fetch = (data >> 3) & 7;
	m_planeA_color_fetch = (data >> 0) & 7;
}

// R#13W - border color
WRITE8_MEMBER( ygv608_device::border_color_w )
{
	m_border_color = data;
}

// R#14R interrupt mask control
READ8_MEMBER( ygv608_device::irq_mask_r )
{
	return (m_raster_irq_mask << 1) | (m_vblank_irq_mask << 0);
}

// R#14W interrupt mask control
WRITE8_MEMBER( ygv608_device::irq_mask_w )
{
	m_vblank_irq_mask = BIT(data, 0);
	m_raster_irq_mask = BIT(data, 1);

	// check if we have an irq in the queue
	vblank_irq_check();
	raster_irq_check();
}

// R#15R / R#16R raster interrupt control
READ8_MEMBER( ygv608_device::irq_ctrl_r )
{
	uint8_t res;

	if(offset == 0) // R#15
		res = m_raster_irq_vpos & 0xff;
	else // R#16
	{
		res = (m_raster_irq_mode << 7);

		res|= (BIT(m_raster_irq_vpos, 8) << 6);

		res|= (m_raster_irq_hpos / 32) & 0x1f;
	}

	return res;
}

// R#15W / R#16W raster interrupt control
WRITE8_MEMBER( ygv608_device::irq_ctrl_w )
{
	if(offset == 0) // R#15
	{
		m_raster_irq_vpos &= ~0xff;
		m_raster_irq_vpos |= data & 0xff;
	}
	else // R#16
	{
		m_raster_irq_mode = BIT(data,7);

		m_raster_irq_vpos &= ~0x100;
		m_raster_irq_vpos |= BIT(data,6) << 8;

		m_raster_irq_hpos = (data & 0x1f) * 32;
	}

	// reset raster timer
	m_raster_timer->reset();
	m_raster_timer->adjust(raster_sync_offset(), 0);

	//printf("%d %d %d %d %d\n",m_raster_irq_hpos,m_raster_irq_vpos,m_raster_irq_mode,m_crtc.htotal,m_crtc.vtotal);
}

// helper for validating and convert to screen position
attotime ygv608_device::raster_sync_offset()
{

	// don't care if h/v pos is higher than CRTC params (NCV2 POST)
	if(m_raster_irq_hpos >  m_crtc.htotal || m_raster_irq_vpos > m_crtc.vtotal )
		return attotime::never;

	// bail out and throw an error if this happens to be used someday
	if(m_raster_irq_mode == true)
	{
		popmessage("Raster IRQ used with mode = true");
		return attotime::never;
	}

	// TODO: actual sync not taken into account, needs a better test than NCV2 limited case
	return m_screen->time_until_pos(m_raster_irq_vpos,m_raster_irq_hpos);
}

// R#17 / R#24 - base address
/*
 * offset & 4 selects plane B
 * -xxx ---- write to base address + 1
 * ---- -xxx write to base address
 */
WRITE8_MEMBER( ygv608_device::base_address_w )
{
	int plane = offset >> 2;
	int addr = ( offset << 1 ) & 0x07;
	m_base_addr[plane][addr] = data & 0x07;
	m_base_addr[plane][addr+1] = (data >> 4) & 0x7;

	m_tilemap_resize = 1;
}

// R#25W - R#27W - X coordinate of initial value
WRITE8_MEMBER( ygv608_device::roz_ax_w )  { m_ax = roz_convert_raw24(&m_raw_ax,offset,data); }

// R#28W - R#29W - increment of coordinate in X direction
WRITE8_MEMBER( ygv608_device::roz_dx_w )  { m_dx = roz_convert_raw16(&m_raw_dx,offset,data); }

// R#30W - R#31W - increment of coordinate in X direction in movement toward Y direction
WRITE8_MEMBER( ygv608_device::roz_dxy_w ) { m_dxy = roz_convert_raw16(&m_raw_dxy,offset,data); }

// R#32W - R#34W - Y coordinate of initial value
WRITE8_MEMBER( ygv608_device::roz_ay_w )  { m_ay = roz_convert_raw24(&m_raw_ay,offset,data); }

// R#35W - R#36W - increment of coordinate in Y direction
WRITE8_MEMBER( ygv608_device::roz_dy_w )  { m_dy = roz_convert_raw16(&m_raw_dy,offset,data); }

// R#37W - R#38W - increment of coordinate in Y direction in movement toward X direction
WRITE8_MEMBER( ygv608_device::roz_dyx_w ) { m_dyx = roz_convert_raw16(&m_raw_dyx,offset,data); }

// ROZ assign helpers
inline uint32_t ygv608_device::roz_convert_raw24(uint32_t *raw_reg, uint8_t offset, uint8_t data)
{
	const uint32_t roz_data_mask24 = 0x1fffff;
	const uint32_t mem_mask = (0xff << offset*8) ^ ~0;
	uint32_t res;

	// substitute the new byte value into the raw register
	*raw_reg &= mem_mask;
	*raw_reg |= data << offset*8;

	// convert raw to the given register
	res = *raw_reg & roz_data_mask24;
	res <<= 7;
	if( res & 0x08000000 ) res |= 0xf8000000;   // 2s complement

	return res;
}

inline uint32_t ygv608_device::roz_convert_raw16(uint16_t *raw_reg, uint8_t offset, uint8_t data)
{
	const uint16_t roz_data_mask16 = 0x1fff;
	const uint16_t mem_mask = (0xff << offset*8) ^ ~0;
	uint32_t res;

	// substitute the new byte value into the raw register
	*raw_reg &= mem_mask;
	*raw_reg |= data << offset*8;

	// convert raw to the given register
	res = *raw_reg & roz_data_mask16;
	res <<= 7;
	if( res & 0x00080000 ) res |= 0xfff80000;   // 2s complement

	return res;
}

// R#39W - R#46W display scan control write
WRITE8_MEMBER( ygv608_device::crtc_w )
{
	//printf("[%d] <- %02x\n",offset+39,data);

	switch(offset+39)
	{
		case 39:
		{
			m_crtc.display_hsync = ((data >> 5) & 7) * 16;
			m_crtc.border_width = (data & 0x1f) * 16;
			break;
		}

		case 40:
		{
			int new_display_width = (data & 0x3f) * 16;
			
			m_crtc.htotal &= ~0x600;
			m_crtc.htotal |= (data & 0xc0) << 3;

			if(new_display_width != m_crtc.display_width)
				m_screen_resize = 1;
			
			m_crtc.display_width = new_display_width;
			break;
		}

		case 41:
		{
			m_crtc.display_hstart &= ~0x1fe;
			m_crtc.display_hstart |= (data & 0xff) << 1;
			break;
		}

		case 42:
		{
			m_crtc.htotal &= ~0x1fe;
			m_crtc.htotal |= (data & 0xff) << 1;

			//printf("H %d %d %d %d %d\n",m_crtc.htotal,m_crtc.display_hstart,m_crtc.display_width,m_crtc.display_hsync,m_crtc.border_width);
			break;
		}

		case 43:
		{
			m_crtc.display_vsync = (data >> 5) & 7;
			m_crtc.border_height = (data & 0x1f) * 8;
			break;
		}

		case 44:
		{
			int new_display_height = (data & 0x3f) * 8;

			// TODO: VSLS, bit 6
			if(new_display_height != m_crtc.display_height)
				m_screen_resize = 1;
			
			m_crtc.display_height = new_display_height;
			break;
		}

		case 45:
		{
			m_crtc.vtotal &= ~0x100;
			m_crtc.vtotal |= BIT(data,7) << 8;

			// TODO: TRES, bit 6

			m_crtc.display_vstart = data & 0x3f;
			break;
		}

		case 46:
		{
			m_crtc.vtotal &= ~0xff;
			m_crtc.vtotal |= data & 0xff;

			// TODO: call it for all mods in the CRTC, add sanity checks
			screen_configure();

			//printf("V %d %d %d %d %d\n",m_crtc.vtotal,m_crtc.display_vstart,m_crtc.display_height,m_crtc.display_vsync,m_crtc.border_height);
			break;
		}
	}
}

// TODO: all horizontal values needs to be divided by 2, presumably some other register?
// TODO: h/vstart not taken into account (needs video mods)
void ygv608_device::screen_configure()
{
//  int display_hend = (m_crtc.display_hstart + (m_crtc.display_width / 2)) - 1;
	int display_hend = (m_crtc.display_width / 2) - 1;
//  int display_vend = (m_crtc.display_vstart + m_crtc.display_height) - 1;
	int display_vend = (m_crtc.display_height) - 1;

	//rectangle visarea(m_crtc.display_hstart, display_hend, m_crtc.display_vstart, display_vend);
	rectangle visarea(0, display_hend, 0, display_vend);

	attoseconds_t period = HZ_TO_ATTOSECONDS(m_screen->clock()) * m_crtc.vtotal * (m_crtc.htotal / 2);

	m_screen->configure(m_crtc.htotal / 2, m_crtc.vtotal, visarea, period );

	// reset vblank timer
	m_vblank_timer->reset();
	//m_vblank_timer->adjust(m_screen->time_until_pos(m_crtc.display_vstart+m_crtc.display_height,0), 0, m_screen->frame_period());
	m_vblank_timer->adjust(m_screen->time_until_pos(m_crtc.display_height,0), 0, m_screen->frame_period());
}


void ygv608_device::ShowYGV608Registers()
{
	int p, b;

	logerror( "YGV608 Registers\n" );
	logerror(
		"\tR#00: $%02X : PNYA(%d),B/A(%c),PNY(%d)\n",
		m_regs.b[0],
		m_regs.s.r0 & r0_pnya,
		((m_plane_select_access == true) ? 'B' : 'A' ),
		m_regs.s.r0 & r0_pny);

	logerror(
		"\tR#01: $%02X : PNXA(%d),PNX(%d)\n",
		m_regs.b[1],
		m_regs.s.r1 & r1_pnxa,
		m_regs.s.r1 & r1_pnx);

	logerror(
		"\tR#02: $%02X : CPAW(%d),CPAR(%d),B/A(%d),SCAW(%d),SCAR(%d),SAAW(%d),SAAR(%d)\n",
		m_regs.b[2],
		m_regs.s.r2 & r2_cpaw,
		m_regs.s.r2 & r2_cpar,
		m_regs.s.r2 & r2_b_a,
		m_regs.s.r2 & r2_scaw,
		m_regs.s.r2 & r2_scar,
		m_regs.s.r2 & r2_saaw,
		m_regs.s.r2 & r2_saar);

	logerror(
		"\tR#03: $%02X : SAA($%02X)\n",
		m_regs.b[3],
		m_sprite_address );

	logerror(
		"\tR#04: $%02X : SCA($%02X)\n",
		m_regs.b[4],
		m_regs.s.sca );

	logerror(
		"\tR#05: $%02X : CC($%02X)\n",
		m_regs.b[5],
		m_regs.s.cc );

	logerror(
		"\tR#06: $%02X : SBA($%02X)\n",
		m_regs.b[6],
		m_sprite_bank );

	logerror(
		"\tR#07: $%02X : DSPE(%d),MD(%d),ZRON(%d),FLIP(%d),DCKM(%d)\n",
		m_regs.b[7],
		m_regs.s.r7 & r7_dspe,
		m_regs.s.r7 & r7_md,
		m_regs.s.r7 & r7_zron,
		m_regs.s.r7 & r7_flip,
		m_regs.s.r7 & r7_dckm);

	logerror(
		"\tR#08: $%02X : HDS(%d),VDS(%d),RLRT(%d),RLSC(%d),PGS(%d)\n",
		m_regs.b[8],
		m_regs.s.r8 & r8_hds,
		m_regs.s.r8 & r8_vds,
		m_regs.s.r8 & r8_rlrt,
		m_regs.s.r8 & r8_rlsc,
		m_regs.s.r8 & r8_pgs);

	logerror(
		"\tR#11: $%02X : CTPA(%d),CTPB(%d),PRM(%d),CBDR(%d),YSE(%d),SCM(%d)\n",
		m_regs.b[11],
		m_regs.s.r11 & r11_ctpa,
		m_regs.s.r11 & r11_ctpb,
		m_regs.s.r11 & r11_prm,
		m_regs.s.r11 & r11_cbdr,
		m_regs.s.r11 & r11_yse,
		m_regs.s.r11 & r11_scm);

	logerror(
		"\tR#40: $%02X : HTL9:8($%02X)=$%06X,HDW(%d)\n",
		m_regs.b[40],
		m_regs.s.r40 & r40_htl89, (int)(m_regs.s.r40 & r40_htl89) << 8,
		m_regs.s.r40 & r40_hdw);

	logerror(
		"\tR#41: $%02X : HDSP($%02X)\n",
		m_regs.b[41],
		m_regs.s.hdsp );

	logerror(
		"\tR#42: $%02X : HTL7:0($%02X)\n",
		m_regs.b[42],
		m_regs.s.htl );

	logerror(
		"\t              HTL=$%03X\n",
		( (int)(m_regs.s.r40 & r40_htl89) << 8 ) |
		( (int)m_regs.s.htl ) );

	logerror(
		"\tR#47: $%02X : TB12:5($%02X) = $%06X\n",
		m_regs.b[47],
		m_regs.s.tb5, (int)m_regs.s.tb5 << 5 );

	logerror(
		"\tR#48: $%02X : TB20:13($%02X) = $%06X\n",
		m_regs.b[48],
		m_regs.s.tb13, (int)m_regs.s.tb13 << 13 );

	logerror(
		"\t              TB=$%06X\n",
		( (int)m_regs.s.tb13 << 13 ) |
		( (int)m_regs.s.tb5 << 5 ) );

	logerror(
		"\tR#49: $%02X : TN11:4($%02X) = $%04X\n",
		m_regs.b[49],
		m_regs.s.tn4, (int)m_regs.s.tn4 << 4 );

	logerror(
		"ShortCuts:\n" );

	for( p=0; p<2; p++ ) {
	logerror( "\t" );
	for( b=0; b<8; b++ ) {
		logerror( "%02X ", m_base_addr[p][b] );
	}
	logerror( "\n" );
	}
}
