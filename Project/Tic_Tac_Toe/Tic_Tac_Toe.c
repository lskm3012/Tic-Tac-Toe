// Tic Tac Toe
// By Karthikeyan Lakshmana Doss

#include "ti/devices/msp432p4xx/inc/msp.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

// Global variable declarations
volatile bool randomMode = false;
volatile bool calculatedMode = false;
volatile bool isUserMove = false;
volatile bool isLaunchPadMove = false;
volatile int board[9] = {0};        // 0 for empty squares, 1 for user moves and -1 for LaunchPad moves
volatile int square = 0;            // Keeps track of which square is being played
volatile char check = 'I';          // Game result stored here (Inconclusive at first)
volatile int numMoves = 0;          // Keeps track of number of moves played
volatile int seconds = 0;           // Keeps track of seconds passed for the timer
volatile int squareTemp = -1;       // Keeps track of temporary square chosen by the user

// Board layout
// 0 1 2
// 3 4 5
// 6 7 8

// Function prototypes
int randomSquare();
int calculatedSquare();
int minimax();
void launchPadMove();
void userMove();
char checkResult();
void clearTempLEDs();
void displayResult();

int main(void)
{
    WDT_A->CTL = WDT_A_CTL_PW |
            WDT_A_CTL_HOLD;                         // Stop WDT

    TIMER_A0->CCR[0] = 32768-1;                     // Interrupt generated every second
    TIMER_A0->CTL = TIMER_A_CTL_SSEL__ACLK |        // ACLK (32768 Hz), UP mode
            TIMER_A_CTL_MC__UP;

    // Set other ports as outputs with no output
    P2->DIR |= 0xFF; P2->OUT = 0;
    P3->DIR |= 0xFF; P3->OUT = 0;
    P4->DIR |= 0xFF; P4->OUT = 0;
    P5->DIR |= 0xFF; P5->OUT = 0;
    P6->DIR |= 0xFF; P6->OUT = 0;
    P7->DIR |= 0xFF; P7->OUT = 0;
    P8->DIR |= 0xFF; P8->OUT = 0;
    P9->DIR |= 0xFF; P9->OUT = 0;
    P10->DIR |= 0xFF; P10->OUT = 0;
    PJ->DIR |= 0xFF; PJ->OUT = 0;

    P1->DIR = ~(BIT1 | BIT4 | BIT6);
    P1->OUT = BIT1 | BIT4 | BIT6;
    P1->REN = BIT1 | BIT4 | BIT6;                   // Enable pull-up resistor (P1.1 output high)
    P1->SEL0 = 0;
    P1->SEL1 = 0;
    P1->IFG = 0;                                    // Clear all P1 interrupt flags
    P1->IE = BIT1 | BIT4;                           // Enable interrupt for P1.1 and P1.4 only
    P1->IES = BIT1 | BIT4 | BIT6;                   // Interrupt on high-to-low transition

    P5->SEL1 |= BIT4;                               // Configure P5.4 for ADC
    P5->SEL0 |= BIT4;

    // Enable timer and button interrupts
    NVIC->ISER[0] = 1 << ((TA0_0_IRQn) & 31);
    NVIC->ISER[0] |= 1 << ((ADC14_IRQn) & 31);
    NVIC->ISER[1] = 1 << ((PORT1_IRQn) & 31);

    // Sampling time, S&H=16, ADC14 on
    ADC14->CTL0 = ADC14_CTL0_SHT0_2 | ADC14_CTL0_SHP | ADC14_CTL0_ON;
    ADC14->CTL1 = ADC14_CTL1_RES_2;                 // Use sampling timer, 12-bit conversion results

    ADC14->MCTL[0] |= ADC14_MCTLN_INCH_1;           // A1 ADC input select; Vref=AVCC
    ADC14->IER0 |= ADC14_IER0_IE0;                  // Enable ADC conv complete interrupt

    // Enable global interrupt
    __enable_irq();

    srand(time(0));

    while (1);
}

