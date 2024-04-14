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
#define SPI_CHAN1 0 // ���� ���� 1
#define SPI_CHAN2 1 // ���� ���� 2
#define SPI_CHAN3 2 // ���� ���� 3
#define SPI_CHAN4 3 // ���� ���� 4
#define SPI_CHAN5 4 // ���� ���� 5
int people = 0; //��� ��

void printLCD(int* temperature, int people); // LCD�� �µ��� ��� ���� ����ϴ� �Լ�
void ClrLcd(); // LCD�� �ʱ�ȭ�ϴ� �Լ�
void lcdLoc(int line); // LCD�� ����� line�� �����ϴ� �Լ� 

// I2C ��� ���� �Լ�
void lcd_byte(int bits, int mode);
void lcd_toggle_enable(int bits);
void lcd_init();
int fd;

 // �µ� ��� ���� �Լ�
void typeInt(int i); 
void typeln(const char* s);
void typeln(const char* s);

void AirconMaintain(); // �������� �� ����� �Ѱ�, ���⸦ �ݺ��ϴ� �Լ�
void AirconTurnoff(); // �������� �� ����� Ű�� �Լ�
void AirconTurnon(); // �������� �� ����� ���� �Լ�

void TurnLED(int people); //LED�� Ű�� �Լ�
void ledInit(void); // LED ������ �ʱ�ȭ�ϴ� �Լ�
void ledColorSet(uchar r_val, uchar g_val, uchar b_val); // LED ���� �����ϴ� �Լ�

void MeasureTemperature(); // �µ��� �����ϴ� �Լ�

void People(); // ��� ���� �����ϴ� �Լ�
void detectPeople(int val); // ����� �ν��ϴ� �Լ�

int main(void)
{
	void (*fp)() = AirconTurnoff; // ó������ �������� ���� �����Ѵ�.
	if (wiringPiSetup() == -1) exit(1); // wiringPi ����
	if (wiringPiSPISetup(0, 500000) == -1) exit(1);  //wiringPiSPI ����
	mcp3004Setup(BASE, 0); // mcp3008 ��� ����

	while (1) {
		int past_people = people; // ���� �°� �� ����

		People(); // ���� �°� �� ����
		int temperature[2] = { dhtVal[2], dhtVal[3] }; // �µ��� ������, �Ҽ��� �迭�� ����
		printLCD(temperature, people); // �µ��� �°� �� LCD�� ���
		TurnLED(people); // ��� ���� �´� LED �� ON

		if (past_people != people) { // �°� ���� �޶����� ��
				
			if (people > 0 && people < 5) { //0�� �̻� 4�� ���� �� ��� - AirconMaintain()
				fp = AirconMaintain;
			}
			else if (people == 0) { //0���� ��� - AirconTurnoff()
				fp = AirconTurnoff;
			}
			else if (people >= 5) {	//5���� ��� - AirconTurnon ()
				fp = AirconTurnon;
			}
		}

		//������ ������ ��� �ѱ�
		fp();

		MeasureTemperature(); // �µ� ����
		
		delay(8500); //8.5�� delay
	}
	return(0);
}

// LCD�� �µ��� ��� ���� ����ϴ� �Լ�
void printLCD(int* temperature, int people) {
	fd = wiringPiI2CSetup(I2C_ADDR); //I2C ��� ����

	lcd_init(); // LCD ���� �ʱ�ȭ
	ClrLcd(); // LCD �ʱ�ȭ

	lcdLoc(LINE1); // LCD ù��° ��
	typeln("now : "); //"now: " ���
	typeInt(temperature[0]); // �µ� ������ ���
	typeln("."); // "." ���
	typeInt(temperature[1]); // �µ� �Ҽ��� ���
	typeln("C"); // "C" ���

	lcdLoc(LINE2); // LCD �ι�° ��
	typeln("people : "); // "peope: " ���
	typeInt(people); // ��� �� ���
	typeln("/5"); // "/5" ���
}

// LCD ������ ���
void typeInt(int i) {
	char array1[20];
	sprintf(array1, "%d", i);
	typeln(array1);
}

