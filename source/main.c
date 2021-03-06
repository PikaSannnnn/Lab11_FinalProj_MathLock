/*	Author: sdong027
 *  Partner(s) Name: 
 *	Lab Section:
 *	Assignment: Lab #11
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */
/*	
	MC PINS
	PA0-PA7	= LCD_Data
	PB0-PB3	= Shift Register
	PB4-PB5	= LCD_Control
	PB6	= Speaker
	PC0-PC7	= Keypad
	PD0-PD7	= 7-Segment
*/

#include <avr/io.h>
#include <time.h>
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif

#include "essentials.h"
#include "sevenseg.h"
#include "scheduler.h"
#include "lcd.h"

#define GREEN	0x02
#define RED	0x01	

unsigned char gameStartFlag = 0;// flag for if game has been started					
unsigned char difficultySelected = 0;	// whether difficulty has been selected
unsigned char maxOps = 1;	// maximum number of ops, initialized by difficulty			
unsigned char numCompleted = 0;	// resets at a lock unlock	// LED
unsigned char numUnlocks = 0;	// total number of unlocks, need 2 to open safe	// LED
unsigned char timerLED = 0x00;	// flag for if timer LED should be lit/blinked	// LED
unsigned char failed = 0x00;	// whether or not problem is incorrectly solved, 1 => not correct
unsigned char input = '\0';	// global keypad input
unsigned char displayColumn = 1;// column for LCD display
unsigned char endFlag = 0;	// flag for game end
unsigned char alarmOn = 0;
const double frq = 262.00;	// frequency for speakers
long totalTime = 0;		// total "time" value							
int difficulty = 0;		// 0 = none ; 1 = unsecure ; 2 = secure ; 3 = maximum security		
int numAttempts = 0;		// total number of attempts, depends on difficulty
int score = 0;			// score

// SM FUNCTIONS
int SetDifficultySM(int state);
int MathProblemSM(int state);
int SafeSM(int state);	// main sm, handles locked, unlocked, and in betweens (and fail)
int TimerSM(int state);
int AlarmSoundSM(int state);

// COMPUTATIONAL/OPERATIONS FUNCTIONS	(not part of scheduler)
int Input(int state);			// gets keypad input and sets to global input
char* num_to_str(int number);		// converts decimal number to char*
int text_to_num (unsigned char math);	// converts char* to decimal number
void PrintText(char* text);		// PrintsText (utilizes LCD_DisplayString)
unsigned char GetSuccessLED();		// Returns binary of which LEDs to light for "completed" LEDs
unsigned char GetUnlockLED();		// Returns binary of which LEDs to light for "unlocked" LEDs
void SetLights();			// Sets LED for register
void DisplaySeg(char* value);		// Displays number onto 7-Seg
void ComputeScore();			// Computes score

