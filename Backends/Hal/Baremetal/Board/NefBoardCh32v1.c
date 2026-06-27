/*
 * Backends/Hal/Baremetal/Board/NefBoardCh32v1.c — CH32V103 board support
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 NanoEFI Contributors
 *
 * Clocks: HSI 8 MHz → PLL × 9 = 72 MHz (same as STM32F103 default)
 * SysTick: 72 MHz / 8 = 9 MHz, reload = 9000 → 1 ms tick
 * UART1: PA9/PA10, 115200 8N1
 * GPIO: pin number = port*16 + pin (PA0=0, PB0=16, PC0=32 ...)
 *
 * Requires: ch32v103.h (WCH SDK or your own CMSIS-style header)
 */

#include "../NefBoard.h"
#include "ch32v103.h"  /* WCH peripheral register definitions */

/*──────────────────────────────────────────────────────────────────────
 * SysTick ms counter
 *──────────────────────────────────────────────────────────────────────*/
static volatile UINT64 STickMs = 0;

/* SysTick IRQ handler — 1 ms per tick */
void SysTick_Handler(void) {
    STickMs++;
    SysTick->SR = 0;  /* clear compare flag */
}

/*──────────────────────────────────────────────────────────────────────
 * Board init
 *──────────────────────────────────────────────────────────────────────*/
static void ClockInit(void) {
    /* Enable HSE or stay on HSI — CH32V103 default is HSI 8 MHz.
     * PLL: HSI/2 × 18 = 72 MHz */
    RCC->CFGR0 &= ~RCC_CFGR0_PLLSRC;           /* PLL src = HSI/2 */
    RCC->CFGR0 = (RCC->CFGR0 & ~RCC_CFGR0_PLLMULL) |
                  RCC_CFGR0_PLLMULL18;           /* ×18 */
    RCC->CTLR  |= RCC_CTLR_PLLON;
    while (!(RCC->CTLR & RCC_CTLR_PLLRDY));
    RCC->CFGR0 = (RCC->CFGR0 & ~RCC_CFGR0_SW) |
                  RCC_CFGR0_SW_PLL;              /* switch to PLL */
    while ((RCC->CFGR0 & RCC_CFGR0_SWS) != RCC_CFGR0_SWS_PLL);
}

static void SysTickInit(void) {
    /* 72 MHz / 8 = 9 MHz counter clock, reload 9000 → 1 ms */
    SysTick->CTLR = 0;
    SysTick->SR   = 0;
    SysTick->CNT  = 0;
    SysTick->CMP  = 9000 - 1;
    SysTick->CTLR = (1 << 0) |  /* enable */
                    (1 << 1) |  /* TICKINT: generate IRQ */
                    (0 << 2);   /* HCLK/8 */
    NVIC_EnableIRQ(SysTicK_IRQn);
}

static void Uart1Init(void) {
    RCC->APB2PCENR |= RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA;
    /* PA9 = TX (AF push-pull), PA10 = RX (input floating) */
    GPIOA->CFGHR &= ~(0xFFu << 4);
    GPIOA->CFGHR |=  (0x0Bu << 4);   /* PA9:  AF push-pull 50 MHz */
    GPIOA->CFGHR |=  (0x04u << 8);   /* PA10: input floating */
    USART1->BRR   = 72000000 / 115200;
    USART1->CTLR1 = USART_CTLR1_TE | USART_CTLR1_UE;
}

void NefBoardInit(void) {
    ClockInit();
    SysTickInit();
    Uart1Init();
}

/*──────────────────────────────────────────────────────────────────────
 * Time
 *──────────────────────────────────────────────────────────────────────*/
UINT64 NefBoardGetTickMs(void) { return STickMs; }

void NefBoardDelayMs(UINT32 Ms) {
    UINT64 End = STickMs + Ms;
    while (STickMs < End);
}

/*──────────────────────────────────────────────────────────────────────
 * UART log
 *──────────────────────────────────────────────────────────────────────*/
