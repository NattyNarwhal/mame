// license:BSD-3-Clause
// copyright-holders:Sandro Ronco
/***************************************************************************

Chess-Master (G-5003-500) (10*U505 roms)
Chess-Master (G-5003-501) (2 roms set)
Chess-Master Diamond (G-5004-500)

BTANB:
- chessmst corner leds flicker sometimes

TODO:
- chessmsta isn't working, needs a redump of u2616. Program differences are
  minor so it seems to boot fine if you take 064/065 from chessmst, but will
  probably have some problems.

***************************************************************************/

#include "emu.h"

#include "bus/generic/slot.h"
#include "bus/generic/carts.h"
#include "cpu/z80/z80.h"
#include "machine/clock.h"
#include "machine/z80pio.h"
#include "machine/sensorboard.h"
#include "sound/beep.h"
#include "sound/spkrdev.h"
#include "video/pwm.h"

#include "speaker.h"

// internal artwork
#include "chessmst.lh"
#include "chessmstdm.lh"


namespace {

class chessmst_state : public driver_device
{
public:
	chessmst_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_pio(*this, "z80pio%u", 0),
		m_board(*this, "board"),
		m_led_pwm(*this, "led_pwm"),
		m_digit_pwm(*this, "digit_pwm"),
		m_speaker(*this, "speaker"),
		m_beeper(*this, "beeper"),
		m_extra(*this, "EXTRA"),
		m_buttons(*this, "BUTTONS"),
		m_digits(*this, "digit%u", 0U),
		m_direct_led(*this, "dled%u", 0U)
	{ }

	DECLARE_INPUT_CHANGED_MEMBER(halt_button);
	DECLARE_INPUT_CHANGED_MEMBER(reset_button);

	void chessmst(machine_config &config);
	void chessmsta(machine_config &config);
	void chessmstdm(machine_config &config);

protected:
	virtual void machine_start() override;

private:
	required_device<z80_device> m_maincpu;
	required_device_array<z80pio_device, 2> m_pio;
	required_device<sensorboard_device> m_board;
	required_device<pwm_display_device> m_led_pwm;
	optional_device<pwm_display_device> m_digit_pwm;
	optional_device<speaker_sound_device> m_speaker;
	optional_device<beep_device> m_beeper;
	required_ioport m_extra;
	required_ioport m_buttons;
	output_finder<4> m_digits;
	output_finder<2> m_direct_led;

	void chessmst_io(address_map &map);
	void chessmst_mem(address_map &map);
	void chessmstdm_mem(address_map &map);
	void chessmstdm_io(address_map &map);

	void digits_w(uint8_t data);
	void pio1_port_a_w(uint8_t data);
	void pio1_port_b_w(uint8_t data);
	void pio1_port_b_dm_w(uint8_t data);
	uint8_t pio2_port_a_r();
	void pio2_port_b_w(uint8_t data);
	DECLARE_WRITE_LINE_MEMBER(timer_555_w);

	void update_leds();
	void update_digits();