int main(void) {
	DDRA = 0xFF; PORTA = 0x00;
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xF0; PORTC = 0x0F;
	DDRD = 0xFF; PORTD = 0x00;

	int randNum;
	int numPeriod = 0; 
	TimerSet(1);	// MathSM will update/change at 1ms per update (make it seem instant)
	TimerOn();
	LCD_init(); 
	transmit_data(0x00);	// "Clear" register
	LCD_ClearScreen();
	set_PWM(frq);

	static task difficultysm, safe, math, keyin, timersm, alarmsm;
	task *tasks[] = {&difficultysm, &safe, &math, &keyin, &timersm, &alarmsm};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	const char start = -1;

	difficultysm.state = start;
	difficultysm.period = 50;
	difficultysm.elapsedTime = difficultysm.period;
	difficultysm.TickFct = &SetDifficultySM;

	safe.state = start;
	safe.period = 50;
	safe.elapsedTime = safe.period;
	safe.TickFct = &SafeSM;

	math.state = start;
	math.period = 1;
	math.elapsedTime = math.period;
	math.TickFct = &MathProblemSM;

	keyin.state = start;
	keyin.period = 50;
	keyin.elapsedTime = keyin.period;
	keyin.TickFct = &Input;

	timersm.state = start;
	timersm.period = 50;
	timersm.elapsedTime = timersm.period;
	timersm.TickFct = &TimerSM;

	alarmsm.state = start; 
	alarmsm.period = 250;
	alarmsm.elapsedTime = alarmsm.period;
	alarmsm.TickFct = &AlarmSoundSM;

	while (1) {
			// Only run if randNum = 0
		if ((!gameStartFlag) && (!difficultySelected)) {	// game not started and difficulty not selected.
			numPeriod++;
		}
		else if ((!gameStartFlag) && difficultySelected) {	// difficulty has been selected, seed rand
			randNum = (difficulty * numPeriod * 3) % 7;
			srand(randNum);
			//srand(0);					// DEBUG - Getting same problems
			gameStartFlag = 1;	// sets gameStartFlag to true, prevents this and above if from running
		}

		for (unsigned short i = 0; i < numTasks; i++) {
			if (tasks[i]->elapsedTime == tasks[i]->period) {
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 1;	
		}
		SetLights();
		DisplaySeg(num_to_str(numAttempts));

		while (!TimerFlag);
		TimerFlag = 0;
	}
	return 1;
}

enum DifficultyStates {WAIT_DIFF, DOWN, UP, SELECT, PRINT_DIFFICULTY};
int SetDifficultySM(int state) {
	if (endFlag || gameStartFlag) {
		return -1;
	}

	switch (state) {
		case WAIT_DIFF:
			if (input == 'A') {
				state = UP;
			}
			else if (input == 'C') {
				state = DOWN;
			}
			else if (input == 'B') {
				state = SELECT;
			}
			break;
		case DOWN:
			state = PRINT_DIFFICULTY;
			break;
		case UP:
			state = PRINT_DIFFICULTY;
			break;
		case SELECT:
			// WAIT FOR RESTART
			break;
		case PRINT_DIFFICULTY:
			state = WAIT_DIFF;
			break;
		default:
			difficulty = 1;
			state = PRINT_DIFFICULTY;
			break;
	}

	switch (state) {
		case WAIT_DIFF:
			// nothing
			break;
		case DOWN:	// Down movement, not decrease
			if (difficulty < 3) {
				difficulty++;
			}
			break;
		case UP:	// Up movement, not increase
			if (difficulty > 1) {
				difficulty--;
			}
			break;
		case SELECT:
			difficultySelected = 1;
			numAttempts = 5 - ((difficulty - 1) * 2); 	// 5, 3, 1
			totalTime = 180000 - (60000 * (difficulty - 1));
			if (difficulty == 1) { maxOps = 2; }
			else if (difficulty == 2) { maxOps = 3; }
			else if (difficulty == 3) { maxOps = 5; }
			break;
		case PRINT_DIFFICULTY:
			displayColumn = 1;
			LCD_ClearScreen();
			if (difficulty == 1){
				PrintText("==  UNSECURE  ==");
			}
			else if (difficulty == 2) {
				PrintText("==  SECURE  ====");
			}
			else if (difficulty == 3) {
				PrintText("==  MAXIMUM  ===");
				PrintText("==  SECURITY  ==");
			}
			break;
	}
	// outputs difficulties
	// sets selected difficulty
		// set numAttempts
		// set totalTime
	// difficulty chosen by cycling (and printing it out)
		// use A ^, C v, B select
	return state;
}
enum InputStates {WAIT, WAIT_RELEASE};
int Input(int state) {	// get input and store it -> handles single press only.	
	unsigned char keypadIn = GetKeypadKey();

	switch (state) {
		case WAIT:
			if (keypadIn != '\0') {
				state = WAIT_RELEASE;
				input = keypadIn;
			}
			break;
		case WAIT_RELEASE:
			if (keypadIn == '\0') {
				state = WAIT;
			}
			input = '\0';
			break;
		default:
			state = WAIT;
			input = '\0';
			break;
	}
	return state;
}
enum MathStates {MATH_CLEAR, FIRSTNUM, NUMBER, OPERATOR, PRINT, SOLVE, CHECK, UNLOCKING};
int MathProblemSM(int state) {	// prints and checks math inputs
	if (endFlag || (!gameStartFlag && !difficultySelected)) {
		return -1;
	}

	static unsigned char solved;	// whether or not problem is solved, 0 => not yet solved (but not failed)
	static unsigned char unlocking;	// unlocking flag
	static int Solution;		// solution to math
	static unsigned short equationLen;	// length of equation (used for displayColumn)
	static int InputSolution;	// solution to input
	static char* operator = "\0";	// opearator being used
	static short numOps;		// counter of operators in equation
	static int tmpVal;
	static short eqMaxOps;		// number of operations that WILL be in equation

	switch (state) {
		case MATH_CLEAR:
			state = FIRSTNUM;
			break;
		case FIRSTNUM:
			state = OPERATOR;
			break;
		case NUMBER:
			if (numOps < eqMaxOps) {				// FIX maxOps UPDATE FOR DIFFICULTIES
				state = OPERATOR;
			}
			else {
				state = PRINT;
			}
			break;
		case OPERATOR:
			state = NUMBER;
			break;
		case PRINT:
			state = SOLVE;
			break;
		case SOLVE:
			if (input == 'A' || input == 'B' || input == 'C' || input == 'D' || input == '#' || input == '*') {
				state = CHECK;
			}
			break;
		case CHECK:
			if (failed) {
				state = SOLVE;
			}
			else if (solved && !unlocking) {
				state = MATH_CLEAR;
			}
			else if (solved && unlocking) {
				state = UNLOCKING;
			}
			break;
		case UNLOCKING:
			if (!unlocking) {
				state = MATH_CLEAR;
			}
			break;
		default:
			state = MATH_CLEAR;
			solved = 0;
			unlocking = 0;
			failed = 0;
			Solution = 0;
			InputSolution = -1;
			equationLen = 0;
			numCompleted = 0;
			numOps = 0;
			tmpVal = 0;
			break;
	}

	switch (state) {
		case MATH_CLEAR:
			LCD_ClearScreen();
			displayColumn = 1;
			solved = 0;
			unlocking = 0;
			failed = 0;
			Solution = 0;
			InputSolution = -1;	// Set to -1, prevents blank input from being accepted for "0" solutions
			equationLen = 0;
			numOps = 0;
			tmpVal = 0;
			eqMaxOps = (rand() % maxOps) + 1;	// sets num ops that will be in equation
			break;
		case FIRSTNUM:
			tmpVal = (rand()) % (((15 + (5 * numUnlocks)) * difficulty) + 1);	// range between 0-10 inclusive, 0-15 inclusive if 1 is unlocked (for unsecure level), multiply by difficulty
			Solution = tmpVal;

			PrintText(num_to_str(tmpVal));
			break;
		case NUMBER:
			tmpVal = (rand()) % (((15 + (5 * numUnlocks)) * difficulty) + 1);
			if (*operator == '+') {
				Solution = Solution + tmpVal;
			}	 
			else if (*operator == '-') {
				Solution = Solution - tmpVal;
				if (Solution < 0) {			// math problems should only be in the positives (negatives can't be inputted)
					Solution = Solution + tmpVal;	// revert previous math op
					displayColumn--;		// goes back to rewrite over previous '-'
					operator = "+";			// change math op to + instead
					PrintText(operator);
					Solution = Solution + tmpVal;	// performs add op
				}
			}
			else if (*operator == '*') {
				Solution = Solution * tmpVal;
			}
			else if (*operator == '/') {
				if (tmpVal == 0) {			// math problems should only be in the positives (negatives can't be inputted)
					tmpVal = 1;
				}
				Solution = Solution / tmpVal;
			}
			else if (*operator == '%') {
				Solution = Solution % tmpVal;
			}
			
			PrintText(num_to_str(tmpVal));
			break;
		case OPERATOR:
			tmpVal = rand() % (8 + (4 * (8 * (difficulty - 1))));	// EASY Max = 8(0-7), MEDIUM Max = 40(0-39), HARD Max = 72(0-71)
			if (tmpVal < 4) {
				operator = "+";
			}
			else if (tmpVal < 8) {
				operator = "-";
			}
			else if (tmpVal < 24) {
				operator = "*";
			}
			else if (tmpVal < 40) {
				operator = "/";
			}
			else if (tmpVal < 72) {
				operator = "%";
			}

			numOps++;

			PrintText(operator);
			break;
		case PRINT:
			PrintText("=\0");
			//PrintText(num_to_str(Solution));	// DEBUG - Print Solution
			equationLen += displayColumn;	// displayColumn - 1 at this part is equal to the equation len
			break;
		case SOLVE:
			if (input != '\0') {
				if (InputSolution < 0) {	// Set InputSolution to 0 ONLY if an input is detected, prevents no input being correct for "0" solutions
					InputSolution = 0;	// sets InputSolution to 0 so that the numbers can be added and shifted correctly
				}
				PrintText(num_to_str(text_to_num(input)));
				InputSolution *= 10;	// shifts digit left
				InputSolution += text_to_num(input);	// adds new digit
				input = '\0'; 
			}
			failed = 0;	// resets fail flag after transition is complete
			break;
		case CHECK:
			input = '\0';	// Clears input; Transition to this state does not clear input. This is needed.
			if (Solution == InputSolution) {	// correct
				solved = 1;
				numCompleted++;
				if (numCompleted >= 3) {
					tmpVal = 0;
					unlocking = 1;
				}
			}
			else {					// incorrect
				displayColumn = equationLen;
				LCD_Clean(displayColumn);
				InputSolution = -1;
				failed = 1;
				numCompleted = 0;
				numAttempts--;
			}
			break;
		case UNLOCKING:
			if (tmpVal == 0) {
				displayColumn = 1;
				LCD_ClearScreen();
				PrintText("Unlocking");
			}
			else if (tmpVal == 500 || tmpVal == 1000 || tmpVal == 1500) {
				PrintText(".");
			}

			tmpVal++;
			if (tmpVal >= 2000) {	// 2 second unlock time
				unlocking = 0;
				numCompleted = 0;
				numUnlocks++;
			}
			break;
	}

	return state;	// return to do punishment
}
enum SafeStates {GAME_INIT, LOCKED, ONE_UNLOCK, UNLOCKED, WAIT_END, END_GAME, WAIT_END_GAME, RESTART, ALARM};
int SafeSM(int state) {	// main sm, handles locked, unlocked, and in betweens (and fail)
	if (!gameStartFlag && !difficultySelected) {
		return -1;
	}

	static int cnt;
	switch (state) {
		case GAME_INIT:
			state = LOCKED;
			break;
		case LOCKED:
			if (numAttempts <= 0 || endFlag) {	// endFlag in this case can only be set this in TimerSM
				state = ALARM;
			}
			if (numUnlocks == 1) {
				state = ONE_UNLOCK;
			}
			break;
		case ONE_UNLOCK:
			if (numAttempts <= 0 || endFlag) {	// endFlag in this case can only be set this in TimerSM
				state = ALARM;
			}
			if (numUnlocks == 2) {
				state = UNLOCKED;
			}
			break;
		case UNLOCKED:
			state = WAIT_END;
			break;
		case WAIT_END:
			if (input != '\0') {
				state = END_GAME;
			}
			break;
		case END_GAME:
			state = WAIT_END_GAME;
			break;
		case WAIT_END_GAME:
			if (input != '\0') {
				state = RESTART;
			}
			break;
		case RESTART:
			state = GAME_INIT;
			break;
		case ALARM:			// ALARM will NOT include sound
			state = WAIT_END;
			break;
		default:
			state = GAME_INIT;
			cnt = 0;
			break;
	}

	switch (state) {
		case GAME_INIT:
			endFlag = 0;
			break;
		case LOCKED:
			// nothing
			break;
		case ONE_UNLOCK:
			// nothing
			break;
		case UNLOCKED:
			endFlag = 1;
			timerLED = 0x00;
			LCD_ClearScreen();
			displayColumn = 1;
			PrintText("====  SAFE  ====");
			PrintText("==  UNLOCKED  ==");
			break;
		case WAIT_END:
			cnt++;
			break;
		case END_GAME:
			alarmOn = 0;
			LCD_ClearScreen();
			displayColumn = 1;
			ComputeScore();
			PrintText("Score: ");
			PrintText(num_to_str(score));
			break;
		case WAIT_END_GAME:
			// nothing
			break;
		case RESTART:
			numCompleted = 0;
			numUnlocks = 0;	
			numAttempts = 0;
			failed = 0;
			input = '\0';
			timerLED = 0;
			displayColumn = 1;
			endFlag = 0;
			score = 0;
			difficultySelected = 0;	// TEST
			gameStartFlag = 0;	// TEST
			difficulty = 1;		// TEST
			break;
		case ALARM:
			endFlag = 1;
			alarmOn = 1;
			LCD_ClearScreen();
			displayColumn = 1;
			timerLED = 0x01;
			PrintText(" ALARM  SOUNDED ");
			break;
	}
	return state;	
}
enum TimerStates {BS_5, BS_10, BS_15, BS_20, BS_30, BS_40, BS_50, BS_C};
int TimerSM(int state) {
	if (endFlag || (!gameStartFlag && !difficultySelected)) {
		return -1;
	}

	static long blinkTmr;
	static long timePassed;
	blinkTmr += 50;		// Add 50 ms to blinkTmr every time, first Iteration ignored, default sets to 0
	timePassed += 50;	// Add 50 ms to timePassed every time, first Iteration ignored, default sets to 0

	switch (state) {
		case BS_5:
			if (timePassed >= 30000) {
				state = BS_10;
				timePassed = 0;
			}
			break;
		case BS_10:
			if (timePassed >= 30000) {
				state = BS_15;
				timePassed = 0;
			}
			break;
		case BS_15:
			if (timePassed >= 30000) {
				state = BS_20;
				timePassed = 0;
			}
			break;
		case BS_20:
			if (timePassed >= 30000) {
				state = BS_30;
				timePassed = 0;
			}
			break;
		case BS_30:
			if (timePassed >= 30000) {
				state = BS_40;
				timePassed = 0;
			}
			break;
		case BS_40:
			if (timePassed >= 20000) {
				state = BS_50;
				timePassed = 0;
			}
			break;
		case BS_50:
			if (timePassed >= 10000) {
				state = BS_C;
				timePassed = 0;
			}
			break;
		case BS_C:
			// send to default... i think
			break;
		default:
			blinkTmr = 0;
			timePassed = 0;

			if (totalTime == 180000) {
				state = BS_5;
			}
			else if (totalTime == 120000) {
				state = BS_15;
			}
			else if (totalTime == 60000) {
				state = BS_30;
			}
			break;
	}

	switch (state) {
		case BS_5:
			if (timerLED && (blinkTmr >= 100)){
				timerLED = 0x00;
				blinkTmr = 0;	// resets after each blink (resets 50ms after blinkTmr has been set; blinkTmr will be 50ms higher than minimum)
			}
			if (blinkTmr >= 35900) {
				timerLED = 0x01;
				blinkTmr = 0;
			}
			break;
		case BS_10:
			if (timerLED && (blinkTmr >= 100)){
				timerLED = 0x00;
				blinkTmr = 0;	// resets after each blink (resets 50ms after blinkTmr has been set; blinkTmr will be 50ms higher than minimum)
			}
			if (blinkTmr >= 14900) {
				timerLED = 0x01;
				blinkTmr = 0;
			}
			break;
		case BS_15:
			if (timerLED && (blinkTmr >= 100)){
				timerLED = 0x00;
				blinkTmr = 0;	// resets after each blink (resets 50ms after blinkTmr has been set; blinkTmr will be 50ms higher than minimum)
			}
			if (blinkTmr >= 7900) {
				timerLED = 0x01;
				blinkTmr = 0;
			}
			break;
		case BS_20:
			if (timerLED && (blinkTmr >= 100)){
				timerLED = 0x00;
				blinkTmr = 0;	// resets after each blink (resets 50ms after blinkTmr has been set; blinkTmr will be 50ms higher than minimum)
			}
			if (blinkTmr >= 4400) {
				timerLED = 0x01;
				blinkTmr = 0;
			}
			break;
		case BS_30:
			if (timerLED && (blinkTmr >= 100)){
				timerLED = 0x00;
				blinkTmr = 0;	// resets after each blink (resets 50ms after blinkTmr has been set; blinkTmr will be 50ms higher than minimum)
			}
			if (blinkTmr >= 1900) {
				timerLED = 0x01;
				blinkTmr = 0;
			}
			break;
		case BS_40:
			if (timerLED && (blinkTmr >= 100)){
				timerLED = 0x00;
				blinkTmr = 0;	// resets after each blink (resets 50ms after blinkTmr has been set; blinkTmr will be 50ms higher than minimum)
			}
			if (blinkTmr >= 650) {
				timerLED = 0x01;
				blinkTmr = 0;
			}
			break;
		case BS_50:
			if (timerLED && (blinkTmr >= 100)){
				timerLED = 0x00;
				blinkTmr = 0;	// resets after each blink (resets 50ms after blinkTmr has been set; blinkTmr will be 50ms higher than minimum)
			}
			if (blinkTmr >= 100) {
				timerLED = 0x01;
				blinkTmr = 0;
			}
			break;
		case BS_C:
			timerLED = 0x01;
			endFlag = 1;
			break;
	}

	return state;
}
enum AlarmSoundStates {SILENT, PULSE_ON, PULSE_OFF};
int AlarmSoundSM(int state) {
	if (!gameStartFlag && !difficultySelected) {
		return -1;
	}

	switch (state) {
		case SILENT:
			if (alarmOn) {
				state = PULSE_ON;
			}
			break;
		case PULSE_ON:
			if (!alarmOn) {
				state = SILENT;
			}
			else {
				state = PULSE_OFF;
			}
			break;
		case PULSE_OFF:
			if (!alarmOn) {
				state = SILENT;
			}
			else {
				state = PULSE_ON;
			}
			break;
		default:
			state = SILENT;
			break;
	}
	
	switch (state) {
		case SILENT:
			PWM_off();
			break;
		case PULSE_ON:
			PWM_on();
			break;
		case PULSE_OFF:
			PWM_off();
			break;
	}
	return state;
}

char* num_to_str(int number) {
	// Getting each digit place up to 999999
	short ones = number % 10;	
	short tens = (number % 100) / 10;	
	short hunds = (number % 1000) / 100;
	short thous = (number % 10000) / 1000;
	short tthous = (number % 100000) / 10000;
	short hthous = number / 100000;

	if (hthous != 0) {	// number is in 100,000's
		char* numTxt = "000000";
		*(numTxt) = hthous + '0';		// converts and stores ascii vers. of int
		*(numTxt + 1) = tthous + '0'; 		// converts and stores ascii vers. of int
		*(numTxt + 2) = thous + '0';
		*(numTxt + 3) = hunds + '0';
		*(numTxt + 4) = tens + '0';
		*(numTxt + 5) = ones + '0';
		return numTxt;
	}
	else if (tthous != 0) {	// number is in 10,000's
		char* numTxt = "00000";
		*(numTxt) = tthous + '0';		// converts and stores ascii vers. of int
		*(numTxt + 1) = thous + '0'; 		// converts and stores ascii vers. of int
		*(numTxt + 2) = hunds + '0';
		*(numTxt + 3) = tens + '0';
		*(numTxt + 4) = ones + '0';
		return numTxt;

	}
	else if (thous != 0) {	// number is in 1,000's
		char* numTxt = "0000";
		*(numTxt) = thous + '0';		// converts and stores ascii vers. of int
		*(numTxt + 1) = hunds + '0'; 		// converts and stores ascii vers. of int
		*(numTxt + 2) = tens + '0';
		*(numTxt + 3) = ones + '0';
		return numTxt;

	}
	else if (hunds != 0) {	// number is in 100's
		char* numTxt = "000";
		*(numTxt) = hunds + '0';		// converts and stores ascii vers. of int
		*(numTxt + 1) = tens + '0'; 		// converts and stores ascii vers. of int
		*(numTxt + 2) = ones + '0';
		return numTxt;

	}
	else if (tens != 0) {	// number is in 10's
		char* numTxt = "00";
		*(numTxt) = tens + '0';		// converts and stores ascii vers. of int
		*(numTxt + 1) = ones + '0'; 	// converts and stores ascii vers. of int
		return numTxt;
	}
	else if (ones != 0) {	// number is in 1's
		char* numTxt = "0";	
		*(numTxt) = (ones + '0');	// converts and stores ascii vers. of int
		return numTxt;
	}
	else {
		char* numTxt = "0";	
		*(numTxt) = (ones + '0');	// converts and stores ascii vers. of int
		return numTxt;
	}

	return "";
}

int text_to_num (unsigned char math) {
	switch (math) {
		case '1':
			return 1;
		case '2':
			return 2;
		case '3':
			return 3;
		case '4':
			return 4;
		case '5':
			return 5;
		case '6':
			return 6;
		case '7':
			return 7;
		case '8':
			return 8;
		case '9':
			return 9;
		case '0':
			return 0;
	}

	return 0;
}
void PrintText(char* text) {
	// Calculate text length
	unsigned short textLen = 0;
	char* charPtr = text;
	while (*(charPtr) != '\0') {
		textLen++;	
		charPtr++;
	}

	LCD_DisplayString(displayColumn, (const unsigned char *)(text));
	displayColumn += textLen;
}
unsigned char GetSuccessLED() {
	if (numCompleted == 0) {
		return 0x00;
	}
	else if (numCompleted == 1) {
		return 0x01;
	}
	else if (numCompleted == 2) {
		return 0x03;
	}
	else if (numCompleted >= 3) {
		return 0x07;
	}

	return 0x00;
}
unsigned char GetUnlockLED() {
	unsigned char returnLEDs = (RED << 2) | RED;	// all red

	if (numUnlocks == 1) {
		returnLEDs = (RED << 2) | GREEN;
	}
	if (numUnlocks == 2) {
		returnLEDs = (GREEN << 2) | GREEN;
	}

	return returnLEDs;
}
void SetLights() {
	unsigned char ledOutput = timerLED;
	ledOutput = ledOutput << 3;
	ledOutput = ledOutput | GetSuccessLED();
	ledOutput = ledOutput << 4;
	ledOutput = ledOutput | GetUnlockLED();

	transmit_data(ledOutput);
}
void DisplaySeg(char* value) {
	unsigned char sevensegVal = (unsigned char)(*(value));	// number is guaranteed to be single digit (in first address of c-string)
	unsigned char ssOutput = 0x00 | Write7Seg(text_to_num(sevensegVal));
	PORTD = ssOutput;
}
void ComputeScore() {
	int totalAttempts = 5 - ((difficulty - 1) * 2); 	// 5, 3, 1
	score = (100 * (difficulty * difficulty)) * (((double)numAttempts / totalAttempts) + numUnlocks);
}

