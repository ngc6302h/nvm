#program to calculate first 40 fibonacci numbers
start:
add r4, r0, 40       #set the loop counter to 40
xor r1, r1, r1       #zero r1
xor r5, r5, r5       #zero r5
int 0x04             #print 0, the first number of the sequence
add r2, r0, 1        #set r2 to 1
add r1, r0, 1        #set r1 to 1
int 0x04             #print 1, the second number of the sequence
loop:                #this is a tag for the assembler
add r3, r5, r2       #calculate the next number in the sequence
add r1, r0, r3       #copy r3 to r1
int 0x04             #print it
add r5, r0, r2       #save the last last number
add r2, r0, r3       #save the last number
jmp loop if r4 != r0 #loop while r4 != 0
