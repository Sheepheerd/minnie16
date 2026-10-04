; sums 1..10 and stores the result (55 = 0x37) at 0x0100

        LI   r1, 10        ; n
        MOV  r2, r0        ; sum = 0
        CALL sum
        LI   r3, 0x0100
        SW   r2, 0(r3)
        JMP  done
        NOP                ; skipped
done:   HALT

sum:    ADD  r2, r2, r1
        ADDI r1, r1, -1
        BNEZ r1, sum
        RET
