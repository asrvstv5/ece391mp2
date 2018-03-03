/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)

//========================================================================
// ========================Declaring local functions.=======================
int tuxctl_init(struct tty_struct* tty);
int tuxctl_set_led(struct tty_struct* tty, unsigned long arg);
int tuxctl_btn(unsigned long arg);
unsigned char swapBits(unsigned char n, unsigned char p1, unsigned char p2);
//======================== Declaring global variables.======================
static unsigned char inputButton1;	//Contains info from button packets A, B, C and ST
static unsigned char inputButton2;	//Contains info from button packets up, down, left and right
static unsigned int data=0;			//Contains the integer data combined by using inputButton1 and inputButton2.
static unsigned char clear[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};	//Used to clear the LED
int ready_state = 1;				//Checks if LED is ready to take next input or not
int button_ready = 1;				// Checks if Buttons are ready to take next input or not
//==========================================================================

/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in 
 * tuxctl-ld.c. It calls this function, so all warnings there apply 
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;
    unsigned char mask;		//mask used to obtain the low 4 bits 
    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];
//=======================================================================
    mask = 0x0F;
    switch(a) {
    //If button is pressed, store the message packets into global variables.
    	case MTCP_BIOC_EVENT:

			inputButton1 = (unsigned char)(mask & b);	
			inputButton2 = (unsigned char)(mask & c);

    		break;
    //If reset is pressed, initialize the tux again and clear the LED values.
    	case MTCP_RESET:
    		tuxctl_init(tty);
    		if(ready_state == 1) {
    			ready_state = 0;
    			tuxctl_ldisc_put(tty, clear, 6);
    		}
    		break;
    //If MTCP_ACK is called then set ready state to 1. 
    	case MTCP_ACK:
    		ready_state = 1;
    		break;
    }
//=======================================================================
  //  printk("packet : %x %x %x\n", a, b, c); 
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/

int 
tuxctl_ioctl (struct tty_struct* tty, struct file* file, 
	      unsigned cmd, unsigned long arg)
{
    switch (cmd) {
 //Initialize the tux
	case TUX_INIT:
		tuxctl_init(tty);
		return 0;
 //If Tux buttons is pressed then call the helper
 	case TUX_BUTTONS:
 		if(button_ready ==1){
    		button_ready = 0;
			tuxctl_btn(arg);
		}
		return 0;
 // Set the LED to the values represented by arg.
	case TUX_SET_LED:
		if(ready_state ==1){
			ready_state = 0;
			tuxctl_set_led(tty, arg);
		}
		return 0;
	case TUX_LED_ACK:
	case TUX_LED_REQUEST:
	case TUX_READ_LED:
	default:
	    return -EINVAL;
    }
}


/*
 * tuxctl_init
 *   DESCRIPTION: Initializes the tux controller
 *   INPUTS: tty - software structure for tux controller
 *   OUTPUTS: none
 *   RETURN VALUE: Returns 0 on success.  
 *   SIDE EFFECTS: Turns Button input output on and passes control to user.
 */   
int
tuxctl_init(struct tty_struct* tty)
{
	char opcode1[1] = {MTCP_BIOC_ON};
	char opcode2[1] = {MTCP_LED_USR};
	tuxctl_ldisc_put(tty, opcode1, 1);
	tuxctl_ldisc_put(tty, opcode2, 1);
	return 0;
}

/*
 * tuxctl_set_led
 *   DESCRIPTION: Sets the LED to the value represented by pointer arg
 *   INPUTS: 32 bit integer 
 *   OUTPUTS: none
 *   RETURN VALUE: Returns 0 on success.
 *   SIDE EFFECTS:  Sets the LED to the value represented by pointer arg
 */   
