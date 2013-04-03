/*************************************************************************

Edward Kuschel

15 January 2011




*************************************************************************/ 
#include <avr/io.h> 
#include <util/delay.h>
#include <avr/interrupt.h>
#include "lcd.h"

#define MUX_ACTIVE 0b10000000		
#define MUX_IN_MASK 0b00000111
#define BUTTON1 0b00000111
#define BUTTON2 0b00000110
#define BUTTON3 0b00000101
#define BUTTON4 0b00000100
#define BUTTON5 0b00000011
#define BUTTON6 0b00000010
#define BUTTON7 0b00000001
#define BUTTON8 0b00000000

#define ROTPORT PORTD
#define ROTDDR	DDRD
#define ROTPIN	PIND
#define ROTPA	PD2
#define ROTPB	PD3
#define ROTA !((1<<ROTPA)&ROTPIN)
#define ROTB !((1<<ROTPB)&ROTPIN)

#define BEATLENGTH 8

void init_MAIN();
void init_USART();
void welcomeScreen();	
void uart_tx_char(char input);
void midi(char cmd, char data1, char data2);
void scrolling();
int poll_inputs();
int led_shifting(char led_array[]);
int get_rot_status(void);
void clear_rot_status(void);
int select_pot();
void output_on_beat();
void turn_midi_off();
void control_screen();
void poll_pots();

volatile unsigned int result;
volatile int button_input = 0, pot_num = 1, beat =0, button_press = 1;
volatile int check_beat = -1, check_input = -1;
volatile int rotary_status, overflow_count = 0, overflow_count2 = 0;
volatile int temp1 = 0, temp2 = 0, temp3 = 0, temp4 = 0, temp5 = 0, temp6 = 0, temp7 = 0, temp8 = 0;
volatile int note1 = 60, note2 = 60, note3 = 60, note4 = 60, note5 = 60, note6 = 60, note7 = 60, note8 = 60;


ISR(INT0_vect){

}

ISR(INT1_vect){

}

ISR(ADC_vect){
	turn_midi_off();
	result = ADCH;
	result = result/2;
	switch (pot_num){
		case 1:
		ADMUX = 0b01100000;
		note1 = result;
		break;
		case 2:
		ADMUX = 0b01100001;
		note2 = result;
		break;
		case 3:
		ADMUX = 0b01100010;
		note3 = result;
		break;
		case 4:
		ADMUX = 0b01100011;
		note4 = result;
		break;
		case 5:
		ADMUX = 0b01100100;
		note5 = result;
		break;
		case 6:
		ADMUX = 0b01100101;
		note6 = result;
		break;
		case 7:
		ADMUX = 0b01100110;
		note7 = result;
		break;
		case 8:
		ADMUX = 0b01100111;
		note8 = result;
		break;
	}
}

ISR(TIMER1_OVF_vect){
	turn_midi_off();
	overflow_count++;
	if (overflow_count >= BEATLENGTH/4){
		overflow_count = 0;

		output_on_beat();	
		PORTD |= (1<<4);

		if (beat <= 7){
		beat++;
		}

		else if (beat > 7){
		beat = 1;
		}
	}
	if (overflow_count == (BEATLENGTH/1)){
		PORTD &= ~(1<<4);}
}

ISR(TIMER2_OVF_vect){
//	overflow_count2++;
//	if (overflow_count2 >= 82){
//		overflow_count2 = 0;
//		midi(0b11111000, 0, 0);
//	}
}

int main(void){
	
	init_MAIN();	

	// Notification Test
	_delay_ms(500);
	welcomeScreen();
	sei();

	LCDOn();				// Turn display on
	LCDClear();				// Clear Display

	while(1){	
		button_press = poll_inputs();
	//	poll_pots();
		control_screen();
	}
	
	return(0);
}

void init_MAIN(){
// Initialise I/O Ports
	DDRD |= (1<<4);
	DDRC = 0b01111000;

	// Rotary Encoder Initialisation
	ROTDDR &= ~((1<<ROTPA)|(1<<ROTPB));
	
	PORTD &= ~(1<<4);
	PORTC |= (1<<6);	// Set PC6 to enable shift register

	ADMUX = 0b01100000;
	ADCSRA = 0b10001101;

	GICR = (1 << INT0);
//	GICR = (1 << INT1); 
	MCUCR = (1<<ISC01); 
//	MCUCR = (1<<ISC11); 
//	MCUCR = (1<<ISC10); 

	TCCR1A = 0b00000000;	// normal mode 
	TCCR1B = 0b00000010;	// 
	TCCR2 = 0b00000010;
	TIMSK  = 0b01000100;	// enable Timer 1 overflow interrupt

	// Initialize the LCD
	InitLCD(LS_NOBLINK|LS_NOCURSER);

	// Initialize the USART
	init_USART();
}

