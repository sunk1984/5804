;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  cstartup.s
;;  startup asm file, contains low-level initialization.
;;
;;  Copyright(C) 2010, Honeywell Integrated Technology (China) Co.,Ltd.
;;  Security FCT team
;;  All rights reserved.

;;  History
;;  2010.07.14  ver.1.00    First release by Taoist
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

AT91C_BASE_CCFG     DEFINE  0xFFFFEF10
AT91C_BASE_SMC      DEFINE  0xFFFFEC00
AT91C_BASE_PMC      DEFINE  0xFFFFFC00
AT91C_BASE_WDTC     DEFINE  0xFFFFFD40
AT91C_BASE_PIOA     DEFINE  0xFFFFF400
AT91C_BASE_PIOB     DEFINE  0xFFFFF600
AT91C_BASE_PIOC     DEFINE  0xFFFFF800
AT91C_BASE_SDRAMC   DEFINE  0xFFFFEA00


        MODULE  ?cstartup

        ;; Forward declaration of sections.
        SECTION IRQ_STACK:DATA:NOROOT(3)
        SECTION FIQ_STACK:DATA:NOROOT(3)
        SECTION CSTACK:DATA:NOROOT(3)

;
; The module in this file are included in the libraries, and may be
; replaced by any user-defined modules that define the PUBLIC symbol
; __iar_program_start or a user defined start symbol.
;
; To override the cstartup defined in the library, simply add your
; modified version to the workbench project.

        SECTION .intvec:CODE:NOROOT(2)

        PUBLIC  __vector
;        PUBLIC  __vector_0x14
        PUBLIC  __iar_program_start
        EXTERN  Undefined_Handler
        EXTERN  SWI_Handler
        EXTERN  Prefetch_Handler
        EXTERN  Abort_Handler
        EXTERN  IRQ_Handler
        EXTERN  FIQ_Handler

        ARM

__iar_init$$done:               ; The vector table is not needed
                                ; until after copy initialization is done