// LCD �ʱ�ȭ
void ClrLcd() {
	lcd_byte(0x01, LCD_CMD);
	lcd_byte(0x02, LCD_CMD);
}

// LCD ����
void lcdLoc(int line) {
	lcd_byte(line, LCD_CMD);
}

// �µ� ������ ����
void typeln(const char* s) {
	while (*s) lcd_byte(*(s++), LCD_CHR);
}

//LCD ��� ����
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

// LCD ��� ����
void lcd_toggle_enable(int bits) {
	delayMicroseconds(500);
	wiringPiI2CReadReg8(fd, (bits | ENABLE));
	delayMicroseconds(500);
	wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
	delayMicroseconds(500);
}

// LCD ��� ����
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


void AirconMaintain() { // �������� �� ����� �Ѱ�, ���⸦ �ݺ��ϴ� �Լ�
	pinMode(REL, OUTPUT); 

	digitalWrite(REL, LOW); // 4�� ���� ����
	delay(4000);

	digitalWrite(REL, HIGH); // 3�� ���� �ѱ�
	delay(3000);
}

void AirconTurnoff() { // �������� �� ����� ���� �Լ�
	pinMode(REL, OUTPUT);
	digitalWrite(REL, LOW); // ����

	return;
}

void AirconTurnon() { // �������� �� ����� Ű�� �Լ�
	pinMode(REL, OUTPUT);

	digitalWrite(REL, HIGH); // �ѱ�

	return;
}

void TurnLED(int people) { // LED�� Ű�� �Լ�
	ledInit(); // LED ���� �ʱ�ȭ
	if (people >= 5) { // �°��� 5�� �̻��̶��
		ledInit();
		ledColorSet(0xff, 0x00, 0x00);   // Red �ѱ�
	}
	else { // 4�� ���϶��
		ledInit();
		ledColorSet(0x00, 0xff, 0x00);   // Green �ѱ�
	}
}

// LED �ʱ�ȭ �Լ�
void ledInit(void)
{
	softPwmCreate(LedPinRed, 0, 100);
	softPwmCreate(LedPinGreen, 0, 100);
}

// LED �� ���� �Լ�
void ledColorSet(uchar r_val, uchar g_val, uchar b_val)
{
	softPwmWrite(LedPinRed, r_val);
	softPwmWrite(LedPinGreen, g_val);
}


void MeasureTemperature() // �µ� ���� �Լ�
{
	uint8_t laststate = HIGH;
	uint8_t counter = 0;
	uint8_t j = 0, i;

	dhtVal[0] = dhtVal[1] = dhtVal[2] = dhtVal[3] = dhtVal[4] = 0;

	// MCU 18ms ���� LOW signal ����
	pinMode(DHTPIN, OUTPUT); 	
	digitalWrite(DHTPIN, LOW); //pull down
	delay(18); // dealy

	//40micros ���� HIGH signal ����
	digitalWrite(DHTPIN, HIGH); //pull up
	delayMicroseconds(40); // deay

	//DHT�� signal ����
	pinMode(DHTPIN, INPUT);

	// DHT�� MCU�� data ������ ����
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

void People() { // ��� ���� �Լ�
	people = 0;  //��� �� ���� �ʱ�ȭ 

	int val1 = analogRead(BASE + SPI_CHAN1); // 1��° �������� �� �б�
	int val2 = analogRead(BASE + SPI_CHAN2); // 2��° �������� �� �б�
	int val3 = analogRead(BASE + SPI_CHAN3); // 3��° �������� �� �б�
	int val4 = analogRead(BASE + SPI_CHAN4); // 4��° �������� �� �б�
	int val5 = analogRead(BASE + SPI_CHAN5); // 5��° �������� �� �б�

	// �� ���� ������ ��� ���� �Ǵ�
	detectPeople(val1); 
	detectPeople(val2);
	detectPeople(val3);
	detectPeople(val4);
	detectPeople(val5);
}

void detectPeople(int val) { // ���� ������ ��� ���� �Ǵ�
	if (val <= 20) { //��Ⱑ 20�����̸� ��� �ִ� ������ ���� 
		people = people + 1; 
	}
}