void init_USART(){

	// Set normal speed
	UCSRA = 0b00000000;

	// Enable USART for transmission
	UCSRB = 0b00001000;

	// No parity, 1 stop bit and 8 data bits
	UCSRC = 0b10000110;

	// Baud Rate 31250bps
	UBRRH = 0x00;
	UBRRL = 0x0F;
}

void welcomeScreen()
{
//  welcome screen
	LCDOn();				// Turn display on
	LCDClear();				// Clear Display
	LCDGotoXY(6,0);			// Go to position and print message
	LCDWriteString("M");
	_delay_ms(300);
	LCDWriteString("I");
	_delay_ms(300);
	LCDWriteString("D");
	_delay_ms(300);
	LCDWriteString("I");
	_delay_ms(300);
	LCDGotoXY(1,1);
	LCDWriteString("Control Device");
	
//	PORTB = ~0b00000100;	// Power light ON
	_delay_ms(1500);		// Wait to clear screen
	LCDOff();
}

void uart_tx_char(char input){
    // wait until UDRE flag is set to logic 1
	while ((UCSRA & (1<<UDRE)) == 0x00){;}
	PORTD |= (1<<4);
	PORTD &= ~(1<<4);
	UDR = input; // Write character to UDR for transmission
}

void midi(char cmd, char data1, char data2){ 
	uart_tx_char(cmd);
  	uart_tx_char(data1);
  	uart_tx_char(data2);	
}

/*
void scrolling(){
char textIn[12];
int i=0;

while(i <= sizeof(textIn)){
LCDWriteStringXY(0,0, "textIn[i]");
LCDWriteStringXY(1,0, "textIn[i+1]");
LCDWriteStringXY(2,0, (char)textIn[i+2]);
LCDWriteStringXY(3,0, (char)textIn[i+3]);
LCDWriteStringXY(4,0, (char)textIn[i+4]);
LCDWriteStringXY(5,0, (char)textIn[i+5]);
LCDWriteStringXY(6,0, (char)textIn[i+6]);
LCDWriteStringXY(7,0, (char)textIn[i+7]);
LCDWriteStringXY(8,0, (char)textIn[i+8]);
LCDWriteStringXY(9,0, (char)textIn[i+9]);
LCDWriteStringXY(10,0, (char)textIn[i+10]);
LCDWriteStringXY(11,0, (char)textIn[i+11]);
LCDWriteStringXY(12,0, (char)textIn[i+12]);
LCDWriteStringXY(13,0, (char)textIn[i+13]);
LCDWriteStringXY(14,0, (char)textIn[i+14]);
LCDWriteStringXY(15,0, (char)textIn[i+15]);
i++;
_delay_ms(200);}
}
*/

int poll_inputs(){
	
	// Button Input Function

	if((PINC & MUX_ACTIVE) != 0){						// If any button is pressed...
	button_input = PINC;								// Read info from port

	button_input = (button_input & MUX_IN_MASK);		// Read in current push button
		
	LCDClear();											// Clear Display

		if (button_input==BUTTON8){	
			_delay_ms(200);
			if (temp8 == 0)
				temp8 = 1;
			else if (temp8 == 1)
				temp8 = 0;
		}
		else if (button_input==BUTTON7){
			_delay_ms(200);	
			if (temp7 == 0)
				temp7 = 1;
			else if (temp7 == 1)
				temp7 = 0;
		}
		else if (button_input==BUTTON6){
			_delay_ms(200);	
			if (temp6 == 0)
				temp6 = 1;
			else if (temp6 == 1)
				temp6 = 0;
		}
		else if (button_input==BUTTON5){
			_delay_ms(200);	
			if (temp5 == 0)
				temp5 = 1;
			else if (temp5 == 1)
				temp5 = 0;
		}
		else if (button_input==BUTTON4){
			_delay_ms(200);	
			if (temp4 == 0)
				temp4 = 1;
			else if (temp4 == 1)
				temp4 = 0;
		}
		else if (button_input==BUTTON3){
			_delay_ms(200);	
			if (temp3 == 0)
				temp3 = 1;
			else if (temp3 == 1)
				temp3 = 0;
		}
		else if (button_input==BUTTON2){
			_delay_ms(200);	
			if (temp2 == 0)
				temp2 = 1;
			else if (temp2 == 1)
				temp2 = 0;			
		}
		else if (button_input==BUTTON1){
			_delay_ms(200);
			if (temp1 == 0)
				temp1 = 1;
			else if (temp1 == 1)
				temp1 = 0;
		}
		else
			return(0);
	}	
	
	if((~PINB & 0b10000000) != 0){
	turn_midi_off();
	pot_num = select_pot();
	return(9);
	}

	else 
	return(100);
}