	uint16_t m_matrix = 0;
	uint8_t m_led_data = 0;
	uint8_t m_digit_matrix = 0;
	int m_digit_dot = 0;
	uint16_t m_digit_data = 0;
};

void chessmst_state::machine_start()
{
	m_digits.resolve();
	m_direct_led.resolve();

	save_item(NAME(m_matrix));
	save_item(NAME(m_led_data));
	save_item(NAME(m_digit_matrix));
	save_item(NAME(m_digit_dot));
	save_item(NAME(m_digit_data));
}


// Address Maps

void chessmst_state::chessmst_mem(address_map &map)
{
	map.unmap_value_high();
	map.global_mask(0x7fff); // A15 not connected
	map(0x0000, 0x27ff).rom();
	map(0x3400, 0x3bff).ram();
}

void chessmst_state::chessmstdm_mem(address_map &map)
{
	map.unmap_value_high();
	map(0x0000, 0x3fff).rom();
	map(0x4000, 0x7fff).r("cartslot", FUNC(generic_slot_device::read_rom));
	map(0x8000, 0x8bff).ram();
}

void chessmst_state::chessmst_io(address_map &map)
{
	map.unmap_value_high();
	map.global_mask(0xff);
	//map(0x00, 0x03).mirror(0xf0); read/write in both, not used by the software
	map(0x04, 0x07).mirror(0xf0).rw(m_pio[0], FUNC(z80pio_device::read), FUNC(z80pio_device::write));
	map(0x08, 0x0b).mirror(0xf0).rw(m_pio[1], FUNC(z80pio_device::read), FUNC(z80pio_device::write));
}

void chessmst_state::chessmstdm_io(address_map &map)
{
	chessmst_io(map);
	map(0x4c, 0x4c).w(FUNC(chessmst_state::digits_w));
}

WRITE_LINE_MEMBER(chessmst_state::timer_555_w)
{
	m_pio[1]->strobe_b(state);
	m_pio[1]->data_b_write(m_matrix);
}


// Input ports

INPUT_CHANGED_MEMBER(chessmst_state::halt_button)
{
	m_pio[0]->strobe_a(newval);
	reset_button(field, param, 0, 0);
}

INPUT_CHANGED_MEMBER(chessmst_state::reset_button)
{
	// pressing both 'extra' buttons causes a reset
	const bool reset = (m_extra->read() & 0x03) == 0x03;
	m_maincpu->set_input_line(INPUT_LINE_RESET, reset ? ASSERT_LINE : CLEAR_LINE);
}

static INPUT_PORTS_START( chessmst )
	PORT_START("BUTTONS")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Hint / 7")        PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_CODE(KEYCODE_H)
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Random / 6")      PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_CODE(KEYCODE_R)
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Referee / 5")     PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_CODE(KEYCODE_F)
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Self Play / 4")   PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_CODE(KEYCODE_S)
	PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Board / 3")       PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_CODE(KEYCODE_B)
	PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Color / 2")       PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_CODE(KEYCODE_C)
	PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("Level / 1")       PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_CODE(KEYCODE_L)
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYPAD) PORT_NAME("New Game / 0")    PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_CODE(KEYCODE_N)

	PORT_START("EXTRA")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Halt") PORT_CODE(KEYCODE_F2) PORT_CHANGED_MEMBER(DEVICE_SELF, chessmst_state, halt_button, 0) // -> PIO(0) ASTB pin
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Reset") PORT_CODE(KEYCODE_F1) PORT_CHANGED_MEMBER(DEVICE_SELF, chessmst_state, reset_button, 0) // -> Z80 RESET pin if HALT is pressed too
INPUT_PORTS_END

static INPUT_PORTS_START( chessmstdm )
	PORT_START("BUTTONS")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Move Fore")               PORT_CODE(KEYCODE_RIGHT)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Move Back")               PORT_CODE(KEYCODE_LEFT)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Board")                   PORT_CODE(KEYCODE_B)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Match / Time")            PORT_CODE(KEYCODE_M)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Parameter / Information") PORT_CODE(KEYCODE_I)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Selection / Dialogue")    PORT_CODE(KEYCODE_S)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Function / Notation")     PORT_CODE(KEYCODE_F)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Enter")                   PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD)

	PORT_START("EXTRA")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("Monitor")                 PORT_CODE(KEYCODE_F1) PORT_CHANGED_MEMBER(DEVICE_SELF, chessmst_state, reset_button, 0)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_NAME("View")                    PORT_CODE(KEYCODE_F2) PORT_CHANGED_MEMBER(DEVICE_SELF, chessmst_state, reset_button, 0)
INPUT_PORTS_END


// I/O

void chessmst_state::update_digits()
{
	uint16_t digit_data = bitswap<16>(m_digit_data, 3,5,12,10,14,1,2,13,8,6,11,15,7,9,4,0);
	m_digit_pwm->matrix(m_digit_matrix, digit_data | (m_digit_dot << 16));
}

void chessmst_state::digits_w(uint8_t data)
{
	m_digit_data = (m_digit_data << 4) | (data & 0x0f);
	m_digit_matrix = (data >> 4) & 0x0f;
	update_digits();
}

void chessmst_state::update_leds()
{
	m_led_pwm->matrix(m_matrix, m_led_data);
}

void chessmst_state::pio1_port_a_w(uint8_t data)
{
	m_led_data = ~data;
	update_leds();
}

void chessmst_state::pio1_port_b_w(uint8_t data)
{
	m_matrix = (m_matrix & 0xff) | ((data & 0x03)<<8);
	update_leds();

	m_direct_led[0] = BIT(~data, 2); // check
	m_direct_led[1] = BIT(~data, 3); // cm

	m_speaker->level_w(BIT(data, 6));
}