// Port1 ISR
void PORT1_IRQHandler(void)
{
    volatile uint32_t i;
    if (P1->IFG & BIT1)
    {
        randomMode = true;
        isUserMove = true;
        P1->IE = BIT6;                              // Disable all P1 interrupts except for 1.6
        P2->OUT |= BIT1;                            // User's turn now
        TIMER_A0->CCTL[0] = TIMER_A_CCTLN_CCIE;     // TACCR0 interrupt enabled (after mode is chosen)
        ADC14->CTL0 |= ADC14_CTL0_ENC | ADC14_CTL0_SC; // Start sampling conversion
    }
    else if (P1->IFG & BIT4)
    {
        calculatedMode = true;
        isUserMove = true;
        P1->IE = BIT6;                              // Disable all P1 interrupts except for 1.6
        P2->OUT |= BIT1;                            // User's turn now
        TIMER_A0->CCTL[0] = TIMER_A_CCTLN_CCIE;     // TACCR0 interrupt enabled (after mode is chosen)
        ADC14->CTL0 |= ADC14_CTL0_ENC | ADC14_CTL0_SC; // Start sampling conversion
    }
    else if ((P1->IFG & BIT6) && isUserMove)
    {
        if (!board[squareTemp])
        {
            seconds = 0;
            square = squareTemp;                    // Update square
            squareTemp = -1;                        // Default value for squareTemp
            clearTempLEDs();
            userMove();
        }
        else
            ADC14->CTL0 |= ADC14_CTL0_ENC | ADC14_CTL0_SC; // Start sampling conversion
    }

    P1->IFG = 0;                                    // Clear all interrupts in port 1
    for(i = 0; i < 10000; i++);                     // For button debouncing
}

// Timer A0 ISR
void TA0_0_IRQHandler(void)
{
    static bool isOver = false;                     // Checks when the game is over
    seconds++;                                      // Keeping track of seconds passed

    if (isUserMove && (seconds >= 10))
    {
        seconds = 0;
        squareTemp = -1;
        clearTempLEDs();
        square = randomSquare();
        userMove();
    }
    else if (isLaunchPadMove && (seconds >= 5))
    {
        seconds = 0;
        squareTemp = -1;
        clearTempLEDs();
        launchPadMove();
        // Start sampling/conversion
        ADC14->CTL0 |= ADC14_CTL0_ENC | ADC14_CTL0_SC;
    }

    if (isOver)
        displayResult();                            // Executed one second after game ends

    if (checkResult() != 'I')
    {
        isUserMove = false;
        isLaunchPadMove = false;
        isOver = true;
        P2->OUT &= ~(BIT0 | BIT1);                  // Turn off LaunchPad LED
    }

    TIMER_A0->CCTL[0] &= ~TIMER_A_CCTLN_CCIFG;
}

// ADC14 interrupt service routine
void ADC14_IRQHandler(void)
{
    static int adcVal = -1;         // Used to check if ADC output changed
    adcVal = ADC14->MEM[0];

    switch (adcVal)
    {
    case 0 ... 350:
        squareTemp = 0;
        if (!board[squareTemp])
            P2->OUT |= BIT6;
        break;
    case 450 ... 800://(int) ((2/9.0)*4096):
        squareTemp = 1;
        if (!board[squareTemp])
            P2->OUT |= BIT4;
        break;
    case 900 ... 1250:
        squareTemp = 2;
        if (!board[squareTemp])
            P5->OUT |= BIT6;
        break;
    case 1350 ... 1700:
        squareTemp = 3;
        if (!board[squareTemp])
            P6->OUT |= BIT6;
        break;
    case 1800 ... 2150:
        squareTemp = 4;
        if (!board[squareTemp])
            P6->OUT |= BIT7;
        break;
    case 2250 ... 2600:
        squareTemp = 5;
        if (!board[squareTemp])
            P2->OUT |= BIT3;
        break;
    case 2700 ... 3050:
        squareTemp = 6;
        if (!board[squareTemp])
            P5->OUT |= BIT1;
        break;
    case 3150 ... 3500:
        squareTemp = 7;
        if (!board[squareTemp])
            P3->OUT |= BIT5;
        break;
    case 3600 ... 4095:
        squareTemp = 8;
        if (!board[squareTemp])
            P3->OUT |= BIT7;
        break;
    }

    clearTempLEDs();

    // Start sampling/conversion again
    if (isUserMove)
        ADC14->CTL0 |= ADC14_CTL0_ENC | ADC14_CTL0_SC;
    else
    {
        squareTemp = -1;
        clearTempLEDs();
    }
}


