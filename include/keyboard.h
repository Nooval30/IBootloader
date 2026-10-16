#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_UP   0x100        /* special keys come back as values above 0xFF */
#define KEY_DOWN 0x101

void keyboard_init(void);
int  keyboard_empty(void);
int  keyboard_pop(void);      /* next character or KEY_*, or -1 if nothing is buffered */

#endif
