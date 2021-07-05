#program that prints hello world
.data
msg: .cstring "Hello, world!\n"
.text
add r1, r0, &msg  #load the string address
int 0             #set the interrupt code to print string(0