int randomSquare()
{
    int emptySquares[9] = {0};
    int numEmpty = 0;
    int i = 0;
    for (; i < 9; i++)
    {
        if (!board[i])
            emptySquares[numEmpty++] = i;           // Keep track of possible squares
    }
    int randomNum = rand() % numEmpty;
    return emptySquares[randomNum];                 // Random number between 0 and 8
}

int calculatedSquare()
{
    int bestScore = -100;
    int bestSquare, score, i;
    for (i = 0; i < 9; i++)
    {
        if (!board[i])
        {
            board[i] = -1;                          // Update board temporarily
            if (checkResult() == 'L')
                return i;                           // Instantly return for a winning square
            score = minimax(false);                 // Predicting possibilities
            board[i] = 0;                           // Clear updated square
            if (score > bestScore)
            {
                bestScore = score;
                bestSquare = i;
            }
        }
    }
    return bestSquare;
}

int minimax(isMaximizing)                           // Algorithm for LaunchPad playing Tic Tac Toe
{
    int bestScore, score;
    uint32_t i;
    check = checkResult();
    // Base case
    switch (check)
    {
    case 'L':
        return 5;
    case 'U':
        return -5;
    case 'D':
        return 0;
    }

    // Recursive case
    if (isMaximizing)                               // LaunchPad always tries to maximize optimally
    {
        bestScore = -100;
        for (i = 0; i < 9; i++)
        {
            if (!board[i])
            {
                board[i] = -1;                      // Assume LaunchPad plays optimally
                score = minimax(false);             // Assume user also plays optimally in return
                board[i] = 0;
                if (score > bestScore)
                    bestScore = score;
            }
        }
        return bestScore;
    }
    else                                            // User always tries to minimize optimally
    {
        bestScore = 100;
        for (i = 0; i < 9; i++)
        {
            if (!board[i])
            {
                board[i] = 1;                       // Assume user plays optimally
                score = minimax(true);              // Assume LaunchPad also plays optimally in return
                board[i] = 0;
                if (score < bestScore)
                    bestScore = score;
            }
        }
        return bestScore;
    }
}

void launchPadMove()
{
    if (randomMode)
        square = randomSquare();
    else if (calculatedMode && (numMoves <= 1))
    { // Hardcoding the first case to save time with minimax
        if (!board[4])
            square = 4;
        else
            square = 0;
    }
    else if (calculatedMode && (numMoves > 1))
        square = calculatedSquare();

    switch(square)
    {
    case 0:
        P6->OUT |= BIT0;
        break;
    case 1:
        P3->OUT |= BIT2;
        break;
    case 2:
        P3->OUT |= BIT3;
        break;
    case 3:
        P4->OUT |= BIT1;
        break;
    case 4:
        P4->OUT |= BIT3;
        break;
    case 5:
        P1->OUT |= BIT5;
        break;
    case 6:
        P4->OUT |= BIT6;
        break;
    case 7:
        P6->OUT |= BIT5;
        break;
    case 8:
        P6->OUT |= BIT4;
        break;
    }

    board[square] = -1;                             // Update board
    numMoves++;                                     // Update number of moves
    isLaunchPadMove = false;
    isUserMove = true;

    P2->OUT &= ~BIT0;
    P2->OUT |= BIT1;                                // User's turn
}