void NefBoardUartPutchar(char C) {
    while (!(USART1->STATR & USART_STATR_TXE));
    USART1->DATAR = (UINT8)C;
}

/*──────────────────────────────────────────────────────────────────────
 * GPIO
 * Pin encoding: port*16 + pin  (PA0=0 .. PA15=15, PB0=16 .. PC15=47)
 *──────────────────────────────────────────────────────────────────────*/
static GPIO_TypeDef *PortFromPin(UINT8 Pin, UINT8 *Bit) {
    *Bit = Pin & 0x0Fu;
    switch (Pin >> 4) {
        case 0: RCC->APB2PCENR |= RCC_APB2Periph_GPIOA; return GPIOA;
        case 1: RCC->APB2PCENR |= RCC_APB2Periph_GPIOB; return GPIOB;
        case 2: RCC->APB2PCENR |= RCC_APB2Periph_GPIOC; return GPIOC;
        case 3: RCC->APB2PCENR |= RCC_APB2Periph_GPIOD; return GPIOD;
        default: return NULL;
    }
}

EfiStatus NefBoardPinMode(UINT8 Pin, UINT8 Mode) {
    UINT8 Bit;
    GPIO_TypeDef *Port = PortFromPin(Pin, &Bit);
    if (!Port) return NEF_EINVAL;
    /* Use CRL/CRH: 4 bits per pin */
    volatile UINT32 *Cr = (Bit < 8) ? &Port->CFGLR : &Port->CFGHR;
    UINT32 Shift = (Bit & 7u) * 4u;
    UINT32 Cfg;
    switch (Mode) {
        case NANO_EFI_GPIO_OUTPUT: Cfg = 0x3u; break; /* push-pull 50 MHz */
        case NANO_EFI_GPIO_INPUT:  Cfg = 0x4u; break; /* input floating   */
        default:                   return NEF_EINVAL;
    }
    *Cr = (*Cr & ~(0xFu << Shift)) | (Cfg << Shift);
    return NEF_OK;
}

EfiStatus NefBoardPinSet(UINT8 Pin, UINT8 Val) {
    UINT8 Bit;
    GPIO_TypeDef *Port = PortFromPin(Pin, &Bit);
    if (!Port) return NEF_EINVAL;
    if (Val) Port->BSHR = (1u << Bit);
    else     Port->BCR  = (1u << Bit);
    return NEF_OK;
}

EfiStatus NefBoardPinGet(UINT8 Pin, UINT8 *Val) {
    if (!Val) return NEF_EINVAL;
    UINT8 Bit;
    GPIO_TypeDef *Port = PortFromPin(Pin, &Bit);
    if (!Port) return NEF_EINVAL;
    *Val = (Port->INDR >> Bit) & 1u;
    return NEF_OK;
}

/*──────────────────────────────────────────────────────────────────────
 * Capability
 *──────────────────────────────────────────────────────────────────────*/
UINT32 NefBoardQueryCap(UINT32 Id) {
    switch (Id) {
        case NANO_EFI_CAP_MALLOC: return 0u;  /* no heap */
        case NANO_EFI_CAP_FPU:    return 0u;  /* CH32V103: no FPU */
        case NANO_EFI_CAP_DSP:    return 0u;
        case NANO_EFI_CAP_CACHE:  return 0u;
        default:                  return 0u;
    }
}

/*──────────────────────────────────────────────────────────────────────
 * Critical section — RISC-V: save/restore mstatus.MIE
 *──────────────────────────────────────────────────────────────────────*/
UINT32 NefBoardEnterCritical(void) {
    UINT32 Mstatus;
    __asm volatile ("csrrci %0, mstatus, 8" : "=r"(Mstatus));
    return Mstatus;
}

void NefBoardExitCritical(UINT32 SavedState) {
    if (SavedState & 0x8u)  /* MIE was set */
        __asm volatile ("csrsi mstatus, 8");
}
