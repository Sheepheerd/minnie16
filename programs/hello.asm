        LI   r13, 0xFF00
        LI   r1, msg
loop:   LB   r2, (r1)
        BEQZ r2, done
        SB   r2, (r13)
        ADDI r1, r1, 1
        JMP  loop
done:   HALT

msg:    .asciz "Hello from minnie16!\n"

