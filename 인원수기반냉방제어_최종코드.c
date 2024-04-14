#include <wiringPiI2C.h>
#include <wiringPiSPI.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <lcd.h> 
#include <mcp3004.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include <string.h>

#define MAXTIMINGS	85
#define DHTPIN		7	
int dhtVal[5] = { 0, 0, 0, 0, 0 };

#define uchar unsigned char
#define LedPinRed    0
#define LedPinGreen  1

#define REL 2

#define I2C_ADDR 0x27
#define LCD_CHR 1 
#define LCD_CMD 0
#define LINE1 0x80
#define LINE2 0xc0
#define LCD_BACKLIGHT 0x08
#define ENABLE 0b00000100

#define BASE 100 
#define SPI_CHAN1 0 // 조도 센서 1
#define SPI_CHAN2 1 // 조도 센서 2
#define SPI_CHAN3 2 // 조도 센서 3
#define SPI_CHAN4 3 // 조도 센서 4
#define SPI_CHAN5 4 // 조도 센서 5
int people = 0; //사람 수

void printLCD(int* temperature, int people); // LCD에 온도와 사람 수를 출력하는 함수
void ClrLcd(); // LCD를 초기화하는 함수
void lcdLoc(int line); // LCD에 출력할 line을 지정하는 함수 

// I2C 통신 관련 함수
void lcd_byte(int bits, int mode);
void lcd_toggle_enable(int bits);
void lcd_init();
int fd;

 // 온도 출력 관련 함수
void typeInt(int i); 
void typeln(const char* s);
void typeln(const char* s);

void AirconMaintain(); // 열전소자 쿨러 모듈을 켜고, 끄기를 반복하는 함수
void AirconTurnoff(); // 열전소자 쿨러 모듈을 키는 함수
void AirconTurnon(); // 열전소자 쿨러 모듈을 끄는 함수

void TurnLED(int people); //LED를 키는 함수
void ledInit(void); // LED 설정을 초기화하는 함수
void ledColorSet(uchar r_val, uchar g_val, uchar b_val); // LED 색을 설정하는 함수

void MeasureTemperature(); // 온도를 측정하는 함수

void People(); // 사람 수를 측정하는 함수
void detectPeople(int val); // 사람을 인식하는 함수

int main(void)
{
	void (*fp)() = AirconTurnoff; // 처음에는 에어컨을 끄고 시작한다.
	if (wiringPiSetup() == -1) exit(1); // wiringPi 설정
	if (wiringPiSPISetup(0, 500000) == -1) exit(1);  //wiringPiSPI 설정
	mcp3004Setup(BASE, 0); // mcp3008 모듈 설정

	while (1) {
		int past_people = people; // 이전 승객 수 저장

		People(); // 현재 승객 수 측정
		int temperature[2] = { dhtVal[2], dhtVal[3] }; // 온도의 정수값, 소숫값 배열에 저장
		printLCD(temperature, people); // 온도와 승객 수 LCD에 출력
		TurnLED(people); // 사람 수에 맞는 LED 불 ON

		if (past_people != people) { // 승객 수가 달라졌을 때
				
			if (people > 0 && people < 5) { //0명 이상 4명 이하 인 경우 - AirconMaintain()
				fp = AirconMaintain;
			}
			else if (people == 0) { //0명인 경우 - AirconTurnoff()
				fp = AirconTurnoff;
			}
			else if (people >= 5) {	//5명인 경우 - AirconTurnon ()
				fp = AirconTurnon;
			}
		}

		//설정한 에어컨 모드 켜기
		fp();

		MeasureTemperature(); // 온도 측정
		
		delay(8500); //8.5초 delay
	}
	return(0);
}

// LCD에 온도와 사람 수를 출력하는 함수
void printLCD(int* temperature, int people) {
	fd = wiringPiI2CSetup(I2C_ADDR); //I2C 통신 시작

	lcd_init(); // LCD 설정 초기화
	ClrLcd(); // LCD 초기화

	lcdLoc(LINE1); // LCD 첫번째 줄
	typeln("now : "); //"now: " 출력
	typeInt(temperature[0]); // 온도 정수값 출력
	typeln("."); // "." 출력
	typeInt(temperature[1]); // 온도 소숫값 출력
	typeln("C"); // "C" 출력

	lcdLoc(LINE2); // LCD 두번째 줄
	typeln("people : "); // "peope: " 출력
	typeInt(people); // 사람 수 출력
	typeln("/5"); // "/5" 출력
}

// LCD 정숫값 출력
void typeInt(int i) {
	char array1[20];
	sprintf(array1, "%d", i);
	typeln(array1);
}

// LCD 초기화
void ClrLcd() {
	lcd_byte(0x01, LCD_CMD);
	lcd_byte(0x02, LCD_CMD);
}

// LCD 설정
void lcdLoc(int line) {
	lcd_byte(line, LCD_CMD);
}

// 온도 정숫값 설정
void typeln(const char* s) {
	while (*s) lcd_byte(*(s++), LCD_CHR);
}

