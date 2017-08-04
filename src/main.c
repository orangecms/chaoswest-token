#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "i2cusb.h"
#include "light_ws2812.h"

#define LED_R (1 << PB5)
#define LED_G (1 << PB0)
#define LED_B (1 << PB1)

struct cRGB ws2813_led[5];

uint8_t frontRGB[3] = {0, 0, 0};
uint8_t frontFadeRGB[3] = {0, 0, 0};
uint8_t pwmClock = 0;

const uint8_t pwmTable[] PROGMEM = {
	0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b,
	0x0c, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x10, 0x10,
	0x10, 0x11, 0x11, 0x11, 0x12, 0x12, 0x13, 0x13, 0x13, 0x14, 0x14, 0x15, 0x15, 0x16, 0x16, 0x17,
	0x17, 0x18, 0x18, 0x19, 0x19, 0x1a, 0x1a, 0x1b, 0x1b, 0x1c, 0x1d, 0x1d, 0x1e, 0x1f, 0x1f, 0x20,
	0x21, 0x21, 0x22, 0x23, 0x24, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x29, 0x2a, 0x2b, 0x2c, 0x2d,
	0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3b, 0x3c, 0x3d, 0x3e, 0x40,
	0x41, 0x43, 0x44, 0x46, 0x47, 0x49, 0x4a, 0x4c, 0x4e, 0x4f, 0x51, 0x53, 0x55, 0x56, 0x58, 0x5a,
	0x5c, 0x5e, 0x60, 0x62, 0x65, 0x67, 0x69, 0x6b, 0x6e, 0x70, 0x72, 0x75, 0x78, 0x7a, 0x7d, 0x80,
	0x82, 0x85, 0x88, 0x8b, 0x8e, 0x91, 0x94, 0x98, 0x9b, 0x9e, 0xa2, 0xa5, 0xa9, 0xad, 0xb0, 0xb4,
	0xb8, 0xbc, 0xc0, 0xc5, 0xc9, 0xcd, 0xd2, 0xd6, 0xdb, 0xe0, 0xe5, 0xea, 0xef, 0xf4, 0xfa, 0xff
};

uint8_t readTemperature() {
	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC)) { }
	return ADCH;
}

void setFrontFadeColorValue(uint8_t channel, uint8_t value) {
	frontFadeRGB[channel] = pgm_read_byte(&(pwmTable[value]));
}

void setFrontColorValue(uint8_t channel, uint8_t value) {
	frontRGB[channel] = pgm_read_byte(&(pwmTable[value]));
	frontFadeRGB[channel] = frontRGB[channel];
}

/* Timer 0: 125kHz Software PWM */
/*ISR(TIMER0_COMPA_vect)
{
	if (pwmClock < frontRGB[0]) {
		PORTB &= ~LED_R;
	} else {
		PORTB |= LED_R;
	}
	if (pwmClock < frontRGB[1]) {
		PORTB &= ~LED_G;
	} else {
		PORTB |= LED_G;
	}
	if (pwmClock < frontRGB[2]) {
		PORTB &= ~LED_B;
	} else {
		PORTB |= LED_B;
	}

	pwmClock++;

	if (pwmClock == 0) {
		for (uint8_t i = 0; i < 3; i++) {
			if (frontRGB[i] > frontFadeRGB[i]) {
				frontRGB[i]--;
			} else if (frontRGB[i] < frontFadeRGB[i]) {
				frontRGB[i]++;
			}
		}
	}
}*/

