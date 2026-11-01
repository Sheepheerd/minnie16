        LI   r13, 0xFF00
        LI   r10, 0xFF04
        LI   r11, 0xFF02
        LIL  r12, 10
        LI   r1, prompt
        CALL puts
        LI   r8, name
        MOV  r9, r8
        LIL  r14, 63

wait:   LB   r2, (r10)
        ADDI r3, r0, 1
        AND  r3, r2, r3
        BNEZ r3, got
        BEQZ r2, wait
        JMP  done
got:    LB   r2, (r11)
        SUB  r3, r2, r12
        BEQZ r3, done
        BEQZ r14, wait
        SB   r2, (r9)
        ADDI r9, r9, 1
        ADDI r14, r14, -1
        JMP  wait

done:   SB   r0, (r9)
        LI   r1, hello
        CALL puts
        MOV  r1, r8
        CALL puts
        LI   r1, bang
        CALL puts
        HALT

puts:   LB   r2, (r1)
        BEQZ r2, pdone
        SB   r2, (r13)
        ADDI r1, r1, 1
        JMP  puts
pdone:  RET

prompt: .asciz "What's your name? "
hello:  .asciz "Hello, "
bang:   .asciz "!\n"
name:   .space 64