//LCD 출력 설정
void lcd_byte(int bits, int mode) {
	int bits_high;
	int bits_low;

	bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
	bits_low = mode | (bits << 4) & 0xF0 | LCD_BACKLIGHT;

	wiringPiI2CReadReg8(fd, bits_high);
	lcd_toggle_enable(bits_high);

	wiringPiI2CReadReg8(fd, bits_low);
	lcd_toggle_enable(bits_low);
}

// LCD 출력 설정
void lcd_toggle_enable(int bits) {
	delayMicroseconds(500);
	wiringPiI2CReadReg8(fd, (bits | ENABLE));
	delayMicroseconds(500);
	wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
	delayMicroseconds(500);
}

// LCD 출력 설정
void lcd_init()
{
	lcd_byte(0x33, LCD_CMD);
	lcd_byte(0x32, LCD_CMD);
	lcd_byte(0x06, LCD_CMD);
	lcd_byte(0x0C, LCD_CMD);
	lcd_byte(0x28, LCD_CMD);
	lcd_byte(0x01, LCD_CMD);
	delayMicroseconds(500);
}


void AirconMaintain() { // 열전소자 쿨러 모듈을 켜고, 끄기를 반복하는 함수
	pinMode(REL, OUTPUT); 

	digitalWrite(REL, LOW); // 4초 동안 끄기
	delay(4000);

	digitalWrite(REL, HIGH); // 3초 동안 켜기
	delay(3000);
}

void AirconTurnoff() { // 열전소자 쿨러 모듈을 끄는 함수
	pinMode(REL, OUTPUT);
	digitalWrite(REL, LOW); // 끄기

	return;
}

void AirconTurnon() { // 열전소자 쿨러 모듈을 키는 함수
	pinMode(REL, OUTPUT);

	digitalWrite(REL, HIGH); // 켜기

	return;
}

void TurnLED(int people) { // LED를 키는 함수
	ledInit(); // LED 설정 초기화
	if (people >= 5) { // 승객이 5명 이상이라면
		ledInit();
		ledColorSet(0xff, 0x00, 0x00);   // Red 켜기
	}
	else { // 4명 이하라면
		ledInit();
		ledColorSet(0x00, 0xff, 0x00);   // Green 켜기
	}
}

// LED 초기화 함수
void ledInit(void)
{
	softPwmCreate(LedPinRed, 0, 100);
	softPwmCreate(LedPinGreen, 0, 100);
}

// LED 색 설정 함수
void ledColorSet(uchar r_val, uchar g_val, uchar b_val)
{
	softPwmWrite(LedPinRed, r_val);
	softPwmWrite(LedPinGreen, g_val);
}


void MeasureTemperature() // 온도 측정 함수
{
	uint8_t laststate = HIGH;
	uint8_t counter = 0;
	uint8_t j = 0, i;

	dhtVal[0] = dhtVal[1] = dhtVal[2] = dhtVal[3] = dhtVal[4] = 0;

	// MCU 18ms 동안 LOW signal 보냄
	pinMode(DHTPIN, OUTPUT); 	
	digitalWrite(DHTPIN, LOW); //pull down
	delay(18); // dealy

	//40micros 동안 HIGH signal 보냄
	digitalWrite(DHTPIN, HIGH); //pull up
	delayMicroseconds(40); // deay

	//DHT가 signal 받음
	pinMode(DHTPIN, INPUT);

	// DHT가 MCU에 data 보내는 과정
	for (i = 0; i < MAXTIMINGS; i++){
		counter = 0;
		while (digitalRead(DHTPIN) == laststate){
			counter++;
			delayMicroseconds(1);
			if (counter == 255)
				break;
		}

		laststate = digitalRead(DHTPIN);
		if (counter == 255)
			break;

		/* ignore first 3 transitions */
		if ((i >= 4) && (i % 2 == 0))
		{
			/* shove each bit into the storage bytes */
			dhtVal[j / 8] <<= 1;
			if (counter > 16)
				dhtVal[j / 8] |= 1;
			j++;
		}
	}

	return;
}

void People() { // 사람 측정 함수
	people = 0;  //사람 수 정보 초기화 

	int val1 = analogRead(BASE + SPI_CHAN1); // 1번째 조도센서 값 읽기
	int val2 = analogRead(BASE + SPI_CHAN2); // 2번째 조도센서 값 읽기
	int val3 = analogRead(BASE + SPI_CHAN3); // 3번째 조도센서 값 읽기
	int val4 = analogRead(BASE + SPI_CHAN4); // 4번째 조도센서 값 읽기
	int val5 = analogRead(BASE + SPI_CHAN5); // 5번째 조도센서 값 읽기

	// 각 조도 센서의 사람 유무 판단
	detectPeople(val1); 
	detectPeople(val2);
	detectPeople(val3);
	detectPeople(val4);
	detectPeople(val5);
}

void detectPeople(int val) { // 조도 센서의 사람 유무 판단
	if (val <= 20) { //밝기가 20이하이면 사람 있는 것으로 감지 
		people = people + 1; 
	}
}