int led_shifting(char led_array[]){
	
	// LED Shift Function

	int i;

	if (check_beat == beat)					// do nothing if same button is pressed 
	return(0);

	PORTC |= (1<<3);

	for(i=7; i>=0; i--){
	if (led_array[i] == '1')
		PORTC |= (1<<4);
	if (led_array[i] == '0')
		PORTC &= ~(1<<4);

	PORTC |= (1<<5);
	PORTC &= ~(1<<5);
	}

	check_beat = beat;
	return(0);
}

int get_rot_status(void){
	return(rotary_status);
}

void clear_rot_status(void){
	rotary_status = 0;
}

int select_pot(){

	if (pot_num <= 7){
		pot_num++;
		_delay_ms(200);
		return(pot_num);}
	else if (pot_num > 7){
		pot_num = 1;
		_delay_ms(200);
		return(pot_num);}
	else{
		LCDClear();
		LCDWriteString("PotSel ERROR!");
		return(0);}
}

void output_on_beat(){
	
	switch (beat){
		case 1:
			if (temp1 == 0){
				led_shifting("10000000");
				midi(0x80,note8,0x7f);
				midi(0x90,note1,0x7f);
			}
			else if (temp1 == 1){
				led_shifting("00000000");
				midi(0x80,note8,0x7f);
				midi(0x80,note1,0x7f);
			}
		break;
		case 2:
			if (temp2 == 0){
				led_shifting("01000000");
				midi(0x80,note1,0x7f);
				midi(0x90,note2,0x7f);
			}
			else if (temp2 == 1){
				led_shifting("00000000");
				midi(0x80,note1,0x7f);
				midi(0x80,note2,0x7f);
			}
		break;
		case 3:
			if (temp3 == 0){
				led_shifting("00100000");
				midi(0x80,note2,0x7f);
				midi(0x90,note3,0x7f);
			}
			else if (temp3 == 1){
				led_shifting("00000000");
				midi(0x80,note2,0x7f);
				midi(0x80,note3,0x7f);
			}
		break;
		case 4:
			if (temp4 == 0){
				led_shifting("00010000");
				midi(0x80,note3,0x7f);
				midi(0x90,note4,0x7f);
			}
			else if (temp4 == 1){
				led_shifting("00000000");
				midi(0x80,note3,0x7f);
				midi(0x80,note4,0x7f);
			}
		break;
		case 5:
			if (temp5 == 0){
				led_shifting("000010000");
				midi(0x80,note4,0x7f);
				midi(0x90,note5,0x7f);
			}
			else if (temp5 == 1){
				led_shifting("00000000");
				midi(0x80,note4,0x7f);
				midi(0x80,note5,0x7f);
			}
		break;
		case 6:
			if (temp6 == 0){
				led_shifting("00000100");
				midi(0x80,note5,0x7f);
				midi(0x90,note6,0x7f);
			}
			else if (temp6 == 1){
				led_shifting("00000000");
				midi(0x80,note5,0x7f);
				midi(0x80,note6,0x7f);
			}
		break;
		case 7:
			if (temp7 == 0){
				led_shifting("00000010");
				midi(0x80,note6,0x7f);
				midi(0x90,note7,0x7f);	
			}
			else if (temp7 == 1){
				led_shifting("00000000");
				midi(0x80,note6,0x7f);
				midi(0x80,note7,0x7f);
			}
		break;
		case 8:
			if (temp8 == 0){
				led_shifting("00000001");
				midi(0x80,note7,0x7f);
				midi(0x90,note8,0x7f);
			}
			else if (temp8 == 1){
				led_shifting("00000000");
				midi(0x80,note7,0x7f);
				midi(0x80,note8,0x7f);
			}
		break;
	}
}

void turn_midi_off(){
	
	switch (beat){
		case 1:
				midi(0x80,note1,0x7f);
		break;
		case 2:
				midi(0x80,note2,0x7f);
		break;
		case 3:
				midi(0x80,note3,0x7f);
		break;
		case 4:
				midi(0x80,note4,0x7f);
		break;
		case 5:
				midi(0x80,note5,0x7f);
		break;
		case 6:
				midi(0x80,note6,0x7f);
		break;
		case 7:
				midi(0x80,note7,0x7f);
		break;
		case 8:
				midi(0x80,note8,0x7f);
		break;
	}
}

void control_screen(){
	// Pot ADC Converter
	LCDHome();
	LCDWriteString("Control Number ");
	LCDGotoXY(15,0);
	LCDWriteInt(pot_num, 1);	
	LCDGotoXY(0,1);
	LCDWriteString("CC:");
	ADCSRA |= (1<<ADSC);
	while(ADCSRA & (1 << ADSC)){;}
	LCDGotoXY(3,1);	
	LCDWriteInt(result, 3);
}

void poll_pots(){
	// 
	ADMUX = 0b01100000;
	ADMUX = 0b01100001;
	ADMUX = 0b01100010;
	ADMUX = 0b01100011;
	ADMUX = 0b01100100;
	ADMUX = 0b01100101;
	ADMUX = 0b01100110;
	ADMUX = 0b01100111;
}
