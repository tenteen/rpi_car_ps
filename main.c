// I burned up the high side switch and ATTiny25 on my Mausberry 3A car supply.  I left it dangling loose and 
// it shorted to ground somewhere on the chassis. I couldn't recover the original binary, so I'm writing 
// replacement firmware instead.  I haven't tried contacting Mausberry and I am in no way affiliated with 
// Mausberry. This is just for fun.
//
// BOM: 
// BTS5090 - High side switch - switches 12V BAT to DC-DC converter
// b 150 24 - Polyfuse - 24V 1.5A polyfuse protecting 12V BAT input.
// '103' 8 pin - 4x 10k resistor pack.  Used for pull up/down.
// unknown 5 pin device - 5V linear regulator e.g. L2204.  Provides 5V when 12V SWITCHED turns on
// unknown 3 pin device - pair of diodes with common cathode.  This supplies 5v when either supply is on.
// ATtiny25 - If you're here, you know what this does
// 
// Principle of operation:
// When 12V SWITCHED is turned on, the little 2204 provides 5v to the microcontroller (uC).  The uC
// immediately turns on the BTS5090 which provides 12V BAT to the main DC-DC converter.  This powers up the pi.
// When 12V SWITCHED is turned off, the uC continues to run off of 12V BAT via the DC-DC converter through 
// the common cathode diode arrangement.  The pi is notified of power loss via PI_IN.  When PI_OUT goes 
// low, the uC turns off the BTS5090 which powers down the entire circuit.
//
// The LED is configured to blink after power on before the Pi script raises PI_OUT.  The LED then goes solid,
// indicating normal operation.  When switched power is removed, the LED blinks again, waiting for the PI_OUT
// pin to go low.  When the PI_OUT pin goes low, main power is removed and the LED goes out.
//          _________
// RESET ---|1  T   |--- VCC
//          |   I   |
// PB3   ---|   N   |--- PB2
//          |   Y   |
// PB4   ---|   2   |--- PB1
//          |   5   |
// GND   ---|_______|--- PB0
//
// Pin assignments:
// RESET: NC
// PB3: PI_IN
// PB4: LED
// PB2: BTS5090 IN
// PB1: 5V switched sense
// PB0: PI_OUT
//
// NOTE: Pin PB1 is directly connected to 5V, so it cannot be driven by an ISP programmer to
// program the device while power is present.  The ATTiny25 must be powered externally with 
// the rest of the circuit unpowered.  I also had to slow the clock way down to successfully 
// write flash - 100uS (-B 100 for avrdude).

#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>


#define F_CPU 0x1000000UL

#define PI_IN        PB3
#define LED          PB4
#define BIG_SWITCH   PB2
#define SWITCHED_PWR PB1
#define PI_OUT       PB0

static void enable_timer0(void) {
	TCCR0B |= (_BV(CS02) | _BV(CS00)); // F_CPU / 1024
}

static void disable_timer0(void) {
	TCCR0B &= ~(_BV(CS02) | _BV(CS01) | _BV(CS00)); // 0
}

static bool pi_script_running = false;
ISR(PCINT0_vect) {

	if (PINB & _BV(PI_OUT)) {
		// Pi script turns the pin high when running.
		disable_timer0();
		pi_script_running = true;

	} else if (pi_script_running) {
		// Pi is requesting power off.  Apply muzzle to temple and depress trigger smoothly.
		PORTB &= ~_BV(BIG_SWITCH);

		// If we're still alive, then the switched power is still on.  Turn off LED to indicate
		// power removed from pi.
		disable_timer0();
		PORTB &= ~_BV(LED);
	}

	if (PINB & _BV(SWITCHED_PWR)) {
		// Notify Pi the switched power is on
		PORTB &= ~_BV(PI_IN);

		// Stop blinking the LED
		disable_timer0();
		PORTB = PINB | _BV(LED);
	} else {
		// Notify Pi the switch is off
		PORTB |= _BV(PI_IN);

		// Blink the LED indicating we're waiting for the pi to power down
		enable_timer0();
	}
}

ISR(TIM0_OVF_vect) {
	// Toggle LED.  At F_CPU = 1MHz and timer 0 running F_CPU/1024, one overflow is roughly 1/4 second.
	PORTB = PINB ^ _BV(LED);
}

int main(void) {
	
	// Enable outputs
	DDRB = _BV(LED) | _BV(BIG_SWITCH) | _BV(PI_IN);

	// Enable Pin Change Interrupts for input pins
	GIMSK = _BV(PCIE);
	PCMSK = _BV(SWITCHED_PWR) | _BV(PI_OUT);

	// Configure and enable timer0 to blink the led 
	// indicating waiting for pi to start
	TIMSK = _BV(TOIE0);  // enable overflow int
	enable_timer0();

	// Turn on main power
	PORTB = _BV(BIG_SWITCH);

	// Enable interrupts and sleep forever.
	sei();
	for(;;) {
		sleep_mode();
	}
}