int main(void) {
	temperature_setup();
	usb_setup();
	
	// Initialize GPIOs
	PORTB |= LED_R | LED_G | LED_B;
	DDRB |= LED_R | LED_G | LED_B;

	ws2813_led[0].r=255;ws2813_led[0].g=0;ws2813_led[0].b=0;
	ws2813_led[1].r=255;ws2813_led[1].g=255;ws2813_led[1].b=0;
	ws2813_led[2].r=0;ws2813_led[2].g=255;ws2813_led[2].b=0;
	ws2813_led[3].r=0;ws2813_led[3].g=0;ws2813_led[3].b=255;
	ws2813_led[4].r=255;ws2813_led[4].g=0;ws2813_led[4].b=255;
	ws2812_setleds(ws2813_led,5);
	_delay_ms(100);
	ws2812_setleds(ws2813_led,5);
	_delay_ms(100);
	PORTB |= (LED_R | LED_G | LED_B);
    

    uint16_t virtual_timer = 0;
    uint8_t step = 0;
    for(;;)
	{
		virtual_timer++;
		if (virtual_timer == 0) {
			temperature_measure();

			switch (step) {
				case 0:
				setFrontFadeColorValue(0, 255);
				setFrontFadeColorValue(1, 0);
				setFrontFadeColorValue(2, 0);
				break;

				case 1:
				setFrontFadeColorValue(0, 255);
				setFrontFadeColorValue(1, 255);
				setFrontFadeColorValue(2, 0);
				break;

				case 2:
				setFrontFadeColorValue(0, 0);
				setFrontFadeColorValue(1, 255);
				setFrontFadeColorValue(2, 0);
				break;

				case 3:
				setFrontFadeColorValue(0, 0);
				setFrontFadeColorValue(1, 255);
				setFrontFadeColorValue(2, 255);
				break;

				case 4:
				setFrontFadeColorValue(0, 0);
				setFrontFadeColorValue(1, 0);
				setFrontFadeColorValue(2, 255);
				break;

				case 5:
				setFrontFadeColorValue(0, 255);
				setFrontFadeColorValue(1, 0);
				setFrontFadeColorValue(2, 255);
				step = -1;
				break;

				default:
				break;
			}
			step++;
		}
		if (1) {
			if (pwmClock < frontRGB[0]) {
				PORTB &= ~LED_R;
			} else {
				PORTB |= LED_R;
			}
			if (pwmClock < frontRGB[1]) {
				PORTB &= ~LED_G;
			} else {
				PORTB |= LED_G;
			}
			if (pwmClock < frontRGB[2]) {
				PORTB &= ~LED_B;
			} else {
				PORTB |= LED_B;
			}

			pwmClock++;

			if (pwmClock == 0) {
				for (uint8_t i = 0; i < 3; i++) {
					if (frontRGB[i] > frontFadeRGB[i]) {
						frontRGB[i]--;
					} else if (frontRGB[i] < frontFadeRGB[i]) {
						frontRGB[i]++;
					}
				}
			}
		}
		usb_loop();
	}

    return 0; 
}

/*
#define TEMPERATURE_THRESHOLD 80

int main() {
	// Initialize GPIOs
	PORTB |= LED_R | LED_G | LED_B;
	DDRB |= LED_R | LED_G | LED_B;

	// Setup ADC (1.1 V reference, ca. +50°C; 0 V is ca. -50°C)
	ADMUX = (1 << REFS2) | (1 << REFS1) | (1 << ADLAR) | (1 << MUX0);
	ADCSRA = (1 << ADEN); // | (1 << ADIE);

	// Initialize Timer (no prescaler, CTC mode with 125kHz frequency)
	TCCR0A = (1 << WGM01);
	OCR0A = 132;
	TIMSK = (1 << OCIE0A);
	TCCR0B = (1 << CS00);
	sei();

	//setFrontColorValue(2, 255);

	setFrontColorValue(0, 127);
	setFrontColorValue(2, 127);

	while (1) {
		/*for (uint8_t fadeState = 0; fadeState < 6; fadeState++) {
			uint8_t fadeColor = 0;
			do {
				switch (fadeState) {
					case 0: // fade to red + blue
					setFrontColorValue(0, fadeColor);
					break;

					case 1: // fade to red
					setFrontColorValue(2, 255 - fadeColor);
					break;

					case 2: // fade to red + green
					setFrontColorValue(1, fadeColor);
					break;

					case 3: // fade to green
					setFrontColorValue(0, 255 - fadeColor);
					break;

					case 4: // fade to green + blue
					setFrontColorValue(2, fadeColor);
					break;

					case 5: // fade to blue
					setFrontColorValue(1, 255 - fadeColor);
					break;
				}
				_delay_ms(10);
				fadeColor++;
			} while (fadeColor != 0);
		}* /

		uint8_t tempValue = ((int8_t)readTemperature());
		uint8_t tempColor = 0;
		if (tempValue > TEMPERATURE_THRESHOLD + 8) {
			tempColor = 0xFF;
		} else if (tempValue < TEMPERATURE_THRESHOLD - 6) {
			tempColor = 0x00;
		} else {
			tempColor = (tempValue + 8 - TEMPERATURE_THRESHOLD);
			tempColor = (tempColor << 4) | tempColor;
		}

		setFrontFadeColorValue(0, tempColor);
		setFrontFadeColorValue(2, 255 - tempColor);
	}

	return 0;
}*/