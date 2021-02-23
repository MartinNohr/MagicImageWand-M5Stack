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
void setup()
{
	m5.begin();
	//m5.Lcd.begin();
	m5.Lcd.setTextColor(GREEN);
	m5.Lcd.setCursor(0, 0);
	m5.Lcd.setTextSize(2);
	m5.Lcd.print("hello green world");
}

void loop()
{
	m5.update();
	if (m5.BtnA.wasPressed()) {
		m5.Lcd.clearDisplay();
		m5.Lcd.setCursor(0, 0);
		float bat = AXP192().GetBatPower();
		m5.Lcd.println("bat: " + String(bat));
	}
	if (m5.BtnB.wasPressed()) {
		m5.Lcd.println("You pressed B");
	}
	if (m5.BtnC.wasPressed()) {
		m5.Lcd.clearDisplay();
		m5.Lcd.setCursor(0, 0);
		m5.Lcd.qrcode("Magic Image Wand");
	}
}