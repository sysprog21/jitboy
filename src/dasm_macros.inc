|.macro clean_flag, mask
    | push rax
    | lahf
    | and ah, ~mask
    | sahf
    | pop rax
|.endmacro

|.macro set_flag, mask
    | push rax
    | lahf
    | or ah, mask
    | sahf
    | pop rax
|.endmacro

|.macro write_byte, addr, value
    | pushfq
    | push r0
    | push r1
    | push r2
    | push r6
    | push r7
    | push r8
    | push r9
    | push r10
    | push r11
    | mov rArg1, state
    | mov rArg2, addr
    | mov rArg3, value
    | mov rax, &gb_memory_write
    | call rax
    | .nop 1
    | pop r11
    | pop r10
    | pop r9
    | pop r8
    | pop r7
    | pop r6
    | pop r2
    | pop r1
    | pop r0
    | popfq
|.endmacro

|.if DEBUG
|.macro print, text
    | pushfq
    | push r0
    | push r1
    | push r2
    | push r6
    | push r7
    | push r8
    | push r9
    | push r10
    | push r11
    | mov rArg1, text
    | mov rax, &puts
    | call rax
    | .nop 1
    | pop r11
    | pop r10
    | pop r9
    | pop r8
    | pop r7
    | pop r6
    | pop r2
    | pop r1
    | pop r0
    | popfq
|.endmacro
|.else
|.define print, .nop
|.endif

|.if DEBUG
|| static void printnum(uint16_t num) { printf("%i", num); }
|.endif

|.if DEBUG
|.macro printi, addr
    | pushfq
    | push r0
    | push r1
    | push r2
    | push r6
    | push r7
    | push r8
    | push r9
    | push r10
    | push r11
    | mov rArg1, addr
    | mov rax, &printnum
    | call rax
    | .nop 1
    | pop r11
    | pop r10
    | pop r9
    | pop r8
    | pop r7
    | pop r6
    | pop r2
    | pop r1
    | pop r0
    | popfq
|.endmacro
|.else
|.define printi, .nop
|.endif

|.macro bt_call
    || if (inst->opcode == CALL) {
    |      mov byte state->trap_reason, REASON_CALL
    || }
    || if (inst->opcode == RST) {
    |      mov byte state->trap_reason, REASON_RST
    || }
|.endmacro

|.macro bt_ret
    | mov byte state->trap_reason, REASON_RET
|.endmacro

|.macro prologue
    | push rbx
    | push rsp
    | push rbp
    | push r12
    | push r13
    | push r14
    | push r15
    | mov aState, rArg1
    | mov xA, 0
    | mov A, state->a
    | mov xB, 0
    | mov B, state->b
    | mov xC, 0
    | mov C, state->c
    | mov xD, 0
    | mov D, state->d
    | mov xE, 0
    | mov E, state->e
    | mov xH, 0
    | mov H, state->h
    | mov xL, 0
    | mov L, state->l
    | mov xSP, 0
    | mov SP, state->_sp
    | mov tmp1, state->mem
    | mov aMem, [tmp1 + offsetof(gb_memory, mem)]
    | mov tmp2, state->flags
    | push tmp2
|.endmacro

|.macro return, addr
    | pop tmp2
    | mov state->flags, tmp2
    | mov state->a, A
    | mov state->b, B
    | mov state->c, C
    | mov state->d, D
    | mov state->e, E
    | mov state->h, H
    | mov state->l, L
    | mov state->_sp, SP
    | pop r15
    | pop r14
    | pop r13
    | pop r12
    | pop rbp
    | pop rsp
    | pop rbx
    | mov rRet, addr
    | ret
|.endmacro

|.macro inst1, opcode, op1
    || switch (op1) {
    || case REG_A:
    |      opcode A
    ||     break;
    || case REG_B:
    |      opcode B
    ||     break;
    || case REG_C:
    |      opcode C
    ||     break;
    || case REG_D:
    |      opcode D
    ||     break;
    || case REG_E:
    |      opcode E
    ||     break;
    || case REG_H:
    |      opcode H
    ||     break;
    || case REG_L:
    |      opcode L
    ||     break;
    || case MEM_HL:
    |      pushfq
    |      and xL, 0xff
    |      and xH, 0xff
    |      mov tmp2, xH
    |      shl tmp2, 8
    |      mov tmp1, xL
    |      add tmp1, tmp2
    |      popfq
    |      opcode byte [aMem + tmp1]
    ||     break;
    || default:
    ||     LOG_ERROR("Invalid operand to opcode\n");
    ||     return false;
    || }