void chessmst_state::pio1_port_b_dm_w(uint8_t data)
{
	m_matrix = (m_matrix & 0xff) | ((data & 0x04)<<6);

	m_digit_dot = BIT(data, 4);
	update_digits();

	m_beeper->set_state(BIT(data, 3));

	m_direct_led[0] = BIT(data, 5); // monitor
	m_direct_led[1] = BIT(data, 6); // playmode
}

uint8_t chessmst_state::pio2_port_a_r()
{
	uint8_t data = 0;

	// The pieces position on the chessboard is identified by 64 Hall
	// sensors, which are in a 8x8 matrix with the corresponding LEDs.
	for (int i = 0; i < 8; i++)
	{
		if (BIT(m_matrix, i))
			data |= m_board->read_file(i);
	}

	if (m_matrix & 0x100)
		data |= m_buttons->read();

	return ~data;
}

void chessmst_state::pio2_port_b_w(uint8_t data)
{
	m_matrix = (data & 0xff) | (m_matrix & ~0xff);
	update_leds();
}


// Machine Configuration

static const z80_daisy_config chessmst_daisy_chain[] =
{
	{ "z80pio0" },
	{ nullptr }
};

static const z80_daisy_config chessmstdm_daisy_chain[] =
{
	{ "z80pio1" },
	{ nullptr }
};

void chessmst_state::chessmst(machine_config &config)
{
	// basic machine hardware
	Z80(config, m_maincpu, 9.8304_MHz_XTAL/4); // UB880 Z80 clone
	m_maincpu->set_addrmap(AS_PROGRAM, &chessmst_state::chessmst_mem);
	m_maincpu->set_addrmap(AS_IO, &chessmst_state::chessmst_io);
	m_maincpu->set_daisy_config(chessmst_daisy_chain);

	Z80PIO(config, m_pio[0], 9.8304_MHz_XTAL/4);
	m_pio[0]->out_int_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ0);
	m_pio[0]->out_pa_callback().set(FUNC(chessmst_state::pio1_port_a_w));
	m_pio[0]->out_pb_callback().set(FUNC(chessmst_state::pio1_port_b_w));

	Z80PIO(config, m_pio[1], 9.8304_MHz_XTAL/4);
	m_pio[1]->in_pa_callback().set(FUNC(chessmst_state::pio2_port_a_r));
	m_pio[1]->out_pb_callback().set(FUNC(chessmst_state::pio2_port_b_w));

	SENSORBOARD(config, m_board);
	m_board->set_type(sensorboard_device::MAGNETS);
	m_board->init_cb().set(m_board, FUNC(sensorboard_device::preset_chess));
	m_board->set_delay(attotime::from_msec(100));

	// video hardware
	PWM_DISPLAY(config, m_led_pwm).set_size(10, 8);
	config.set_default_layout(layout_chessmst);

	// sound hardware
	SPEAKER(config, "mono").front_center();
	SPEAKER_SOUND(config, m_speaker).add_route(ALL_OUTPUTS, "mono", 0.25);
}

void chessmst_state::chessmsta(machine_config &config)
{
	chessmst(config);

	// faster UA880 CPU
	const XTAL clk = 8_MHz_XTAL / 2;
	m_maincpu->set_clock(clk);
	m_pio[0]->set_clock(clk);
	m_pio[1]->set_clock(clk);
}

void chessmst_state::chessmstdm(machine_config &config)
{
	chessmsta(config);

	// basic machine hardware
	m_maincpu->set_addrmap(AS_PROGRAM, &chessmst_state::chessmstdm_mem);
	m_maincpu->set_addrmap(AS_IO, &chessmst_state::chessmstdm_io);
	m_maincpu->set_daisy_config(chessmstdm_daisy_chain);

	CLOCK(config, "555_timer", 500).signal_handler().set(FUNC(chessmst_state::timer_555_w));

	m_pio[0]->out_pb_callback().set(FUNC(chessmst_state::pio1_port_b_dm_w));
	m_pio[0]->in_pb_callback().set_ioport("EXTRA");

	m_pio[0]->out_int_callback().set_nop();
	m_pio[1]->out_int_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ0);

	// video hardware
	PWM_DISPLAY(config, m_digit_pwm).set_size(4, 17);
	m_digit_pwm->set_segmask(0xf, 0x1ffff);
	m_digit_pwm->output_digit().set([this](offs_t offset, uint64_t data) { m_digits[offset] = data; });
	config.set_default_layout(layout_chessmstdm);

	// sound hardware
	config.device_remove("speaker");
	BEEP(config, m_beeper, 1000).add_route(ALL_OUTPUTS, "mono", 0.25);

	// cartridge
	GENERIC_CARTSLOT(config, "cartslot", generic_plain_slot, "chessmstdm_cart");
	SOFTWARE_LIST(config, "cart_list").set_original("chessmstdm");
}