void userMove()
{
    switch(square)
    {
    case 0:
        P2->OUT |= BIT6;
        break;
    case 1:
        P2->OUT |= BIT4;
        break;
    case 2:
        P5->OUT |= BIT6;
        break;
    case 3:
        P6->OUT |= BIT6;
        break;
    case 4:
        P6->OUT |= BIT7;
        break;
    case 5:
        P2->OUT |= BIT3;
        break;
    case 6:
        P5->OUT |= BIT1;
        break;
    case 7:
        P3->OUT |= BIT5;
        break;
    case 8:
        P3->OUT |= BIT7;
        break;
    }

    board[square] = 1;                          // Update board
    numMoves++;                                 // Update number of moves
    isUserMove = false;
    isLaunchPadMove = true;

    P2->OUT &= ~BIT1;
    P2->OUT |= BIT0;                            // LaunchPad's turn
}

char checkResult()
{
    int i;

    // Check rows
    for (i = 0; i < 9; i+=3)
    {
        if ((board[i] == board[i+1]) && (board[i]  == board[i+2]))
        {
            if (board[i] == 1)
                return 'U';                     // "User" wins
            else if (board[i] == -1)
                return 'L';                     // LaunchPad wins
        }
    }

    // Check columns
    for (i = 0; i < 3; i++)
    {
        if ((board[i] == board[i+3]) && (board[i]  == board[i+6]))
        {
            if (board[i] == 1)
                return 'U';                     // "User" wins
            else if (board[i] == -1)
                return 'L';                     // LaunchPad wins
        }
    }

    // Check diagonals
    if (( (board[0] == board[4]) && (board[4] == board[8]) ) ||
            ( (board[2] == board[4]) && (board[4] == board[6]) ))
    {
        if (board[4] == 1)
            return 'U';                         // "User" wins
        else if (board[4] == -1)
            return 'L';                         // LaunchPad wins
    }

    // Check if tie
    for (i = 0; i < 9; i++)
    {
        if (!board[i])
            return 'I';                         // Inconclusive - game in progress
    }

    // If none of the other cases are true
    return 'D';                                 // "Draw"
}

void clearTempLEDs()
{
    uint32_t i;                                 // Loop variable
    for (i = 0; i < 9; i++)
    {
        if ((i == squareTemp) || board[i])
            continue;
        switch (i)
        {
        case 0:
            P2->OUT &= ~BIT6;
            break;
        case 1:
            P2->OUT &= ~BIT4;
            break;
        case 2:
            P5->OUT &= ~BIT6;
            break;
        case 3:
            P6->OUT &= ~BIT6;
            break;
        case 4:
            P6->OUT &= ~BIT7;
            break;
        case 5:
            P2->OUT &= ~BIT3;
            break;
        case 6:
            P5->OUT &= ~BIT1;
            break;
        case 7:
            P3->OUT &= ~BIT5;
            break;
        case 8:
            P3->OUT &= ~BIT7;
            break;
        }
    }
}

void displayResult()
{
    switch (checkResult())
    {
    case 'I':
        return;                                 // Do nothing if game is in progress
    case 'L':
        P5->OUT = 0;                            // Resetting LEDs not overwritten
        P1->OUT = BIT5;                         // Overwriting all LEDs red
        P3->OUT = (BIT2 | BIT3);
        P4->OUT = (BIT1 | BIT3 | BIT6);
        P6->OUT = (BIT0 | BIT4 | BIT5);
        break;
    case 'U':
        P4->OUT = 0;                            // Resetting LEDs not overwritten
        P1->OUT = 0;
        P2->OUT = (BIT3 | BIT4 | BIT6);         // Overwriting all LEDs green
        P3->OUT = (BIT5 | BIT7);
        P5->OUT = (BIT1 | BIT6);
        P6->OUT = (BIT6 | BIT7);
        break;
    case 'D':
        P3->OUT = 0;                            // Resetting LEDs not overwritten
        P1->OUT = BIT5;                         // Overwriting 4 LEDs red and 4 LEDs green
        P2->OUT = (BIT4 | BIT6);
        P4->OUT = BIT6;
        P5->OUT = (BIT6);
        P6->OUT = (BIT4 | BIT5 | BIT6);
        break;
    }

    // Disable global interrupt
    __disable_irq();

}
