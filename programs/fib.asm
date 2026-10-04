; writes the first 10 fibonacci numbers as words starting at 0x0100

        LI   r1, 0x0100    ; where to put them
        LI   r4, 10        ; how many
        CALL fib
        HALT

; fib: r1 = destination, r4 = count
fib:    MOV  r2, r0        ; a = 0
        ADDI r3, r0, 1     ; b = 1
loop:   SW   r2, 0(r1)     ; store a
        ADD  r5, r2, r3    ; next = a + b
        MOV  r2, r3        ; a = b
        MOV  r3, r5        ; b = next
        ADDI r1, r1, 2     ; next word
        ADDI r4, r4, -1
        BNEZ r4, loop
        RET