int 
tuxctl_set_led(struct tty_struct* tty, unsigned long arg)
{
	
  //Mapping the characters from 0 to F on the LED.
	char map_char[NUM_HEX] = {0xE7, 0x06, 0xCB, 0x8F, 0x2E, 0xAD, 0xED, 0x86, 0xEF, 0xAE, 0xEE, 0x6D, 0xE1, 0x4F, 0xE9, 0xE8};
	int i = 0;
	unsigned long temp = arg;
 	char words[LED_NUM]; 				// array to store the 4 characters to display on the LED
	int mask = 0x000F;					// mask to obtain the last 4 bits of an integer 0x000F = 00011111
	char tuxPrint[NUM_BUF];			// buffer to pass to put
	char specs[LED_NUM];	
	char temp2;

// storing the 4 characters into the array words
	for(i=0; i<LED_NUM; i++, temp = temp>>4)
	{
		words[i]=(char)(temp & mask);
	}
//storing the values into the buffer
	tuxPrint[0]= MTCP_LED_SET;
	tuxPrint[1]= 0xF;
//STORING THE LED values on the buffer
	temp2 = ( char)(mask & temp);
	mask = 0x0001;				// Resetting the mask to obtain 1 bit at a time. 
	for(i=0; i<LED_NUM; i++)
	{
		specs[i] = mask & temp2;
		temp2 = temp2>>1; 
	}

	temp = temp>>8;
	mask = 0x0001;
// Mapping the LED values onto the buffer to light up the tux
	for(i=0; i<LED_NUM; i++){
		if(specs[i] != 0){
			tuxPrint[i+2] = (char)map_char[(int)words[i]];
		}
		else{
			tuxPrint[i+2]=0x0;
		}
		if((mask & temp)!=0){
			tuxPrint[i+2] = tuxPrint[i+2] | DECIMAL_POS;
		}
		mask = mask <<1;

	}

	tuxctl_ldisc_put(tty, tuxPrint, NUM_BUF);

	return 0;
}

/*
 * tuxctl_btn
 *   DESCRIPTION: Stores the current Button state into the integer whose pointer is passed
 *   INPUTS: arg-pointer to a 32 bit integer 
 *   OUTPUTS: none
 *   RETURN VALUE: Returns 0 on success. -EINVAL on failure
 *   SIDE EFFECTS: Stores the current Button state into the integer whose pointer is passed
 */   
int tuxctl_btn(unsigned long arg)
{
	unsigned char in2;	//temporary to store inputButton2
	int ret; 			//return value
  // If pointer passed points to NULL
	if((int *)arg == NULL)
	{
		return -EINVAL;
	}
//========================================
  // Storing the 2 packets into an integer : data
	in2 = inputButton2;
	in2 = in2 <<4;
	data = in2 | inputButton1;
  // Swapping bits of Left and Down
	data = swapBits(data, BUTTON_LEFT, BUTTON_DOWN);
  // Copying the integer into the required integer
	ret = copy_to_user((int *)arg,&data, SIZEOF_DATA);
	button_ready = 1;
	if(ret == 0) {
		return 0;
	}
	else {
		return -EINVAL;
	}
//========================================
}


/*
 * swapBits
 *   DESCRIPTION: Swaps 2 bits in an integer
 *   INPUT: n-integer, p1-position1, p2-position2 
 *   OUTPUTS: returned integer has the swapped bits
 *   RETURN VALUE: Returns the integer with the swapped bits
 *   SIDE EFFECTS: none
 */   
unsigned char swapBits(unsigned char n, unsigned char p1, unsigned char p2)
{
	unsigned char bit1;
	unsigned char bit2;
	unsigned char x;
	unsigned char result;
    // Move p1'th to rightmost side 
    bit1 =  (n >> p1) & 1;
    //Move p2'th to rightmost side 
    bit2 =  (n >> p2) & 1;
    // XOR the two bits 
    x = (bit1 ^ bit2);
    // Put the xor bit back to their original positions 
    x = (x << p1) | (x << p2);
    // XOR 'x' with the original number so that the two sets are swapped 
    result = n ^ x;
    return result;
}
