;//*********************************************************************
;//*               SEGGER MICROCONTROLLER SYSTEME GmbH                  *
;//*       Solutions for real time microcontroller applications         *
;//**********************************************************************
;//*                                                                    *
;//*       (C) 2007 - 2008   SEGGER Microcontroller Systeme GmbH        *
;//*                                                                    *
;//*       www.segger.com     Support: support@segger.com               *
;//*                                                                    *
;//**********************************************************************
;//*                                                                    *
;//*       TCP/IP stack for embedded applications                       *
;//*                                                                    *
;//**********************************************************************
;//*
;//* File    : ARM_IAR_IP_cksum.asm
;//* Purpose : Efficient implementation of IP-Checksum code
;//--------- END-OF-HEADER ---------------------------------*/


#ifdef __IAR_SYSTEMS_ASM__

;/*********************************************************************
;*
;*       These defines are necessary to be able to 
;*       let run code in ram.
;*       As there are not defined in any IAR header file
;*       we need to define them here.
;*  References:  ARM ELF manual
;*                Document number: SWS ESPC 0003 B-02
;*               IAR ARM Assembler reference manual
;*                EWARM_AssemblerReference.ENU.pdf
;*/
#define SHT_PROGBITS   0x01
#define SHF_WRITE      0x01
#define SHF_EXECINSTR  0x04

;/*********************************************************************
;*
;*       IAR V5: Code in program memory
;*
;*/
;#if   (__VER__ / 1000000) == 5
        PUBLIC ARM_IP_cksum
        SECTION .textrw:CODE:NOROOT(2)                // Code is in RAM
        SECTION_TYPE SHT_PROGBITS, SHF_WRITE | SHF_EXECINSTR
        CODE32

;/*********************************************************************
;*
;*       Public code
;*
;**********************************************************************
;*/
;/*********************************************************************
;*
;*       ARM_IP_cksum
;*
;* Function description
;*   Computes internet checksum
;*
;* Register usage:
;*   R0    pData     - Must be HWord aligned
;*   R1    NumHWords
;*   R2    Sum
;*   R3 - R9, R12   Used for data transfers
;*   R13   SP
;*   R14   LR (contains return address)
;*   R15   PC
;*/

;//U16 IP_cksum(void * ptr, unsigned NumHWords) {
ARM_IP_cksum
        MOVS     R2,#+0    ;// Sum = 0
;//
;// 32-bit align pointer
;//
;// if ((int)pData & 2) {
;//   Sum += *pData++;
;//   NumHWords--;
;// }
        TST      r0, #2
        LDRNEH   r2, [r0], #2
        SUBNE    r1, r1, #1
;//
;// Fast loop, 8 words = 16 halfwords at a time
;//
        subs     R1, R1, #16
        BCC      FastLoop8Done
        stmdb    SP!, {R4-R9}   ;// Push preserved registers. Note: R3, R12 are temp and do not need to be saved.
FastLoop8
        ldmia    r0!, {r3-r9, r12}     ; // load 16 bytes
        ADDS     R2,R2,R3
        ADCS     R2,R2,R4
        ADCS     R2,R2,R5
        ADCS     R2,R2,R6
        ADCS     R2,R2,R7
        ADCS     R2,R2,R8
        ADCS     R2,R2,R9
        ADCS     R2,R2,R12
        ADC      R2,R2, #0
        SUBS     R1,R1, #16
        BCS      FastLoop8
        ldmia    SP!, {R4-R9}    ;// Pop preserved registers
FastLoop8Done
;//
;// Handle remaining full words (between 0 and 7)
;//
        mov      r3, #-1
        sub      r3, r3, r1
        mov      r3, r3, LSR #1
        adds     pc, pc, r3, LSL #3    ;// Clears Carry
        nop
        ldr      r3, [r0], #4      ; // 7 words
        adcs     r2, r2, r3
        ldr      r3, [r0], #4      ; // 6 words
        adcs     r2, r2, r3
        ldr      r3, [r0], #4      ; // 5 words
        adcs     r2, r2, r3
        ldr      r3, [r0], #4      ; // 4 words
        adcs     r2, r2, r3
        ldr      r3, [r0], #4      ; // 3 words
        adcs     r2, r2, r3
        ldr      r3, [r0], #4      ; // 2 words
        adcs     r2, r2, r3
        ldr      r3, [r0], #4      ; // 1 words
        adcs     r2, r2, r3
        adc      r2, r2, #0        ; // 0 words

;//
;// Do final half word
;//
        TST      R1, #1
        BEQ      FoldCheckSum
        LDRH     R3,[R0, #+0]
        ADDS     R2,R2,R3
        ADCS     R2,R2, #0
;//
;//  Fold 32-bit sum to 16 bits
;//
;//  while (Sum >> 16) {
;//    Sum = (Sum & 0xffff) + (Sum >> 16);
;//  }
FoldCheckSum
        MOV      r3, r2, ROR #16
        ADD      r2, r2, r3
        MOV      r2, r2, LSR #16
;//
;//   return Sum;
;//
        MOV     R0,R2
        BX      LR

        END

#endif ;//__IAR_SYSTEMS_ASM__

/**************************** End of file ***************************/

