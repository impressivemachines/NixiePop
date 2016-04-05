/*
 * nixie.c
 *
 * Created: 1/17/2016 3:05:42 PM
 * Author : Simon Winder
 * Impressive Machines LLC
 * simon@impressivemachines.com
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

// FUSE BYTES MUST BE:
// EXTENDED = FF
// HIGH = D9
// LOW = DE (for 16MHz external crystal)

//PB0 - SHUTDOWN
//PB1 - COLON_LED
//PB2 - SS_N
//PB3 - MOSI
//PB4 - MISO
//PB5 - SCK
//PB6 - XTAL
//PB7 - XTAL

//PC0 - BCD0
//PC1 - BCD1
//PC2 - BCD2
//PC3 - BCD3
//PC4 - SDTI
//PC5 - SCKI
//PC6 - RES_N

//PD0 - RXD
//PD1 - TXD 
//PD2 - PD2
//PD3 - PD3
//PD4 - N1
//PD5 - N2
//PD6 - N3
//PD7 - N4

//ADC6 - ADC6
//ADC7 - ACC7

#define F_CPU	16000000

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

u8 g_r[4];
u8 g_g[4];
u8 g_b[4];

volatile u8 g_scan;
volatile u8 g_digit[4];
volatile u8 g_timer;

void send_bit(u8 b)
{
	PORTC = (PORTC&0xcf) | (b&1)<<4;
	PORTC = (PORTC&0xdf) | (1<<5);
	PORTC = (PORTC&0xdf);
}

void send_rgb()
{
	send_bit(1); // cmd
	send_bit(0); // cmd
	send_bit(0); // cmd
	send_bit(1); // cmd
	send_bit(0); // cmd
	send_bit(1); // cmd
	send_bit(0); // outtmg
	send_bit(0); // extgck
	send_bit(0); // tmgrst
	send_bit(1); // dsprpt
	send_bit(0); // blank
	u8 i,k;
	for(i=0; i<21; i++)
		send_bit(1);
	for(i=0; i<4; i++)
	{
		for(k=0; k<8; k++)
			send_bit(g_b[i]>>(7-k));
		for(k=0; k<8; k++)
			send_bit(0);
		for(k=0; k<8; k++)
			send_bit(g_g[i]>>(7-k));
		for(k=0; k<8; k++)
			send_bit(0);
		for(k=0; k<8; k++)
			send_bit(g_r[i]>>(7-k));
		for(k=0; k<8; k++)
			send_bit(0);
	}
}

// Note that the ISR turns off both anode and cathode for inactive display tubes
// This prevents ghosting or spread of charge between tubes.
// If the anode is on and no cathodes are connected to ground then the display does
// in fact show a mash up of the numbers because of the zener diodes conducting some current out of all cathodes.

ISR(TIMER0_OVF_vect)
{
	PORTC = (PORTC & 0xf0) | 0xf; // cathodes off
	if(g_digit[g_scan]==0xf)
	{
		PORTD = (PORTD & 0xf); // digit off
	}
	else
	{
		PORTD = (PORTD & 0xf) | (1<<((g_scan)+4)); // digit on
		PORTC = (PORTC & 0xf0) | (g_digit[g_scan]); // cathode on
	}

	g_scan = (g_scan+1) & 3;
	g_timer++;
}

void colon(u8 on)
{
	if(on)
		PORTB |= (1<<PB1);
	else
		PORTB &= ~(1<<PB1);
}

u8 serial_get()
{
	while(!(UCSR0A & (1<<RXC0)));
	return UDR0;
}

void serial_put(u8 c)
{
	while (!(UCSR0A & (1<<UDRE0)));
	UDR0 = c;
}

int main(void)
{
	DDRB = (1<<PB1)|(1<<PB0);
	PORTB = (1<<PB0); // start with HV off
	DDRC = (1<<PC5)|(1<<PC4)|(1<<PC3)|(1<<PC2)|(1<<PC1)|(1<<PC0);
	PORTC = (1<<PC3)|(1<<PC2)|(1<<PC1)|(1<<PC0);
	DDRD = (1<<PD7)|(1<<PD6)|(1<<PD5)|(1<<PD4)|(1<<PD1);
	PORTD = (1<<PD3); // PD3 has pullup
	
	//CLKPR = 0x80;
	//CLKPR = 0; // 16mhz

	g_scan = 0;
	g_timer = 0;
	
	u8 i;
		
	for(i=0; i<4; i++)
	{
		g_r[i] = 0x80;
		g_g[i] = 0x80;
		g_b[i] = 0x80;
		g_digit[i] = 0xf;
	}
	
	send_rgb();
	
	TCCR0A = 0;
	TCCR0B = 4; // free run prescale divide by 256
	TIMSK0 = 1; // enable overflow interrupt (16mhz / 32768 = 488Hz)
	sei();
		
	// check for low on PD3
	if((PIND & (1<<PD3))==0)
	{
		// voltmeter mode
		ADMUX = 6; //00000110 // ADC6 input, Vref = 5V
		ADCSRA = 0x87; //10000111 // Prescaler 128
		
		PORTB &= ~(1<<PB0); // turn on HV
		
		while(1)
		{
			g_timer = 0;
			while(g_timer<128);
			
			ADCSRA |= 0x40;
			while(ADCSRA & 0x40);
			
			u16 result = ADC;
			u32 val = result * 5005UL;
			val = val / 1024UL;
			g_digit[0] = val % 10;
			val /= 10;
			if(val>0)
			{
				g_digit[1] = val % 10;
				val /= 10;
				if(val>0)
				{
					g_digit[2] = val % 10;
					val /= 10;
					if(val>0)
						g_digit[3] = val % 10;
					else
						g_digit[3] = 0xf;
				}
				else
				{
					g_digit[2] = 0xf;
					g_digit[3] = 0xf;
				}
			}
			else
			{
				g_digit[1] = 0xf;
				g_digit[2] = 0xf;
				g_digit[3] = 0xf;
			}
		}
	}
	
	// serial mode
	// 19200 baud
	// 16000000/(16*(51+1)) = 19231 Hz
	UCSR0A = 0;
	UBRR0H = 0;
	UBRR0L = 51;
	UCSR0B = (1<<RXEN0) | (1<<TXEN0);
	UCSR0C = (1<<UCSZ00) | (1<<UCSZ01); // no parity 8 bits one stop bit

	u8 ch;
	u8 ok = 1;
	while(1)
	{
		if(ok)
			ch = serial_get();
		ok = 1;
		if(ch==':')
		{
			serial_put(ch);
			colon(1);
		}
		else if(ch==';')
		{
			serial_put(ch);
			colon(0);
		}
		else if(ch=='$')
		{
			u8 dig[4];
			serial_put(ch);
			for(i=0; i<4; i++)
			{
				ch = serial_get();
				if(ch=='-')
				{
					serial_put(ch);
					dig[i] = 0xf;
				}
				else if(ch>='0' && ch<='9')
				{
					serial_put(ch);
					dig[i] = ch&0xf;
				}
				else
				{
					serial_put('?');
					ok = 0;
					break;
				}
			}
			if(ok)
			{
				g_digit[0] = dig[3];
				g_digit[1] = dig[2];
				g_digit[2] = dig[1];
				g_digit[3] = dig[0];
				
				if(dig[0]==0xf && dig[1]==0xf && dig[2]==0xf && dig[3]==0xf)
				{
					// power down HV
					PORTB |= (1<<PB0);
				}
				else
				{
					// power up HV
					PORTB &= ~(1<<PB0);
				}
			}
		}
		else if(ch=='#')
		{
			u8 buf[12];
			serial_put(ch);
			for(i=0;i<12;i++)
			{
				ch = serial_get();
				if(ch>='0' && ch<='9')
				{
					serial_put(ch);
					buf[i] = ch - '0';
				}
				else if(ch>='a' && ch<='f')
				{
					serial_put(ch);
					buf[i] = ch - 'a' + 10;
				}
				else if(ch>='A' && ch<='F')
				{
					serial_put(ch);
					buf[i] = ch - 'A' + 10;
				}
				else
				{
					serial_put('?');
					ok = 0;
					break;
				}
			}
			
			if(ok)
			{
				g_r[0] = buf[0]<<4;
				g_g[0] = buf[1]<<4;
				g_b[0] = buf[2]<<4;
				g_r[1] = buf[3]<<4;
				g_g[1] = buf[4]<<4;
				g_b[1] = buf[5]<<4;
				g_r[2] = buf[6]<<4;
				g_g[2] = buf[7]<<4;
				g_b[2] = buf[8]<<4;
				g_r[3] = buf[9]<<4;
				g_g[3] = buf[10]<<4;
				g_b[3] = buf[11]<<4;
				send_rgb();
			}
		}
		else
			serial_put('?');
	}

}