|.endmacro

|.macro inst2, opcode, op1, arg2
    || switch (op1) {
    || case REG_A:
    |      opcode A, arg2
    ||     break;
    || case REG_B:
    |      opcode B, arg2
    ||     break;
    || case REG_C:
    |      opcode C, arg2
    ||     break;
    || case REG_D:
    |      opcode D, arg2
    ||     break;
    || case REG_E:
    |      opcode E, arg2
    ||     break;
    || case REG_H:
    |      opcode H, arg2
    ||     break;
    || case REG_L:
    |      opcode L, arg2
    ||     break;
    || case MEM_HL:
    |      pushfq
    |      and xL, 0xff
    |      and xH, 0xff
    |      mov tmp2, xH
    |      shl tmp2, 8
    |      mov tmp1, xL
    |      add tmp1, tmp2
    |      popfq
    |      opcode byte [aMem + tmp1], arg2
    ||     break;
    || default:
    ||     LOG_ERROR("Invalid operand to opcode\n");
    ||     return false;
    || }
|.endmacro

|.macro inst, opcode, op1, op2
    || switch (op1) {
    || case REG_A:
    ||     switch (op2) {
    ||     case REG_A:
    |          opcode A, A
    ||         break;
    ||     case REG_B:
    |          opcode A, B
    ||         break;
    ||     case REG_C:
    |          opcode A, C
    ||         break;
    ||     case REG_D:
    |          opcode A, D
    ||         break;
    ||     case REG_E:
    |          opcode A, E
    ||         break;
    ||     case REG_H:
    |          opcode A, H
    ||         break;
    ||     case REG_L:
    |          opcode A, L
    ||         break;
    ||     case IMM8:
    |          opcode A, (int8_t)inst->args[1]
    ||         break;
    ||     case MEM_8:
    |          opcode A, [aMem + (0xff00 + inst->args[1])]
    ||         break;
    ||     case MEM_C:
    |          and xC, 0xff
    |          opcode A, [aMem + xC + 0xff00]
    ||         break;
    ||     case MEM_HL:
    |          pushfq
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          popfq
    |          opcode A, [aMem + tmp1]
    ||         break;
    ||     case MEM_BC:
    |          pushfq
    |          and xC, 0xff
    |          and xB, 0xff
    |          mov tmp2, xB
    |          shl tmp2, 8
    |          mov tmp1, xC
    |          add tmp1, tmp2
    |          popfq
    |          opcode A, [aMem + tmp1]
    ||         break;
    ||     case MEM_DE:
    |          pushfq
    |          and xE, 0xff
    |          and xD, 0xff
    |          mov tmp2, xD
    |          shl tmp2, 8
    |          mov tmp1, xE
    |          add tmp1, tmp2
    |          popfq
    |          opcode A, [aMem + tmp1]
    ||         break;
    ||     case MEM_16: {
    ||         uint16_t addr = inst->args[1] + 256 * inst->args[2];
    |          opcode A, [aMem + addr]
    ||         break;
    ||     }
    ||     case MEM_DEC_HL:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          opcode A, [aMem + tmp1]
    |          dec tmp1
    |          mov L, tmp1b
    |          shr tmp1, 8
    |          mov H, tmp1b
    ||         break;
    ||     case MEM_INC_HL:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          opcode A, [aMem + tmp1]
    |          inc tmp1
    |          mov L, tmp1b
    |          shr tmp1, 8
    |          mov H, tmp1b
    ||         break;
    ||     default:
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case REG_B:
    ||     switch (op2) {
    ||     case REG_A:
    |          opcode B, A
    ||         break;
    ||     case REG_B:
    |          opcode B, B
    ||         break;
    ||     case REG_C:
    |          opcode B, C
    ||         break;
    ||     case REG_D:
    |          opcode B, D
    ||         break;
    ||     case REG_E:
    |          opcode B, E
    ||         break;
    ||     case REG_H:
    |          opcode B, H
    ||         break;
    ||     case REG_L:
    |          opcode B, L
    ||         break;
    ||     case MEM_HL:
    |          pushfq
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          popfq
    |          opcode B, [aMem + tmp1]
    ||         break;
    ||     case IMM8:
    |          opcode B, inst->args[1]
    ||         break;
    ||     default:
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case REG_C:
    ||     switch (op2) {
    ||     case REG_A:
    |          opcode C, A
    ||         break;
    ||     case REG_B:
    |          opcode C, B
    ||         break;
    ||     case REG_C:
    |          opcode C, C
    ||         break;
    ||     case REG_D:
    |          opcode C, D
    ||         break;
    ||     case REG_E:
    |          opcode C, E
    ||         break;
    ||     case REG_H:
    |          opcode C, H
    ||         break;
    ||     case REG_L:
    |          opcode C, L
    ||         break;
    ||     case MEM_HL:
    |          pushfq
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          popfq
    |          opcode C, [aMem + tmp1]
    ||         break;
    ||     case IMM8:
    |          opcode C, inst->args[1]
    ||         break;
    ||     default:
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case REG_D:
    ||     switch (op2) {
    ||     case REG_A:
    |          opcode D, A
    ||         break;
    ||     case REG_B:
    |          opcode D, B
    ||         break;
    ||     case REG_C:
    |          opcode D, C
    ||         break;
    ||     case REG_D:
    |          opcode D, D
    ||         break;
    ||     case REG_E:
    |          opcode D, E
    ||         break;
    ||     case REG_H:
    |          opcode D, H
    ||         break;
    ||     case REG_L:
    |          opcode D, L
    ||         break;
    ||     case MEM_HL:
    |          pushfq
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          popfq
    |          opcode D, [aMem + tmp1]
    ||         break;
    ||     case IMM8:
    |          opcode D, inst->args[1]
    ||         break;
    ||     default:
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case REG_E:
    ||     switch (op2) {
    ||     case REG_A:
    |          opcode E, A
    ||         break;
    ||     case REG_B:
    |          opcode E, B
    ||         break;
    ||     case REG_C:
    |          opcode E, C
    ||         break;
    ||     case REG_D:
    |          opcode E, D
    ||         break;
    ||     case REG_E:
    |          opcode E, E
    ||         break;
    ||     case REG_H:
    |          opcode E, H
    ||         break;
    ||     case REG_L:
    |          opcode E, L
    ||         break;
    ||     case MEM_HL:
    |          pushfq
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          popfq
    |          opcode E, [aMem + tmp1]
    ||         break;
    ||     case IMM8:
    |          opcode E, inst->args[1]
    ||         break;
    ||     default:
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case REG_H:
    ||     switch (op2) {
    ||     case REG_A:
    |          opcode H, A
    ||         break;
    ||     case REG_B:
    |          opcode H, B
    ||         break;
    ||     case REG_C:
    |          opcode H, C
    ||         break;
    ||     case REG_D:
    |          opcode H, D
    ||         break;
    ||     case REG_E:
    |          opcode H, E
    ||         break;
    ||     case REG_H:
    |          opcode H, H
    ||         break;
    ||     case REG_L:
    |          opcode H, L
    ||         break;
    ||     case MEM_HL:
    |          pushfq
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          popfq
    |          opcode H, [aMem + tmp1]
    ||         break;
    ||     case IMM8:
    |          opcode H, inst->args[1]
    ||         break;
    ||     default:
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case REG_L:
    ||     switch (op2) {
    ||     case REG_A:
    |          opcode L, A
    ||         break;
    ||     case REG_B:
    |          opcode L, B
    ||         break;
    ||     case REG_C:
    |          opcode L, C
    ||         break;
    ||     case REG_D:
    |          opcode L, D
    ||         break;
    ||     case REG_E:
    |          opcode L, E
    ||         break;
    ||     case REG_H:
    |          opcode L, H
    ||         break;
    ||     case REG_L:
    |          opcode L, L
    ||         break;
    ||     case MEM_HL:
    |          pushfq
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          popfq
    |          opcode L, [aMem + tmp1]
    ||         break;
    ||     case IMM8:
    |          opcode L, inst->args[1]
    ||         break;
    ||     default:
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_HL:
    ||     switch (op2) {
    ||     case REG_A:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xA
    |.else
    |          opcode [aMem + tmp1], A
    |.endif    
    ||         break;
    ||     case REG_B:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xB
    |.else
    |          opcode [aMem + tmp1], B
    |.endif    
    ||         break;
    ||     case REG_C:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xC
    |.else
    |          opcode [aMem + tmp1], C
    |.endif    
    ||         break;
    ||     case REG_D:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xD
    |.else
    |          opcode [aMem + tmp1], D
    |.endif    
    ||         break;
    ||     case REG_E:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xE
    |.else
    |          opcode [aMem + tmp1], E
    |.endif    
    ||         break;
    ||     case REG_H:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xH
    |.else
    |          opcode [aMem + tmp1], H
    |.endif    
    ||         break;
    ||     case REG_L:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xL
    |.else
    |          opcode [aMem + tmp1], L
    |.endif    
    ||         break;
    ||     case IMM8:
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, inst->args[1]
    |.else
    |          opcode byte [aMem + tmp1], inst->args[1]
    |.endif    
    ||         break;
    ||     default:
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_BC:
    ||     if (op2 == REG_A) {
    |          and xC, 0xff
    |          and xB, 0xff
    |          mov tmp2, xB
    |          shl tmp2, 8
    |          mov tmp1, xC
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xA
    |.else
    |          opcode [aMem + tmp1], A
    |.endif    
    ||     } else {
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_DE:
    ||     if (op2 == REG_A) {
    |          and xE, 0xff
    |          and xD, 0xff
    |          mov tmp2, xD
    |          shl tmp2, 8
    |          mov tmp1, xE
    |          add tmp1, tmp2
    |.if 'opcode' == 'mov'
    |          write_byte tmp1, xA
    |.else
    |          opcode [aMem + tmp1], A
    |.endif    
    ||     } else {
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_DEC_HL:
    ||     if (op2 == REG_A) {
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          write_byte tmp1, xA
    |          dec tmp1
    |          mov L, tmp1b
    |          shr tmp1, 8
    |          mov H, tmp1b
    ||     } else {
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_INC_HL:
    ||     if (op2 == REG_A) {
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp1, xL
    |          add tmp1, tmp2
    |          write_byte tmp1, xA
    |          inc tmp1
    |          mov L, tmp1b
    |          shr tmp1, 8
    |          mov H, tmp1b
    ||     } else {
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_INC_DE:
    ||     if (op2 == MEM_INC_HL) {
    |          and xE, 0xff
    |          and xD, 0xff
    |          and xL, 0xff
    |          and xH, 0xff
    |          mov tmp2, xD
    |          shl tmp2, 8
    |          mov tmp1, xE
    |          add tmp1, tmp2
    |          mov tmp2, xH
    |          shl tmp2, 8
    |          mov tmp3, xL
    |          add tmp3, tmp2
    |          mov A, [aMem+tmp3]
    |          write_byte tmp1, xA
    |          inc tmp1
    |          mov E, tmp1b
    |          shr tmp1, 8
    |          mov D, tmp1b
    |          inc tmp3
    |          mov L, tmp3b
    |          shr tmp3, 8
    |          mov H, tmp3b
    ||     } else {
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_8:
    ||     if (op2 == REG_A) {
    |          write_byte (0xff00 + inst->args[1]), xA
    ||     } else {
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_C:
    ||     if (op2 == REG_A) {
    |          and xC, 0xff
    |          add xC, 0xff00
    |          write_byte xC, xA
    ||     } else {
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || case MEM_16:
    ||     if (op2 == REG_A) { /* TODO: implement compile-time memory check */
    ||         uint16_t addr = inst->args[2] * 256 + inst->args[1];
    |          write_byte addr, xA
    ||     } else {
    ||         LOG_ERROR("Unsupported operand op2=%i to opcode\n", op2);
    ||         return false;
    ||     }
    ||     break;
    || default:
    ||     LOG_ERROR("Unsupported operand op1=%i to opcode\n", op1);
    ||     return false;
    || }
|.endmacro

|.macro bitinst, opcode, op1, op2, prefix
    || switch (op1) {
    || case REG_A:
    ||     switch (op2) {
    ||     case BIT_0:
    |          opcode A, prefix 0x01
    ||         break;
    ||     case BIT_1:
    |          opcode A, prefix 0x02
    ||         break;
    ||     case BIT_2:
    |          opcode A, prefix 0x04
    ||         break;
    ||     case BIT_3:
    |          opcode A, prefix 0x08
    ||         break;
    ||     case BIT_4:
    |          opcode A, prefix 0x10
    ||         break;
    ||     case BIT_5:
    |          opcode A, prefix 0x20
    ||         break;
    ||     case BIT_6:
    |          opcode A, prefix 0x40
    ||         break;
    ||     case BIT_7:
    |          opcode A, prefix 0x80
    ||         break;
    ||     default:
    ||         LOG_ERROR("Invalid 2nd operand to opcode.\n");
    ||         return false;
    ||     }
    ||     break;
    || case REG_B:
    ||     switch (op2) {
    ||     case BIT_0:
    |          opcode B, prefix 0x01
    ||         break;
    ||     case BIT_1:
    |          opcode B, prefix 0x02
    ||         break;
    ||     case BIT_2:
    |          opcode B, prefix 0x04
    ||         break;
    ||     case BIT_3:
    |          opcode B, prefix 0x08
    ||         break;
    ||     case BIT_4:
    |          opcode B, prefix 0x10
    ||         break;
    ||     case BIT_5:
    |          opcode B, prefix 0x20
    ||         break;
    ||     case BIT_6:
    |          opcode B, prefix 0x40
    ||         break;
    ||     case BIT_7:
    |          opcode B, prefix 0x80
    ||         break;
    ||     default:
    ||         LOG_ERROR("Invalid 2nd operand to opcode.\n");
    ||         return false;
    ||     }
    ||     break;
    || case REG_C:
    ||     switch (op2) {
    ||     case BIT_0:
    |          opcode C, prefix 0x01
    ||         break;
    ||     case BIT_1:
    |          opcode C, prefix 0x02
    ||         break;
    ||     case BIT_2:
    |          opcode C, prefix 0x04
    ||         break;
    ||     case BIT_3:
    |          opcode C, prefix 0x08
    ||         break;
    ||     case BIT_4:
    |          opcode C, prefix 0x10
    ||         break;
    ||     case BIT_5:
    |          opcode C, prefix 0x20
    ||         break;
    ||     case BIT_6:
    |          opcode C, prefix 0x40
    ||         break;
    ||     case BIT_7:
    |          opcode C, prefix 0x80
    ||         break;
    ||     default:
    ||         LOG_ERROR("Invalid 2nd operand to opcode.\n");
    ||         return false;
    ||     }
    ||     break;
    || case REG_D:
    ||     switch (op2) {
    ||     case BIT_0:
    |          opcode D, prefix 0x01
    ||         break;
    ||     case BIT_1:
    |          opcode D, prefix 0x02
    ||         break;
    ||     case BIT_2:
    |          opcode D, prefix 0x04
    ||         break;
    ||     case BIT_3:
    |          opcode D, prefix 0x08
    ||         break;
    ||     case BIT_4:
    |          opcode D, prefix 0x10
    ||         break;
    ||     case BIT_5:
    |          opcode D, prefix 0x20
    ||         break;
    ||     case BIT_6:
    |          opcode D, prefix 0x40
    ||         break;
    ||     case BIT_7:
    |          opcode D, prefix 0x80
    ||         break;
    ||     default:
    ||         LOG_ERROR("Invalid 2nd operand to opcode.\n");
    ||         return false;
    ||     }
    ||     break;
    || case REG_E:
    ||     switch (op2) {
    ||     case BIT_0:
    |          opcode E, prefix 0x01
    ||         break;
    ||     case BIT_1:
    |          opcode E, prefix 0x02
    ||         break;
    ||     case BIT_2:
    |          opcode E, prefix 0x04
    ||         break;
    ||     case BIT_3:
    |          opcode E, prefix 0x08
    ||         break;
    ||     case BIT_4:
    |          opcode E, prefix 0x10
    ||         break;
    ||     case BIT_5:
    |          opcode E, prefix 0x20
    ||         break;
    ||     case BIT_6:
    |          opcode E, prefix 0x40
    ||         break;
    ||     case BIT_7:
    |          opcode E, prefix 0x80
    ||         break;
    ||     default:
    ||         LOG_ERROR("Invalid 2nd operand to opcode.\n");
    ||         return false;
    ||     }
    ||     break;
    || case REG_H:
    ||     switch (op2) {
    ||     case BIT_0:
    |          opcode H, prefix 0x01
    ||         break;
    ||     case BIT_1:
    |          opcode H, prefix 0x02
    ||         break;
    ||     case BIT_2:
    |          opcode H, prefix 0x04
    ||         break;
    ||     case BIT_3:
    |          opcode H, prefix 0x08
    ||         break;
    ||     case BIT_4:
    |          opcode H, prefix 0x10
    ||         break;
    ||     case BIT_5:
    |          opcode H, prefix 0x20
    ||         break;
    ||     case BIT_6:
    |          opcode H, prefix 0x40
    ||         break;
    ||     case BIT_7:
    |          opcode H, prefix 0x80
    ||         break;
    ||     default:
    ||         LOG_ERROR("Invalid 2nd operand to opcode.\n");
    ||         return false;
    ||     }
    ||     break;
    || case REG_L:
    ||     switch (op2) {
    ||     case BIT_0:
    |          opcode L, prefix 0x01
    ||         break;
    ||     case BIT_1:
    |          opcode L, prefix 0x02
    ||         break;
    ||     case BIT_2:
    |          opcode L, prefix 0x04
    ||         break;
    ||     case BIT_3:
    |          opcode L, prefix 0x08
    ||         break;
    ||     case BIT_4:
    |          opcode L, prefix 0x10
    ||         break;
    ||     case BIT_5:
    |          opcode L, prefix 0x20
    ||         break;
    ||     case BIT_6:
    |          opcode L, prefix 0x40
    ||         break;
    ||     case BIT_7:
    |          opcode L, prefix 0x80
    ||         break;
    ||     default:
    ||         LOG_ERROR("Invalid 2nd operand to opcode.\n");
    ||         return false;
    ||     }
    ||     break;
    || case MEM_HL:
    |      and xL, 0xff
    |      and xH, 0xff
    |      mov tmp2, xH
    |      shl tmp2, 8
    |      mov tmp1, xL
    |      add tmp1, tmp2
    ||     switch (op2) {
    ||     case BIT_0:
    |          opcode byte [aMem + tmp1], prefix 0x01
    ||         break;
    ||     case BIT_1:
    |          opcode byte [aMem + tmp1], prefix 0x02
    ||         break;
    ||     case BIT_2:
    |          opcode byte [aMem + tmp1], prefix 0x04
    ||         break;
    ||     case BIT_3:
    |          opcode byte [aMem + tmp1], prefix 0x08
    ||         break;
    ||     case BIT_4:
    |          opcode byte [aMem + tmp1], prefix 0x10
    ||         break;
    ||     case BIT_5:
    |          opcode byte [aMem + tmp1], prefix 0x20
    ||         break;
    ||     case BIT_6:
    |          opcode byte [aMem + tmp1], prefix 0x40
    ||         break;
    ||     case BIT_7:
    |          opcode byte [aMem + tmp1], prefix 0x80
    ||         break;
    ||     default:
    ||         LOG_ERROR("Invalid 2nd operand to opcode.\n");
    ||         return false;
    ||     }
    ||     break;
    || default:
    ||     LOG_ERROR("Invalid 1st operand to opcode.\n");
    ||     return false;
    || }
|.endmacro
