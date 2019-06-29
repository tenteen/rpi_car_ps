// I burned up the FET/high side switch and ATTiny25 on my mausberry car supply.  I couldn't recover the binary off the
// flash, so I'm writing replacement firmware instead.
//
// BOM: 
// BTS5090 - High side switch - switches 12V BAT to DC-DC converter
// b 150 24 - Polyfuse - 24V 1.5A polyfuse protecting 12V BAT input.
// '103' 8 pin - 4x 10k resistors.  Used for pull up/down.
// unknown 5 pin device - 5V linear regulator e.g. L2204.  Provides 5V when 12V SWITCHED turns on
// unknown 3 pin device - pair of diodes with common cathode.  This supplies 5v when either supply is on.
// ATtiny25 - If you're here, you know what this does
// 
// Principle of operation:
// When 12V SWITCHED is turned on, the little 2204 provides 5v to the microcontroller (uC).  The uC
// immediately turns on the BTS5090 which provides 12V BAT to the main DC-DC converter.  This powers up the pi.
// When 12V SWITCHED is turned off, the uC continues to run off of 12V BAT via the DC-DC converter through the common 
// cathode diode arrangement.  The pi is notified via PI_IN.  When PI_OUT goes low (or timeout maybe?) the uC turns off the 
// BTS5090 which powers down the entire circuit.
//          _________
// RESET ---|   T   |--- VCC
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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>


#define F_CPU 0x1000000UL

#define PI_IN        PB3
#define LED          PB4
#define BIG_SWITCH   PB2
#define SWITCHED_PWR PB1
#define PI_OUT       PB0

#define LED_DELAY_MS 200

static void enable_timer0(void) {
	TCCR0B |= _BV(CS02) | _BV(CS00); // F_CPU / 1024
}

static void disable_timer0(void) {
	TCCR0B &= ~(_BV(CS02) | _BV(CS01) | _BV(CS00)); // 0
}

ISR(PCINT0_vect) {

	if (PINB & _BV(PI_OUT)) {
		// Pi is requesting power off.  Apply muzzle to temple and depress trigger smoothly.
		PORTB &= ~_BV(BIG_SWITCH);

		// If we're still alive, then the switched power is still on.  Turn off LED to indicate
		// power removed from pi.
		disable_timer0();
		PORTB &= ~_BV(LED);
	}

	if (!(PINB & _BV(SWITCHED_PWR))) {
		// Notify Pi the switch is off
		PORTB |= _BV(PI_IN);

		// Blink the LED indicating we're waiting for the pi to power down
		enable_timer0();
	}
}

ISR(TIM0_OVF_vect) {
	// Toggle LED.  At F_CPU = 1MHz and timer 0 running F_CPU/1024, one overflow is roughly 1mS.
	static int led_state = 1;
	static int overflows = 0;

	overflows++;

	if (overflows >= LED_DELAY_MS) {
		if (led_state){
			PORTB &= ~_BV(LED);
			led_state = 0;
		} else {
			PORTB |= _BV(LED);
			led_state = 1;
		}
		
		overflows = 0;
	}
}

int main(void) {
	
	// Enable outputs
	DDRB = _BV(LED) | _BV(BIG_SWITCH) | _BV(PI_IN);

	// Enable Pin Change Interrupts for input pins
	GIMSK = PCIE;
	PCMSK = _BV(SWITCHED_PWR) | _BV(PI_OUT);

	// Configure but don't start timer0 to blink the led
	TIMSK = _BV(TOIE0);  // enable overflow int

	// Turn on main power and the LED
	PORTB = _BV(LED) | _BV(BIG_SWITCH);

	// Enable interrupts and sleep forever.
	sei();
	for(;;) {
		sleep_mode();
	}
}
