// All necessary declarations for the Tux Controller driver must be in this file

#ifndef TUXCTL_H
#define TUXCTL_H
#define NUM_HEX 16
#define LED_NUM 4
#define DECIMAL_POS 0x0010
#define BUTTON_LEFT 5
#define BUTTON_DOWN 6
#define SIZEOF_DATA 4
#define NUM_BUF LED_NUM+2


#define TUX_SET_LED _IOR('E', 0x10, unsigned long)
#define TUX_READ_LED _IOW('E', 0x11, unsigned long*)
#define TUX_BUTTONS _IOW('E', 0x12, unsigned long*)
#define TUX_INIT _IO('E', 0x13)
#define TUX_LED_REQUEST _IO('E', 0x14)
#define TUX_LED_ACK _IO('E', 0x15)

#endif
