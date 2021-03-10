/*
 Name:		MagicImageWand_M5Stack.ino
 Created:	2/21/2021 8:49:27 AM
 Author:	martin
*/

// the setup function runs once when you press reset or power the board
// By Ponticelli Domenico.
// 12NOV2020 Added touch screen support for M5Core2 By Zontex
// https://github.com/pcelli85/M5Stack_FlappyBird_game



#include <M5Core2.h>
#include <EEPROM.h>

Button myButton(50, 70, 220, 100, false, "Button",
	{ YELLOW, BLACK, NODRAW },
	{ RED, BLACK, NODRAW });

Gesture swipeDown("swipe down", 100, DIR_DOWN, 30);
Gesture swipeUp("swipe up", 100, DIR_UP, 30);
//Gesture fromTop(Zone(0, 0, 360, 30), ANYWHERE, "from top", 100, DIR_DOWN, 30);

void setup()
{
	m5.begin();
	m5.Lcd.setTextColor(WHITE);
	m5.Lcd.setFreeFont(FSS12);
	m5.Lcd.setCursor(0, m5.Lcd.fontHeight());
	m5.Lcd.setTextSize(1);
	m5.Lcd.print("hello world");
	M5.Buttons.setFont(FSS18);
	M5.Buttons.draw();
	myButton.longPressTime = 700;
}
void loop()
{
	m5.update();
	if (swipeDown.wasDetected()) {
		m5.Lcd.println("swipe down");
	}
	if (swipeUp.wasDetected()) {
		m5.Lcd.println("swipe up");
	}
	if (myButton.event == E_DBLTAP)
		m5.Lcd.println("dbl");
	if (myButton.event == E_LONGPRESSING)
		m5.Lcd.println("long");
	if (myButton.event == E_TAP) {
		m5.Lcd.println("tap");
	}
	if (m5.background.event == E_TAP) {
		m5.Lcd.println("background");
	}
	if (m5.BtnA.isPressed()) {
		m5.Lcd.setCursor(0, m5.Lcd.fontHeight());
		m5.Lcd.println("pressed A: ");
	}
	if (m5.BtnA.wasPressed()) {
		m5.Lcd.setCursor(0, 0);
		float bat = AXP192().GetBatPower();
		m5.Lcd.println("bat: " + String(bat));
	}
	if (m5.BtnB.wasPressed()) {
		m5.Lcd.clear();
		m5.Lcd.println("You pressed B");
	}
	if (m5.BtnC.wasPressed()) {
		m5.Lcd.clearDisplay();
		m5.Lcd.setCursor(0, 0);
		m5.Lcd.qrcode("Magic Image Wand");
	}
}