__vector:
        ; All default exception handlers (except reset) are
        ; defined as weak symbol definitions.
        ; If a handler is defined by the application it will take precedence.
        LDR     PC,[PC,#0x18]           ; Reset
        LDR     PC,[PC,#0x18]           ; Undefined instructions
        LDR     PC,[PC,#0x18]           ; Software interrupt (SWI/SVC)
        LDR     PC,[PC,#0x18]           ; Prefetch abort
        LDR     PC,[PC,#0x18]           ; Data abort
__vector_0x14:
        LDR     PC,[PC,#0x18]           ; while(1)
        LDR     PC,[PC,#0x18]           ; IRQ
        LDR     PC,[PC,#0x18]           ; FIQ

        DATA

Reset_Addr:     DCD   __iar_program_start
Undefined_Addr: DCD   Undefined_Handler
SWI_Addr:       DCD   SWI_Handler
Prefetch_Addr:  DCD   Prefetch_Handler
Abort_Addr:     DCD   Abort_Handler
V14:            DCD   __vector_0x14
IRQ_Addr:       DCD   IRQ_Handler
FIQ_Addr:       DCD   FIQ_Handler


; --------------------------------------------------
; ?cstartup -- low-level system initialization code.
;
; After a reset execution starts here, the mode is ARM, supervisor
; with interrupts disabled.
;



        SECTION .text:CODE:NOROOT(2)

;        PUBLIC  ?cstartup
        EXTERN  ?main
        REQUIRE __vector

        ARM

__iar_program_start:
?cstartup:

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;setup_ebi
        ;; AT91C_BASE_CCFG->CCFG_EBICSA  = 0x0001000A;   Merak
        ;; AT91C_BASE_CCFG->CCFG_EBICSA  = 0x00010002;   Thunderbird
        LDR     R0, =AT91C_BASE_CCFG
        LDR     R1, =0x0001000A
        ;; LDR     R1, =0x00010002
        STR     R1, [R0, #0x0C]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;setup_smc0
        LDR     R0, =AT91C_BASE_SMC
        ;; AT91C_BASE_SMC->SMC_SETUP0   = 0x00000002;
        LDR     R1, =0x00000002
        STR     R1, [R0, #0x00]
        ;; setup SMC_PULSE0 register
        LDR     R1, =0x0A0A0A0A
        STR     R1, [R0, #0x04]

        ;; AT91C_BASE_SMC->SMC_CYCLE0   = 0x000A000A;
        LDR     R1, =0x000A000A
        STR     R1, [R0, #0x08]

        ;; AT91C_BASE_SMC->SMC_MODE0    = 0x10001000;
        LDR     R1, =0x10001000
        STR     R1, [R0, #0x0C]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;setup_smc3
        ;; AT91C_BASE_SMC->SMC_SETUP3   = 0x00010001;
        LDR     R1, =0x00010001
        STR     R1, [R0, #0x30]

        ;; AT91C_BASE_SMC->SMC_PULSE3   = 0x03030303;
        LDR     R1, =0x03030303
        STR     R1, [R0, #0x34]

        ;; AT91C_BASE_SMC->SMC_CYCLE3   = 0x00050005;
        LDR     R1, =0x00050005
        STR     R1, [R0, #0x38]

        ;; AT91C_BASE_SMC->SMC_MODE3    = 0x00020003;
        LDR     R1, =0x00020003
        STR     R1, [R0, #0x3C]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;setup_pmc
        LDR     R0, =AT91C_BASE_PMC
        ;; AT91C_BASE_PMC->CKGR_MOR     = 0x00004001;
        LDR     R1, =0x00004001
        STR     R1, [R0, #0x20]
        ;; check AT91C_BASE_PMC->PMC_SR register MOSCS bit ( =1 OK)
pmc_wait1
        LDR     R1, [R0, #0x68]
        ANDS    R1, R1, #0x01
        BEQ     pmc_wait1
        ;; AT91C_BASE_PMC->CKGR_PLLRA   = 0x2060BF09;
        LDR     R1, =0x2060BF09
        STR     R1, [R0, #0x28]
        ;; AT91C_BASE_PMC->CKGR_PLLRB   = 0x207C3F0C;
        LDR     R1, =0x207C3F0C
        STR     R1, [R0, #0x2C]
        ;; AT91C_BASE_PMC->PMC_MCKR     = 0x00000102;
        LDR     R1, =0x00000102
        STR     R1, [R0, #0x30]
        ;; check AT91C_BASE_PMC->PMC_SR register MCKRDY bit ( =1 OK)
pmc_wait2
        LDR     R1, [R0, #0x68]
        ANDS    R1, R1, #0x08
        BEQ     pmc_wait2
        ;; AT91C_BASE_PMC->PMC_PCER     = 0x0000001C;
        LDR     R1, =0x1C
        STR     R1, [R0, #0x10]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;disable_watchdog
        ;; AT91C_BASE_WDTC->WDTC_WDMR   = 0x00008000;
        LDR     R0, =AT91C_BASE_WDTC
        LDR     R1, =0x00008000
        STR     R1, [R0, #0x04]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;setup_pioa
        ;; get Port A register base address
        LDR     R0, =AT91C_BASE_PIOA

        ;; AT91C_BASE_PIOA->PIO_ASR     = 0x003FF000;
        ;LDR     R1, =0x003FF000
        LDR     R1, =0x003FFFC0  ;for merak plus board £¬for mac and SD card
        STR     R1, [R0, #0x70]

        ;; AT91C_BASE_PIOA->PIO_BSR     = 0xF0000C00;   Merak
        ;; AT91C_BASE_PIOA->PIO_BSR     = 0x3E000C00;   Thunderbird
        ;;LDR     R1, =0xF0000C00
        ;; LDR     R1, =0x3E000C00
        LDR     R1, =0xF1800000  ;for merak plus board
        STR     R1, [R0, #0x74]

        ;; AT91C_BASE_PIOA->PIO_PER     = 0x01C003FF;   Merak
        ;; AT91C_BASE_PIOA->PIO_PER     = 0xC1C003FF;   Thunderbird
        ;;LDR     R1, =0x01C003FF  by Roger
        LDR     R1, =0x0040003F  ;for merak plus board
        ;; LDR     R1, =0xC1C003FF
        STR     R1, [R0, #0x00]

        ;; AT91C_BASE_PIOA->PIO_PDR     = 0xFE3FFC00;   Merak
        ;; AT91C_BASE_PIOA->PIO_PDR     = 0x3E3FFC00;   Thunderbird
        ;;LDR     R1, =0xFE3FFC00      by Roger
        LDR     R1, =0xFFBFFFC0   ;for merak plus board
        ;; LDR     R1, =0x3E3FFC00
        STR     R1, [R0, #0x04]

        ;; AT91C_BASE_PIOA->PIO_MDER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x50]

        ;; AT91C_BASE_PIOA->PIO_MDDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x54]

        ;; AT91C_BASE_PIOA->PIO_PPUDR   = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x60]

        ;; AT91C_BASE_PIOA->PIO_PPUER   = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x64]

        ;; AT91C_BASE_PIOA->PIO_SODR    = 0xFFFFFFFF;
        ;;LDR     R1, =0xFFFFFFF  ;; by roger
        LDR     R1, =0xFFFDFFF 
        STR     R1, [R0, #0x30]

        ;; AT91C_BASE_PIOA->PIO_CODR    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x34]

        ;; AT91C_BASE_PIOA->PIO_OWER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0xA0]

        ;; AT91C_BASE_PIOA->PIO_OWDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0xA4]

        ;; AT91C_BASE_PIOA->PIO_OER     = 0x00400000;   Merak
        ;; AT91C_BASE_PIOA->PIO_OER     = 0x00000000;   Thunderbird
        LDR     R1, =0x00400000
        ;; LDR     R1, =0x00000000
        STR     R1, [R0, #0x10]

        ;; AT91C_BASE_PIOA->PIO_ODR     = 0xFFBFFFFF;   Merak
        ;; AT91C_BASE_PIOA->PIO_ODR     = 0xFFFFFFFF;   Thunderbird
        LDR     R1, =0xFFBFFFFF
        ;; LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x14]

        ;; AT91C_BASE_PIOA->PIO_IFER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x20]

        ;; AT91C_BASE_PIOA->PIO_IFDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x24]

        ;; AT91C_BASE_PIOA->PIO_IER     = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x40]

        ;; AT91C_BASE_PIOA->PIO_IDR     = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x44]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;setup_piob
        ;; get Port B register base address
        LDR     R0, =AT91C_BASE_PIOB

        ;; AT91C_BASE_PIOB->PIO_ASR     = 0x0400CFFF;   Merak
        ;; AT91C_BASE_PIOB->PIO_ASR     = 0x00000000;   Thunderbird
        LDR     R1, =0x0400CFFF
        ;; LDR     R1, =0x00000000
        STR     R1, [R0, #0x70]

        ;; AT91C_BASE_PIOB->PIO_BSR     = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x74]

        ;; AT91C_BASE_PIOB->PIO_PER     = 0xFBFF3000;   Merak
        ;; AT91C_BASE_PIOB->PIO_PER     = 0xFFFFFFFF;   Thunderbird
        LDR     R1, =0xFBFF3000
        ;; LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x00]

        ;; AT91C_BASE_PIOB->PIO_PDR     = 0x0400CFFF;   Merak
        ;; AT91C_BASE_PIOB->PIO_PDR     = 0x00000000;   Thunderbird
        LDR     R1, =0x0400CFFF
        ;; LDR     R1, =0x00000000
        STR     R1, [R0, #0x04]

        ;; AT91C_BASE_PIOB->PIO_MDER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x50]

        ;; AT91C_BASE_PIOB->PIO_MDDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x54]

        ;; AT91C_BASE_PIOB->PIO_PPUDR   = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x60]

        ;; AT91C_BASE_PIOB->PIO_PPUER   = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x64]

        ;; AT91C_BASE_PIOB->PIO_SODR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x30]

        ;; AT91C_BASE_PIOB->PIO_CODR    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x34]

        ;; AT91C_BASE_PIOB->PIO_OWER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0xA0]

        ;; AT91C_BASE_PIOB->PIO_OWDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0xA4]

        ;; AT91C_BASE_PIOB->PIO_OER     = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x10]

        ;; AT91C_BASE_PIOB->PIO_ODR     = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x14]

        ;; AT91C_BASE_PIOB->PIO_IFER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x20]

        ;; AT91C_BASE_PIOB->PIO_IFDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x24]

        ;; AT91C_BASE_PIOB->PIO_IER     = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x40]

        ;; AT91C_BASE_PIOB->PIO_IDR     = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x44]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;setup_pioc

        ;; get Port C register base address
        LDR     R0, =AT91C_BASE_PIOC

        ;; AT91C_BASE_PIOC->PIO_ASR     = 0x00004430;   Merak
        ;; AT91C_BASE_PIOC->PIO_ASR     = 0xFFFF0C30;   Thunderbird
        LDR     R1, =0x00004430
        ;; LDR     R1, =0xFFFF0C30
        STR     R1, [R0, #0x70]

        ;; AT91C_BASE_PIOC->PIO_BSR     = 0x00000003;   Merak
        ;; AT91C_BASE_PIOC->PIO_BSR     = 0x00000000;   Thunderbird
        LDR     R1, =0x00000003
        ;; LDR     R1, =0x00000000
        STR     R1, [R0, #0x74]

        ;; AT91C_BASE_PIOC->PIO_PER     = 0xFFFFBBCC;   Merak
        ;; AT91C_BASE_PIOC->PIO_PER     = 0x0000F3CF;   Thunderbird
        LDR     R1, =0xFFFFBBCC
        ;; LDR     R1, =0x0000F3CF
        STR     R1, [R0, #0x00]

        ;; AT91C_BASE_PIOC->PIO_PDR     = 0x00004433;   Merak
        ;; AT91C_BASE_PIOC->PIO_PDR     = 0xFFFF0C30;   Thunderbird
        LDR     R1, =0x00004433
        ;; LDR     R1, =0xFFFF0C30
        STR     R1, [R0, #0x04]

        ;; AT91C_BASE_PIOC->PIO_MDER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x50]

        ;; AT91C_BASE_PIOC->PIO_MDDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x54]

        ;; AT91C_BASE_PIOC->PIO_PPUDR   = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x60]

        ;; AT91C_BASE_PIOC->PIO_PPUER   = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x64]

        ;; AT91C_BASE_PIOC->PIO_SODR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x30]

        ;; AT91C_BASE_PIOC->PIO_CODR    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x34]

        ;; AT91C_BASE_PIOC->PIO_OWER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0xA0]

        ;; AT91C_BASE_PIOC->PIO_OWDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0xA4]

        ;; AT91C_BASE_PIOC->PIO_OER     = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x10]

        ;; AT91C_BASE_PIOC->PIO_ODR     = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x14]

        ;; AT91C_BASE_PIOC->PIO_IFER    = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x20]

        ;; AT91C_BASE_PIOC->PIO_IFDR    = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x24]

        ;; AT91C_BASE_PIOC->PIO_IER     = 0x00000000;
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x40]

        ;; AT91C_BASE_PIOC->PIO_IDR     = 0xFFFFFFFF;
        LDR     R1, =0xFFFFFFFF
        STR     R1, [R0, #0x44]

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
#ifndef DEBUG

;;setup_sdram
        LDR     R0, =AT91C_BASE_SDRAMC

        ;; AT91C_BASE_SDRAMC->SDRAMC_CR = 0x742262D9;   Merak
        ;; AT91C_BASE_SDRAMC->SDRAMC_CR = 0x74226255;   Thunderbird
        LDR     R1, =0x742262D9
        ;; LDR     R1, =0x74226255
        STR     R1, [R0, #0x08]
        ;; wait 200us
        MOV     R7, #0x000000FF
sdram_wait1
        SUBS    R7, R7, #1
        BNE     sdram_wait1

        ;; setup SDRAMC_MDR register
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x24]
        ;; wait 200us
        MOV     R7, #0x000000FF
sdram_wait2
        SUBS    R7, R7, #1
        BNE     sdram_wait2

        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000002
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x00]
        ;; wait 200us
        MOV     R7, #0x000000FF