// ROM definition

ROM_START( chessmst )
	ROM_REGION( 0x2800, "maincpu", 0 )
	ROM_LOAD("056.bin", 0x0000, 0x0400, CRC(2b90e5d3) SHA1(c47445964b2e6cb11bd1f27e395cf980c97af196) ) // U505
	ROM_LOAD("057.bin", 0x0400, 0x0400, CRC(e666fc56) SHA1(3fa75b82cead81973bea94191a5c35f0acaaa0e6) ) // "
	ROM_LOAD("058.bin", 0x0800, 0x0400, CRC(6a17fbec) SHA1(019051e93a5114477c50eaa87e1ff01b02eb404d) ) // "
	ROM_LOAD("059.bin", 0x0c00, 0x0400, CRC(e96e3d07) SHA1(20fab75f206f842231f0414ebc473ce2a7371e7f) ) // "
	ROM_LOAD("060.bin", 0x1000, 0x0400, CRC(0e31f000) SHA1(daac924b79957a71a4b276bf2cef44badcbe37d3) ) // "
	ROM_LOAD("061.bin", 0x1400, 0x0400, CRC(69ad896d) SHA1(25d999b59d4cc74bd339032c26889af00e64df60) ) // "
	ROM_LOAD("062.bin", 0x1800, 0x0400, CRC(c42925fe) SHA1(c42d8d7c30a9b6d91ac994cec0cc2723f41324e9) ) // "
	ROM_LOAD("063.bin", 0x1c00, 0x0400, CRC(86be4cdb) SHA1(741f984c15c6841e227a8722ba30cf9e6b86d878) ) // "
	ROM_LOAD("064.bin", 0x2000, 0x0400, CRC(e82f5480) SHA1(38a939158052f5e6484ee3725b86e522541fe4aa) ) // "
	ROM_LOAD("065.bin", 0x2400, 0x0400, CRC(4ec0e92c) SHA1(0b748231a50777391b04c1778750fbb46c21bee8) ) // "
ROM_END

ROM_START( chessmsta )
	ROM_REGION( 0x2800, "maincpu", 0 )
	ROM_LOAD("bm001.d204", 0x0000, 0x2000, CRC(6be28876) SHA1(fd7d77b471e7792aef3b2b3f7ff1de4cdafc94c9) ) // U2364D45
	ROM_LOAD("bm108.d205", 0x2000, 0x0800, CRC(6e69ace3) SHA1(e099b6b6cc505092f64b8d51ab9c70aa64f58f70) BAD_DUMP ) // U2616D45 - problem with d3
ROM_END

ROM_START( chessmstdm )
	ROM_REGION( 0x4000, "maincpu", 0 )
	ROM_LOAD("002.d224", 0x0000, 0x2000, CRC(bed56fef) SHA1(dad0f8ddbd9b10013a5bdcc09ee6db39cfb26b78) ) // U2364D45
	ROM_LOAD("201.d225", 0x2000, 0x2000, CRC(c9dc7f29) SHA1(a3e1b66d0e15ffe83a9165d15c4a83013852c2fe) ) // "
ROM_END

} // anonymous namespace


// Driver

//    YEAR  NAME        PARENT    COMPAT  MACHINE     INPUT       CLASS           INIT        COMPANY, FULLNAME, FLAGS
COMP( 1984, chessmst,   0,        0,      chessmst,   chessmst,   chessmst_state, empty_init, "VEB Mikroelektronik \"Karl Marx\" Erfurt", "Chess-Master (set 1)", MACHINE_SUPPORTS_SAVE | MACHINE_CLICKABLE_ARTWORK )
COMP( 1984, chessmsta,  chessmst, 0,      chessmsta,  chessmst,   chessmst_state, empty_init, "VEB Mikroelektronik \"Karl Marx\" Erfurt", "Chess-Master (set 2)", MACHINE_SUPPORTS_SAVE | MACHINE_NOT_WORKING | MACHINE_CLICKABLE_ARTWORK )

COMP( 1987, chessmstdm, 0,        0,      chessmstdm, chessmstdm, chessmst_state, empty_init, "VEB Mikroelektronik \"Karl Marx\" Erfurt", "Chess-Master Diamond", MACHINE_SUPPORTS_SAVE | MACHINE_CLICKABLE_ARTWORK )
