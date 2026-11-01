start:  LI   r14, 0x8000
        LI   r13, 0xFF00
        LI   r12, 0xE000
        LIL  r1, 20
        SW   r1, 0(r14)
        SW   r1, 2(r14)
        SW   r0, 12(r14)
        SW   r0, 14(r14)
        ADDI r1, r0, 1
        CALL serve
        LI   r1, title
        CALL puts

frame:  LW   r1, 16(r13)
        LIL  r2, 0x20
        AND  r2, r1, r2
        BEQZ r2, alive
        HALT

alive:  LIL  r8, 40
        ADDI r3, r0, 1
        ADDI r4, r0, 2
        LW   r2, 0(r14)
        CALL paddle
        SW   r2, 0(r14)
        ADDI r3, r0, 4
        LIL  r4, 8
        LW   r2, 2(r14)
        CALL paddle
        SW   r2, 2(r14)

        LW   r2, 16(r14)
        BEQZ r2, move
        LIL  r2, 0x10
        AND  r2, r1, r2
        BEQZ r2, draw
        SW   r0, 16(r14)

move:   LW   r2, 20(r13)
        ADDI r3, r0, 1
        AND  r2, r2, r3
        BNEZ r2, draw
        LW   r2, 4(r14)
        LW   r3, 6(r14)
        LW   r4, 8(r14)
        LW   r5, 10(r14)
        ADD  r2, r2, r4
        ADD  r3, r3, r5

        BNEZ r3, nottop
        ADDI r5, r0, 1
nottop: LIL  r6, 47
        SUB  r6, r3, r6
        BNEZ r6, notbot
        ADDI r5, r0, -1

notbot: LIL  r8, 8
        ADDI r6, r2, -3
        BNEZ r6, noleft
        LW   r7, 0(r14)
        SUB  r7, r3, r7
        SLT  r7, r7, r8
        BEQZ r7, noleft
        ADDI r4, r0, 1
noleft: LIL  r6, 60
        SUB  r6, r2, r6
        BNEZ r6, noright
        LW   r7, 2(r14)
        SUB  r7, r3, r7
        SLT  r7, r7, r8
        BEQZ r7, noright
        ADDI r4, r0, -1

noright: SW  r2, 4(r14)
        SW   r3, 6(r14)
        SW   r4, 8(r14)
        SW   r5, 10(r14)

        BNEZ r2, nolmiss
        LW   r6, 14(r14)
        ADDI r6, r6, 1
        SW   r6, 14(r14)
        ADDI r1, r0, -1
        CALL serve
        CALL score
        JMP  draw
nolmiss: LIL r6, 63
        SUB  r6, r2, r6
        BNEZ r6, draw
        LW   r6, 12(r14)
        ADDI r6, r6, 1
        SW   r6, 12(r14)
        ADDI r1, r0, 1
        CALL serve
        CALL score

draw:   CALL clear
        MOV  r10, r0
dash:   LIL  r1, 32
        MOV  r2, r10
        ADDI r3, r0, 1
        ADDI r4, r0, 2
        LIL  r5, 8
        CALL rect
        ADDI r10, r10, 4
        LIL  r1, 48
        SLT  r1, r10, r1
        BNEZ r1, dash

        ADDI r1, r0, 2
        LW   r2, 0(r14)
        ADDI r3, r0, 1
        LIL  r4, 8
        LIL  r5, 15
        CALL rect
        LIL  r1, 61
        LW   r2, 2(r14)
        ADDI r3, r0, 1
        LIL  r4, 8
        LIL  r5, 15
        CALL rect
        LW   r1, 4(r14)
        LW   r2, 6(r14)
        ADDI r3, r0, 1
        ADDI r4, r0, 1
        LIL  r5, 14
        CALL rect

        SW   r0, 18(r13)
        JAL  frame

paddle: AND  r5, r1, r3
        BEQZ r5, pdown
        BEQZ r2, pdown
        ADDI r2, r2, -1
pdown:  AND  r5, r1, r4
        BEQZ r5, pdone
        SLT  r5, r2, r8
        BEQZ r5, pdone
        ADDI r2, r2, 1
pdone:  RET

serve:  LIL  r2, 32
        SW   r2, 4(r14)
        SW   r1, 8(r14)
        LW   r2, 20(r13)
        LIL  r3, 31
        AND  r3, r2, r3
        ADDI r3, r3, 7
        SW   r3, 6(r14)
        LIL  r3, 32
        AND  r3, r2, r3
        ADDI r2, r0, 1
        BNEZ r3, sdown
        ADDI r2, r0, -1
sdown:  SW   r2, 10(r14)
        ADDI r2, r0, 1
        SW   r2, 16(r14)
        RET

clear:  MOV  r1, r12
        LIL  r2, 96
        LIL  r3, 32
cloop:  SW   r0, 0(r1)
        SW   r0, 2(r1)
        SW   r0, 4(r1)
        SW   r0, 6(r1)
        SW   r0, 8(r1)
        SW   r0, 10(r1)
        SW   r0, 12(r1)
        SW   r0, 14(r1)
        SW   r0, 16(r1)
        SW   r0, 18(r1)
        SW   r0, 20(r1)
        SW   r0, 22(r1)
        SW   r0, 24(r1)
        SW   r0, 26(r1)
        SW   r0, 28(r1)
        SW   r0, 30(r1)
        ADD  r1, r1, r3
        ADDI r2, r2, -1
        BNEZ r2, cloop
        RET

rect:   ADDI r6, r0, 6
        SLL  r7, r2, r6
        ADD  r7, r7, r1
        ADD  r7, r7, r12
        LIL  r9, 64
rrow:   MOV  r8, r7
        MOV  r6, r3
rcol:   SB   r5, (r8)
        ADDI r8, r8, 1
        ADDI r6, r6, -1
        BNEZ r6, rcol
        ADD  r7, r7, r9
        ADDI r4, r4, -1
        BNEZ r4, rrow
        RET

puts:   LB   r2, (r1)
        BEQZ r2, putsend
        SB   r2, (r13)
        ADDI r1, r1, 1
        JMP  puts
putsend: RET

putnum: MOV  r3, r0
        LIL  r4, 10
ptens:  SLT  r2, r1, r4
        BNEZ r2, pones
        SUB  r1, r1, r4
        ADDI r3, r3, 1
        JMP  ptens
pones:  LIL  r4, 48
        BEQZ r3, plast
        ADD  r2, r3, r4
        SB   r2, (r13)
plast:  ADD  r2, r1, r4
        SB   r2, (r13)
        RET

score:  SW   r15, 18(r14)
        LI   r1, sleft
        CALL puts
        LW   r1, 12(r14)
        CALL putnum
        LI   r1, sright
        CALL puts
        LW   r1, 14(r14)
        CALL putnum
        LIL  r1, 10
        SB   r1, (r13)
        LW   r15, 18(r14)
        RET

title:  .asciz "PONG  left: W/S  right: Up/Down  space: serve  esc: quit\n"
sleft:  .asciz "Left "
sright: .asciz "  Right "

