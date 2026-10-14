#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
int  keyboard_empty(void);
int  keyboard_pop(void);          /* next character, or -1 if nothing is buffered */

#endif