sdram_wait3
        SUBS    R7, R7, #1
        BNE     sdram_wait3

        ;; refresh
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000001
        STR     R1, [R0, #0x10]
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000002
        STR     R1, [R0, #0x20]
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000003
        STR     R1, [R0, #0x30]
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x40]
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000005
        STR     R1, [R0, #0x50]
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000006
        STR     R1, [R0, #0x60]
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000007
        STR     R1, [R0, #0x70]
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000004
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000008
        STR     R1, [R0, #0x80]

        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x00000003
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0xCAFEDEDE
        STR     R1, [R0, #0x90]

        ;; AT91C_BASE_SDRAMC->SDRAMC_TR = 0x2B7;    Merak
        ;; AT91C_BASE_SDRAMC->SDRAMC_TR = 0x61A;    Thunderbird
        LDR     R0, =AT91C_BASE_SDRAMC
        LDR     R1, =0x2B7
        ;; LDR     R1, =0x61A
        STR     R1, [R0, #0x04]
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x00]
        LDR     R0, =0x20000000
        LDR     R1, =0x00000000
        STR     R1, [R0, #0x00]

#endif

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;setup_cache_i
        ;; Enable ICache
        MRC     p15, 0, R0, c1, c0, 0; read control register
        ORR     R0, R0, #0x1000      ; instruction cache
        MCR     p15, 0, R0, c1, c0, 0; write control register
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;
; Initialize the stack pointers.
; The pattern below can be used for any of the exception stacks:
; FIQ, IRQ, SVC, ABT, UND, SYS.
; The USR mode uses the same stack as SYS.
; The stack segments must be defined in the linker command file,
; and be declared above.
;


; --------------------
; Mode, correspords to bits 0-5 in CPSR

MODE_MSK DEFINE 0x1F            ; Bit mask for mode bits in CPSR

USR_MODE DEFINE 0x10            ; User mode
FIQ_MODE DEFINE 0x11            ; Fast Interrupt Request mode
IRQ_MODE DEFINE 0x12            ; Interrupt Request mode
SVC_MODE DEFINE 0x13            ; Supervisor mode
ABT_MODE DEFINE 0x17            ; Abort mode
UND_MODE DEFINE 0x1B            ; Undefined Instruction mode
SYS_MODE DEFINE 0x1F            ; System mode


        MRS     r0, cpsr                ; Original PSR value

        ;; Set up the interrupt stack pointer.

        BIC     r0, r0, #MODE_MSK       ; Clear the mode bits
        ORR     r0, r0, #IRQ_MODE       ; Set IRQ mode bits
        MSR     cpsr_c, r0              ; Change the mode
        LDR     sp, =SFE(IRQ_STACK)     ; End of IRQ_STACK
        BIC     sp,sp,#0x7              ; Make sure SP is 8 aligned

        ;; Set up the fast interrupt stack pointer.

        BIC     r0, r0, #MODE_MSK       ; Clear the mode bits
        ORR     r0, r0, #FIQ_MODE       ; Set FIR mode bits
        MSR     cpsr_c, r0              ; Change the mode
        LDR     sp, =SFE(FIQ_STACK)     ; End of FIQ_STACK
        BIC     sp,sp,#0x7              ; Make sure SP is 8 aligned

        ;; Set up the normal stack pointer.

        BIC     r0 ,r0, #MODE_MSK       ; Clear the mode bits
        ORR     r0 ,r0, #SYS_MODE       ; Set System mode bits
        MSR     cpsr_c, r0              ; Change the mode
        LDR     sp, =SFE(CSTACK)        ; End of CSTACK
        BIC     sp,sp,#0x7              ; Make sure SP is 8 aligned

; Continue to ?main for C-level initialization.

        B       ?main

        END
