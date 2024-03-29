/*
 Name:		MagicImageWand_M5Stack.ino
 Created:	2/21/2021 8:49:27 AM
 Author:	martin
*/

#include <M5Core2.h>
#undef min

#include <EEPROM.h>
#include "fonts.h"

#include "MagicImageWand-M5Stack.h"

#define tft m5.Lcd

Gesture swipeDown("swipe down", 100, DIR_DOWN, 30);
Gesture swipeUp("swipe up", 100, DIR_UP, 30);

RTC_DATA_ATTR int nBootCount = 0;

// some forward references that Arduino IDE needs
int IRAM_ATTR readByte(bool clear);
void IRAM_ATTR ReadAndDisplayFile(bool doingFirstHalf);
uint16_t IRAM_ATTR readInt();
uint32_t IRAM_ATTR readLong();
void IRAM_ATTR FileSeekBuf(uint32_t place);
int FileCountOnly(int start = 0);

//static const char* TAG = "lightwand";
//esp_timer_cb_t oneshot_timer_callback(void* arg)
void IRAM_ATTR oneshot_LED_timer_callback(void* arg)
{
	bStripWaiting = false;
	//int64_t time_since_boot = esp_timer_get_time();
	//Serial.println("in isr");
	//ESP_LOGI(TAG, "One-shot timer called, time since boot: %lld us", time_since_boot);
}

void setup()
{
	m5.begin(true, true, true, false);
	Serial.begin(115200);
	delay(10);
	tft.setFreeFont(FSS12);
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(1);
	tft.textdatum = TL_DATUM;
	CRotaryDialButton::begin(DIAL_A, DIAL_B, DIAL_BTN);
	m5.BtnA.longPressTime = 700;
	m5.background.longPressTime = 700;
	setupSDcard();
	//listDir(SD, "/", 2, "");
	//gpio_set_direction((gpio_num_t)LED, GPIO_MODE_OUTPUT);
	//digitalWrite(LED, HIGH);
	gpio_set_direction((gpio_num_t)FRAMEBUTTON, GPIO_MODE_INPUT);
	gpio_set_pull_mode((gpio_num_t)FRAMEBUTTON, GPIO_PULLUP_ONLY);
	// init the onboard buttons
	gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
	gpio_set_pull_mode(GPIO_NUM_0, GPIO_PULLUP_ONLY);
	gpio_set_direction(GPIO_NUM_35, GPIO_MODE_INPUT);
	//gpio_set_pull_mode(GPIO_NUM_35, GPIO_PULLUP_ONLY); // not needed since there are no pullups on 35, they are input only

	oneshot_LED_timer_args = {
				oneshot_LED_timer_callback,
				/* argument specified here will be passed to timer callback function */
				(void*)0,
				ESP_TIMER_TASK,
				"one-shotLED"
	};
	esp_timer_create(&oneshot_LED_timer_args, &oneshot_LED_timer);

	//WiFi
	WiFi.softAP(ssid, password);
	IPAddress myIP = WiFi.softAPIP();
	// save for the menu system
	strncpy(localIpAddress, myIP.toString().c_str(), sizeof(localIpAddress));
	Serial.print("AP IP address: ");
	Serial.println(myIP);
	server.begin();
	Serial.println("Server started");
	server.on("/", HomePage);
	server.on("/download", File_Download);
	server.on("/upload", File_Upload);
	server.on("/settings", ShowSettings);
	server.on("/fupload", HTTP_POST, []() { server.send(200); }, handleFileUpload);
	///////////////////////////// End of Request commands
	server.begin();

	int width = tft.width();
	int height = tft.height();
	tft.fillScreen(TFT_BLACK);
	tft.setTextColor(menuLineActiveColor);
	rainbow_fill();
	tft.setFreeFont(&Dialog_bold_16);
	if (nBootCount == 0)
	{
		tft.setTextColor(TFT_BLACK);
		tft.setFreeFont(&Irish_Grover_Regular_24);
		tft.drawRect(0, 0, width - 1, height - 1, menuLineActiveColor);
		tft.drawString("Magic Image Wand", 5, 10);
		tft.setFreeFont(&Dialog_bold_16);
		tft.drawString("Version " + myVersion, 20, 90);
		tft.setTextSize(1);
		tft.drawString(__DATE__, 20, 110);
	}
	delay(1000);
	//tft.setFreeFont(&Dialog_bold_16);
	tft.setFreeFont(FSS12);
	tft.setTextColor(menuLineActiveColor);

	// this will fix the signature if necessary
	if (SaveSettings(false, true)) {
		// get the autoload flag
		SaveSettings(false, false, true);
	}
	// load the saved settings if flag is true and the button isn't pushed
	if ((nBootCount == 0) && bAutoLoadSettings && gpio_get_level((gpio_num_t)DIAL_BTN)) {
		// read all the settings
		SaveSettings(false);
	}

	menuPtr = new MenuInfo;
	MenuStack.push(menuPtr);
	MenuStack.top()->menu = MainMenu;
	MenuStack.top()->index = 0;
	MenuStack.top()->offset = 0;

	FastLED.addLeds<NEOPIXEL, DATA_PIN1>(leds, 0, NUM_LEDS);
	//FastLED.addLeds<NEOPIXEL, DATA_PIN2>(leds, 0, NUM_LEDS);	// to test parallel second strip
	//if (bSecondStrip)
	// create the second led controller
	FastLED.addLeds<NEOPIXEL, DATA_PIN2>(leds, NUM_LEDS, NUM_LEDS);
	//FastLED.setTemperature(whiteBalance);
	FastLED.setTemperature(CRGB(whiteBalance.r, whiteBalance.g, whiteBalance.b));
	FastLED.setBrightness(nStripBrightness);
	FastLED.setMaxPowerInVoltsAndMilliamps(5, nStripMaxCurrent);
	if (nBootCount == 0) {
		//bool oldSecond = bSecondStrip;
		//bSecondStrip = true;
		// show 3 pixels on each end red and green, I had a strip that only showed 142 pixels, this will help detect that failure
		SetPixel(0, CRGB::Red);
		SetPixel(1, CRGB::Red);
		SetPixel(2, CRGB::Red);
		SetPixel(143, CRGB::Red);
		SetPixel(142, CRGB::Red);
		SetPixel(141, CRGB::Red);
		FastLED.show();
		delay(100);
		SetPixel(0, CRGB::Green);
		SetPixel(1, CRGB::Green);
		SetPixel(2, CRGB::Green);
		SetPixel(143, CRGB::Green);
		SetPixel(142, CRGB::Green);
		SetPixel(141, CRGB::Green);
		FastLED.show();
		delay(100);
		FastLED.clear(true);
		RainbowPulse();
		//fill_noise8(leds, 144, 2, 0, 10, 2, 0, 0, 10);
		//FastSPI_LED.show();
		//delay(5000);
		//bSecondStrip = oldSecond;
		//// Turn the LED on, then pause
		//SetPixel(0, CRGB::Red);
		//SetPixel(1, CRGB::Red);
		//SetPixel(4, CRGB::Green);
		//SetPixel(5, CRGB::Green);
		//SetPixel(8, CRGB::Blue);
		//SetPixel(9, CRGB::Blue);
		//SetPixel(12, CRGB::White);
		//SetPixel(13, CRGB::White);
		//SetPixel(NUM_LEDS - 0, CRGB::Red);
		//SetPixel(NUM_LEDS - 1, CRGB::Red);
		//SetPixel(NUM_LEDS - 4, CRGB::Green);
		//SetPixel(NUM_LEDS - 5, CRGB::Green);
		//SetPixel(NUM_LEDS - 8, CRGB::Blue);
		//SetPixel(NUM_LEDS - 9, CRGB::Blue);
		//SetPixel(NUM_LEDS - 12, CRGB::White);
		//SetPixel(NUM_LEDS - 13, CRGB::White);
		//SetPixel(0 + NUM_LEDS, CRGB::Red);
		//SetPixel(1 + NUM_LEDS, CRGB::Red);
		//SetPixel(4 + NUM_LEDS, CRGB::Green);
		//SetPixel(5 + NUM_LEDS, CRGB::Green);
		//SetPixel(8 + NUM_LEDS, CRGB::Blue);
		//SetPixel(9 + NUM_LEDS, CRGB::Blue);
		//SetPixel(12 + NUM_LEDS, CRGB::White);
		//SetPixel(13 + NUM_LEDS, CRGB::White);
		//for (int ix = 0; ix < 255; ix += 5) {
		//	FastLED.setBrightness(ix);
		//	FastLED.show();
		//	delayMicroseconds(50);
		//}
		//for (int ix = 255; ix >= 0; ix -= 5) {
		//	FastLED.setBrightness(ix);
		//	FastLED.show();
		//	delayMicroseconds(50);
		//}
		//delayMicroseconds(50);
		//FastLED.clear(true);
		//delayMicroseconds(50);
		//FastLED.setBrightness(nStripBrightness);
		//delay(50);
		//// Now turn the LED off
		//FastLED.clear(true);
		//delayMicroseconds(50);
		//// run a white dot up the display and back
		//for (int ix = 0; ix < STRIPLENGTH; ++ix) {
		//	SetPixel(ix, CRGB::White);
		//	if (ix)
		//		SetPixel(ix - 1, CRGB::Black);
		//	FastLED.show();
		//	delayMicroseconds(50);
		//}
		//for (int ix = STRIPLENGTH - 1; ix >= 0; --ix) {
		//	SetPixel(ix, CRGB::White);
		//	if (ix)
		//		SetPixel(ix + 1, CRGB::Black);
		//	FastLED.show();
		//	delayMicroseconds(50);
		//}
	}
	FastLED.clear(true);
	tft.fillScreen(TFT_BLACK);

	// wait for button release
	while (!digitalRead(DIAL_BTN))
		;
	delay(30);	// debounce
	while (!digitalRead(DIAL_BTN))
		;
	// clear the button buffer
	CRotaryDialButton::clear();
	if (!bSdCardValid) {
		DisplayCurrentFile();
		delay(1000);
		ToggleFilesBuiltin(NULL);
		tft.fillScreen(TFT_BLACK);
	}

	DisplayCurrentFile();
	/*
		analogSetCycles(8);                   // Set number of cycles per sample, default is 8 and provides an optimal result, range is 1 - 255
		analogSetSamples(1);                  // Set number of samples in the range, default is 1, it has an effect on sensitivity has been multiplied
		analogSetClockDiv(1);                 // Set the divider for the ADC clock, default is 1, range is 1 - 255
		analogSetAttenuation(ADC_11db);       // Sets the input attenuation for ALL ADC inputs, default is ADC_11db, range is ADC_0db, ADC_2_5db, ADC_6db, ADC_11db
		//analogSetPinAttenuation(36, ADC_11db); // Sets the input attenuation, default is ADC_11db, range is ADC_0db, ADC_2_5db, ADC_6db, ADC_11db
		analogSetPinAttenuation(37, ADC_11db);
		// ADC_0db provides no attenuation so IN/OUT = 1 / 1 an input of 3 volts remains at 3 volts before ADC measurement
		// ADC_2_5db provides an attenuation so that IN/OUT = 1 / 1.34 an input of 3 volts is reduced to 2.238 volts before ADC measurement
		// ADC_6db provides an attenuation so that IN/OUT = 1 / 2 an input of 3 volts is reduced to 1.500 volts before ADC measurement
		// ADC_11db provides an attenuation so that IN/OUT = 1 / 3.6 an input of 3 volts is reduced to 0.833 volts before ADC measurement
	//   adcAttachPin(VP);                     // Attach a pin to ADC (also clears any other analog mode that could be on), returns TRUE/FALSE result
	//   adcStart(VP);                         // Starts an ADC conversion on attached pin's bus
	//   adcBusy(VP);                          // Check if conversion on the pin's ADC bus is currently running, returns TRUE/FALSE result
	//   adcEnd(VP);

		//adcAttachPin(36);
		adcAttachPin(37);
	*/
	MenuButtons();
	m5.Axp.SetLed(0);
}

void loop()
{
	static bool didsomething = false;
	bool lastStrip = bSecondStrip;
	didsomething = bSettingsMode ? HandleMenus() : HandleRunMode();
	server.handleClient();
	// wait for no keys
	if (didsomething) {
		didsomething = false;
		delay(1);
	}
	static bool bButton0 = false;
	// check preview button
	if (bButton0 && digitalRead(0)) {
		bButton0 = false;
	}
	if (!bButton0 && digitalRead(0) == 0) {
		// debounce
		delay(30);
		if (digitalRead(0) == 0) {
			ShowBmp(NULL);
			bButton0 = true;
			// restore the screen to what it was doing before the bmp display
			if (bSettingsMode) {
				ShowMenu(MenuStack.top()->menu);
			}
			else {
				tft.fillScreen(TFT_BLACK);
				DisplayCurrentFile(bShowFolder);
			}
		}
	}
}


// build menu buttons
void MenuButtons()
{
	int charHeight = tft.fontHeight();
	for (int ix = 0; ix < btnMenuArray.size(); ++ix) {
		btnMenuArray[ix] = new Button(0, ix * charHeight, 320, charHeight - 1, false, "menu");
	}
}

// enable or disable the menu buttons
void EnableButtons(bool enable)
{
	for (int ix = 0; ix < btnMenuArray.size(); ++ix) {
		if (enable)
			btnMenuArray[ix]->draw();
		else
			btnMenuArray[ix]->hide();
	}
}

bool RunMenus(int button)
{
	// save this so we can see if we need to save a new changed value
	bool lastAutoLoadFlag = bAutoLoadSettings;
	// see if we got a menu match
	bool gotmatch = false;
	int menuix = 0;
	MenuInfo* oldMenu;
	bool bExit = false;
	for (int ix = 0; !gotmatch && MenuStack.top()->menu[ix].op != eTerminate; ++ix) {
		// see if this is one is valid
		if (!bMenuValid[ix]) {
			continue;
		}
		//Serial.println("menu button: " + String(button));
		if (button == BTN_SELECT && menuix == MenuStack.top()->index) {
			//Serial.println("got match " + String(menuix) + " " + String(MenuStack.top()->index));
			gotmatch = true;
			//Serial.println("clicked on menu");
			// got one, service it
			switch (MenuStack.top()->menu[ix].op) {
			case eText:
			case eTextInt:
			case eTextCurrentFile:
			case eBool:
				bMenuChanged = true;
				if (MenuStack.top()->menu[ix].function) {
					(*MenuStack.top()->menu[ix].function)(&MenuStack.top()->menu[ix]);
				}
				break;
			case eList:
				bMenuChanged = true;
				if (MenuStack.top()->menu[ix].function) {
					(*MenuStack.top()->menu[ix].function)(&MenuStack.top()->menu[ix]);
				}
				bExit = true;
				// if there is a value, set the min value in it
				if (MenuStack.top()->menu[ix].value) {
					*(int*)MenuStack.top()->menu[ix].value = MenuStack.top()->menu[ix].min;
				}
				break;
			case eMenu:
				if (MenuStack.top()->menu) {
					oldMenu = MenuStack.top();
					MenuStack.push(new MenuInfo);
					MenuStack.top()->menu = oldMenu->menu[ix].menu;
					bMenuChanged = true;
					MenuStack.top()->index = 0;
					MenuStack.top()->offset = 0;
					//Serial.println("change menu");
					// check if the new menu is an eList and if it has a value, if it does, set the index to it
					if (MenuStack.top()->menu->op == eList && MenuStack.top()->menu->value) {
						int ix = *(int*)MenuStack.top()->menu->value;
						MenuStack.top()->index = ix;
						// adjust offset if necessary
						if (ix > 4) {
							MenuStack.top()->offset = ix - 4;
						}
					}
				}
				break;
			case eBuiltinOptions: // find it in builtins
				if (BuiltInFiles[CurrentFileIndex].menu != NULL) {
					MenuStack.top()->index = MenuStack.top()->index;
					MenuStack.push(new MenuInfo);
					MenuStack.top()->menu = BuiltInFiles[CurrentFileIndex].menu;
					MenuStack.top()->index = 0;
					MenuStack.top()->offset = 0;
				}
				else {
					WriteMessage("No settings available for:\n" + String(BuiltInFiles[CurrentFileIndex].text));
				}
				bMenuChanged = true;
				break;
			case eExit: // go back a level
				bExit = true;
				break;
			case eReboot:
				WriteMessage("Rebooting in 2 seconds\nHold button for factory reset", false, 2000);
				ESP.restart();
				break;
			}
		}
		++menuix;
	}
	// if no match, and we are in a submenu, go back one level, or if bExit is set
	if (bExit || (!bMenuChanged && MenuStack.size() > 1)) {
		bMenuChanged = true;
		menuPtr = MenuStack.top();
		MenuStack.pop();
		delete menuPtr;
	}
	// see if the autoload flag changed
	if (bAutoLoadSettings != lastAutoLoadFlag) {
		// the flag is now true, so we should save the current settings
		SaveSettings(true, false, true);
	}
}

// display the menu
// if MenuStack.top()->index is > MENU_LINES, then shift the lines up by enough to display them
// remember that we only have room for MENU_LINES lines
void ShowMenu(struct MenuItem* menu)
{
	MenuStack.top()->menucount = 0;
	int y = 0;
	int x = 0;
	char line[100];
	bool skip = false;
	// loop through the menu
	for (int menix = 0; menu->op != eTerminate; ++menu, ++menix) {
		// make sure menu valid vector is big enough
		if (bMenuValid.size() < menix + 1) {
			bMenuValid.resize(menix + 1);
		}
		bMenuValid[menix] = false;
		switch (menu->op) {
		case eIfEqual:
			// skip the next one if match, only booleans are handled so far
			skip = *(bool*)menu->value != (menu->min ? true : false);
			//Serial.println("ifequal test: skip: " + String(skip));
			break;
		case eElse:
			skip = !skip;
			break;
		case eEndif:
			skip = false;
			break;
		}
		if (skip) {
			bMenuValid[menix] = false;
			continue;
		}
		char line[100], xtraline[100];
		// only displayable menu items should be in this switch
		line[0] = '\0';
		int val;
		bool exists;
		switch (menu->op) {
		case eTextInt:
		case eText:
		case eTextCurrentFile:
			bMenuValid[menix] = true;
			if (menu->value) {
				val = *(int*)menu->value;
				if (menu->op == eText) {
					sprintf(line, menu->text, (char*)(menu->value));
				}
				else if (menu->op == eTextInt) {
					sprintf(line, menu->text, (int)(val / pow10(menu->decimals)), val % (int)(pow10(menu->decimals)));
				}
			}
			else {
				if (menu->op == eTextCurrentFile) {
					sprintf(line, menu->text, MakeMIWFilename(FileNames[CurrentFileIndex], false).c_str());
				}
				else {
					strcpy(line, menu->text);
				}
			}
			// next line
			++y;
			break;
		case eList:
			bMenuValid[menix] = true;
			// the list of macro files
			// min holds the macro number
			val = menu->min;
			// see if the macro is there and append the text
			exists = SD.exists("/" + String(val) + ".miw");
			sprintf(line, menu->text, val, exists ? menu->on : menu->off);
			// next line
			++y;
			break;
		case eBool:
			bMenuValid[menix] = true;
			if (menu->value) {
				// clean extra bits, just in case
				bool* pb = (bool*)menu->value;
				//*pb &= 1;
				sprintf(line, menu->text, *pb ? menu->on : menu->off);
				//Serial.println("bool line: " + String(line));
			}
			else {
				strcpy(line, menu->text);
			}
			// increment displayable lines
			++y;
			break;
		case eBuiltinOptions:
			// for builtins only show if available
			if (BuiltInFiles[CurrentFileIndex].menu != NULL) {
				bMenuValid[menix] = true;
				sprintf(line, menu->text, BuiltInFiles[CurrentFileIndex].text);
				++y;
			}
			break;
		case eMenu:
		case eExit:
		case eReboot:
			bMenuValid[menix] = true;
			if (menu->value) {
				sprintf(xtraline, menu->text, *(int*)menu->value);
			}
			else {
				strcpy(xtraline, menu->text);
			}
			if (menu->op == eExit)
				sprintf(line, "%s%s", "-", xtraline);
			else
				sprintf(line, "%s%s", (menu->op == eReboot) ? "" : "+", xtraline);
			++y;
			break;
		}
		if (strlen(line) && y >= MenuStack.top()->offset) {
			DisplayMenuLine(y - 1, y - 1 - MenuStack.top()->offset, line);
		}
	}
	MenuStack.top()->menucount = y;
	// blank the rest of the lines
	for (int ix = y; ix < MENU_LINES; ++ix) {
		DisplayLine(ix, "");
	}
	// show line if menu has been scrolled
	if (MenuStack.top()->offset > 0)
		tft.drawLine(0, 0, 5, 0, menuLineActiveColor);
	// show bottom line if last line is showing
	if (MenuStack.top()->offset + (MENU_LINES - 1) < MenuStack.top()->menucount - 1)
		tft.drawLine(0, tft.height() - 1, 5, tft.height() - 1, menuLineActiveColor);
	else
		tft.drawLine(0, tft.height() - 1, 5, tft.height() - 1, TFT_BLACK);
	// see if we need to clean up the end, like when the menu shrank due to a choice
	int extra = MenuStack.top()->menucount - MenuStack.top()->offset - MENU_LINES;
	while (extra < 0) {
		DisplayLine(MENU_LINES + extra, "");
		++extra;
	}
}

// switch between SD and built-ins
void ToggleFilesBuiltin(MenuItem* menu)
{
	// clear filenames list
	FileNames.clear();
	bool lastval = bShowBuiltInTests;
	int oldIndex = CurrentFileIndex;
	String oldFolder = currentFolder;
	if (menu != NULL) {
		ToggleBool(menu);
	}
	else {
		bShowBuiltInTests = !bShowBuiltInTests;
	}
	if (lastval != bShowBuiltInTests) {
		if (bShowBuiltInTests) {
			CurrentFileIndex = 0;
			for (int ix = 0; ix < sizeof(BuiltInFiles) / sizeof(*BuiltInFiles); ++ix) {
				// add each one
				FileNames.push_back(String(BuiltInFiles[ix].text));
			}
			currentFolder = "";
		}
		else {
			// read the SD
			currentFolder = lastFolder;
			GetFileNamesFromSD(currentFolder);
		}
	}
	// restore indexes
	CurrentFileIndex = lastFileIndex;
	lastFileIndex = oldIndex;
	currentFolder = lastFolder;
	lastFolder = oldFolder;
}

// toggle a boolean value
void ToggleBool(MenuItem* menu)
{
	bool* pb = (bool*)menu->value;
	*pb = !*pb;
	if (menu->change != NULL) {
		(*menu->change)(menu, -1);
	}
	//Serial.println("autoload: " + String(bAutoLoadSettings));
	//Serial.println("fixed time: " + String(bFixedTime));
}

#define BTN_INT_WIDTH 70
#define BTN_INT_HEIGHT 48
#define BTN_INT_XGAP 4
#define BTN_INT_YGAP 4
// button version of GetIntegerValue
void GetIntegerValue(MenuItem* menu)
{
	m5.update();
	if (menu->change != NULL) {
		(*menu->change)(menu, 1);
	}
	// -1 means to reset to original
	tft.pushState();
	tft.fillScreen(BLACK);
	const char* fmt = menu->decimals ? "%ld.%ld" : "%ld";
	char minstr[20], maxstr[20];
	sprintf(minstr, fmt, menu->min / (int)pow10(menu->decimals), menu->min % (int)pow10(menu->decimals));
	sprintf(maxstr, fmt, menu->max / (int)pow10(menu->decimals), menu->max % (int)pow10(menu->decimals));
	DisplayLine(1, String(minstr), 230);
	DisplayLine(2, "to", 230);
	DisplayLine(3, String(maxstr), 230);
	int oldVal = *(int*)menu->value;
	DisplayLine(0, String(*(int*)(menu->value)));
	// build some buttons
	std::array<Button*, 12> btnArray;
	for (int ix = 0; ix < btnArray.size(); ++ix) {
		static char* txt[12];
		char tmp[4];
		int xpos, ypos;
		// set the text
		switch (ix) {
		case 0:
			txt[ix] = "+/-";
			break;
		case 1:
			txt[ix] = "0";
			break;
		case 2:
			txt[ix] = "DEL";
			break;
		default:
			txt[ix] = tmp;
			sprintf(txt[ix], "%d", ix - 2);
			break;
		}
		xpos = (BTN_INT_WIDTH + BTN_INT_XGAP) * (ix % 3);
		ypos = (240 - BTN_INT_HEIGHT - BTN_INT_YGAP) - ((BTN_INT_HEIGHT + BTN_INT_YGAP) * (ix / 3));
		btnArray[ix] = new Button(xpos, ypos, BTN_INT_WIDTH, BTN_INT_HEIGHT, false, txt[ix], { YELLOW, BLACK, NODRAW }, { GREEN, BLACK, NODRAW });
	}
	// add an OK button
	Button btnOK(320 - BTN_INT_WIDTH, 240 - BTN_INT_HEIGHT - BTN_INT_YGAP, BTN_INT_WIDTH, BTN_INT_HEIGHT, false, "OK", { YELLOW, BLACK, NODRAW }, { GREEN, BLACK, NODRAW });
	// add a cancel button
	Button btnCancel(320 - BTN_INT_WIDTH, 240 - 2 * (BTN_INT_HEIGHT + BTN_INT_YGAP), BTN_INT_WIDTH, BTN_INT_HEIGHT, false, "CANCEL", { YELLOW, BLACK, NODRAW }, { GREEN, BLACK, NODRAW });
	// clear the buttons, for some reason the cancel one is often set to tapped
	btnCancel.cancel();
	btnOK.cancel();
	char line[50];
	bool done = false;
	bool redraw = true;
	bool editing = false;
	int value = *(int*)menu->value;
	// put the current value in the string
	do {
		m5.update();
		if (redraw) {
			sprintf(line, menu->text, value / (int)pow10(menu->decimals), abs(value) % (int)pow10(menu->decimals));
			DisplayLine(0, line);
			redraw = false;
		}
		for (int ix = 0; ix < btnArray.size(); ++ix) {
			if (btnArray[ix]->event == E_TAP) {
				redraw = true;
				// clear if the first time
				if (!editing) {
					value = 0;
					editing = true;
				}
				switch (ix) {
				case 0:	// toggle sign
					value = -value;
					break;
				case 1:		// 0
					value *= 10;
					break;
				case 2:		// DEL (backspace)
					value /= 10;
					break;
				default:	// numbers+2
					value = value * 10 + (ix - 2);
					break;
				}
			}
		}
		// check for ok or cancel button
		if (btnCancel.event == E_TAP) {
			//Serial.println("cancel");
			done = true;
		}
		if (btnOK.event == E_TAP) {
			// check the value
			if (value <= menu->max && value >= menu->min) {
				done = true;
				// save the new value
				*(int*)(menu->value) = value;
			}
			else {
				redraw = true;
				DisplayLine(0, "Value out of range", 0, RED);
				delay(2000);
			}
		}
	} while (!done);
	tft.popState();
	tft.fillScreen(BLACK);
	// delete the buttons
	for (int ix = 0; ix < btnArray.size(); ++ix) {
		delete btnArray[ix];
	}
}
//
//// get integer values
//void GetIntegerValue(MenuItem* menu)
//{
//	tft.fillScreen(TFT_BLACK);
//	// -1 means to reset to original
//	int stepSize = 1;
//	int originalValue = *(int*)menu->value;
//	//Serial.println("int: " + String(menu->text) + String(*(int*)menu->value));
//	char line[50];
//	CRotaryDialButton::Button button = BTN_NONE;
//	bool done = false;
//	const char* fmt = menu->decimals ? "%ld.%ld" : "%ld";
//	char minstr[20], maxstr[20];
//	sprintf(minstr, fmt, menu->min / (int)pow10(menu->decimals), menu->min % (int)pow10(menu->decimals));
//	sprintf(maxstr, fmt, menu->max / (int)pow10(menu->decimals), menu->max % (int)pow10(menu->decimals));
//	DisplayLine(1, String("Range: ") + String(minstr) + " to " + String(maxstr));
//	DisplayLine(3, "Long Press to Accept");
//	int oldVal = *(int*)menu->value;
//	if (menu->change != NULL) {
//		(*menu->change)(menu, 1);
//	}
//	do {
//		//Serial.println("button: " + String(button));
//		switch (button) {
//		case BTN_LEFT:
//			if (stepSize != -1)
//				*(int*)menu->value -= stepSize;
//			break;
//		case BTN_RIGHT:
//			if (stepSize != -1)
//				*(int*)menu->value += stepSize;
//			break;
//		case BTN_SELECT:
//			if (stepSize == -1) {
//				stepSize = 1;
//			}
//			else {
//				stepSize *= 10;
//			}
//			if (stepSize > (menu->max / 10)) {
//				stepSize = -1;
//			}
//			break;
//		case BTN_LONG:
//			if (stepSize == -1) {
//				*(int*)menu->value = originalValue;
//				stepSize = 1;
//			}
//			else {
//				done = true;
//			}
//			break;
//		}
//		// make sure within limits
//		*(int*)menu->value = constrain(*(int*)menu->value, menu->min, menu->max);
//		// show slider bar
//		tft.fillRect(0, 2 * tft.fontHeight(), tft.width() - 1, 6, TFT_BLACK);
//		DrawProgressBar(0, 2 * charHeight + 4, tft.width() - 1, 12, map(*(int*)menu->value, menu->min, menu->max, 0, 100));
//		sprintf(line, menu->text, *(int*)menu->value / (int)pow10(menu->decimals), *(int*)menu->value % (int)pow10(menu->decimals));
//		DisplayLine(0, line);
//		DisplayLine(4, stepSize == -1 ? "Reset: long press (Click +)" : "step: " + String(stepSize) + " (Click +)");
//		if (menu->change != NULL && oldVal != *(int*)menu->value) {
//			(*menu->change)(menu, 0);
//			oldVal = *(int*)menu->value;
//		}
//		while (!done && (button = ReadButton()) == BTN_NONE) {
//			delay(1);
//		}
//	} while (!done);
//	if (menu->change != NULL) {
//		(*menu->change)(menu, -1);
//	}
//}

void UpdateStripBrightness(MenuItem* menu, int flag)
{
	switch (flag) {
	case 1:		// first time
		for (int ix = 0; ix < 64; ++ix) {
			SetPixel(ix, CRGB::White);
		}
		FastLED.show();
		break;
	case 0:		// every change
		FastLED.setBrightness(*(int*)menu->value);
		FastLED.show();
		break;
	case -1:	// last time
		FastLED.clear(true);
		break;
	}
}

void UpdateStripWhiteBalanceR(MenuItem* menu, int flag)
{
	switch (flag) {
	case 1:		// first time
		for (int ix = 0; ix < 64; ++ix) {
			SetPixel(ix, CRGB::White);
		}
		FastLED.show();
		break;
	case 0:		// every change
		FastLED.setTemperature(CRGB(*(int*)menu->value, whiteBalance.g, whiteBalance.b));
		FastLED.show();
		break;
	case -1:	// last time
		FastLED.clear(true);
		break;
	}
}

void UpdateStripWhiteBalanceG(MenuItem* menu, int flag)
{
	switch (flag) {
	case 1:		// first time
		for (int ix = 0; ix < 64; ++ix) {
			SetPixel(ix, CRGB::White);
		}
		FastLED.show();
		break;
	case 0:		// every change
		FastLED.setTemperature(CRGB(whiteBalance.r, *(int*)menu->value, whiteBalance.b));
		FastLED.show();
		break;
	case -1:	// last time
		FastLED.clear(true);
		break;
	}
}

void UpdateStripWhiteBalanceB(MenuItem* menu, int flag)
{
	switch (flag) {
	case 1:		// first time
		for (int ix = 0; ix < 64; ++ix) {
			SetPixel(ix, CRGB::White);
		}
		FastLED.show();
		break;
	case 0:		// every change
		FastLED.setTemperature(CRGB(whiteBalance.r, whiteBalance.g, *(int*)menu->value));
		FastLED.show();
		break;
	case -1:	// last time
		FastLED.clear(true);
		break;
	}
}

void UpdateDisplayBrightness(MenuItem* menu, int flag)
{
	// control LCD brightness
	SetDisplayBrightness(*(int*)menu->value);
}

// set LCD brighntess, 0 to 100
void SetDisplayBrightness(uint b)
{
	uint val = constrain(b, 0, 100);
	int bright = map(val, 0, 100, 2500, 3300);
	m5.Axp.SetLcdVoltage(bright);	// 2500 to 3300
}

uint16_t ColorList[] = {
	TFT_WHITE,
	TFT_BLACK,
	TFT_NAVY,
	TFT_DARKGREEN,
	TFT_DARKCYAN,
	TFT_MAROON,
	TFT_PURPLE,
	TFT_OLIVE,
	TFT_LIGHTGREY,
	TFT_DARKGREY,
	TFT_BLUE,
	TFT_GREEN,
	TFT_CYAN,
	TFT_RED,
	TFT_MAGENTA,
	TFT_YELLOW,
	TFT_ORANGE,
	TFT_GREENYELLOW,
	TFT_PINK,
};

// find the color in the list
int FindMenuColor(uint16_t col)
{
	int ix;
	int colors = sizeof(ColorList) / sizeof(*ColorList);
	for (ix = 0; ix < colors; ++ix) {
		if (col == ColorList[ix])
			break;
	}
	return constrain(ix, 0, colors - 1);
}

void SetMenuColors(MenuItem* menu)
{
	int maxIndex = sizeof(ColorList) / sizeof(*ColorList) - 1;
	int mode = 0;	// 0 for active menu line, 1 for menu line
	int colorIndex = FindMenuColor(menuLineColor);
	int colorActiveIndex = FindMenuColor(menuLineActiveColor);
	tft.fillScreen(TFT_BLACK);
	DisplayLine(4, "Rotate change value");
	DisplayLine(5, "Long Press Exit");
	bool done = false;
	bool change = true;
	while (!done) {
		if (change) {
			DisplayLine(3, String("Click: ") + (mode ? "Normal" : "Active") + " Color");
			DisplayLine(0, "Active", 0, menuLineActiveColor);
			DisplayLine(1, "Normal", 0, menuLineColor);
			change = false;
		}
		switch (CRotaryDialButton::dequeue()) {
		case CRotaryDialButton::BTN_CLICK:
			if (mode == 0)
				mode = 1;
			else
				mode = 0;
			change = true;
			break;
		case CRotaryDialButton::BTN_LONGPRESS:
			done = true;
			break;
		case CRotaryDialButton::BTN_RIGHT:
			change = true;
			if (mode)
				colorIndex = ++colorIndex;
			else
				colorActiveIndex = ++colorActiveIndex;
			break;
		case CRotaryDialButton::BTN_LEFT:
			change = true;
			if (mode)
				colorIndex = --colorIndex;
			else
				colorActiveIndex = --colorActiveIndex;
			break;
		}
		colorIndex = constrain(colorIndex, 0, maxIndex);
		menuLineColor = ColorList[colorIndex];
		colorActiveIndex = constrain(colorActiveIndex, 0, maxIndex);
		menuLineActiveColor = ColorList[colorActiveIndex];
	}
}

// display the button info on the bottom
void ShowButtons(bool all)
{
	tft.fillRect(45, 231, 20, 8, WHITE);
	if (all) {
		//tft.fillRect(151, 236, 20, 3, WHITE);
		tft.fillTriangle(151, 239, 171, 239, 171, 231, WHITE);
		//tft.fillRect(257, 236, 20, 3, WHITE);
		tft.fillTriangle(257, 239, 257, 231, 277, 239, WHITE);
	}
}

// handle the menus
bool HandleMenus()
{
	if (bMenuChanged) {
		ShowMenu(MenuStack.top()->menu);
		bMenuChanged = false;
		ShowButtons(true);
	}
	bool didsomething = true;
	CRotaryDialButton::Button button = ReadButton();
	int lastOffset = MenuStack.top()->offset;
	int lastMenu = MenuStack.top()->index;
	int lastMenuCount = MenuStack.top()->menucount;
	bool lastRecording = bRecordingMacro;
	switch (button) {
	case BTN_SELECT:
		RunMenus(button);
		bMenuChanged = true;
		break;
	case BTN_RIGHT:
		if (bAllowMenuWrap || MenuStack.top()->index < MenuStack.top()->menucount - 1) {
			++MenuStack.top()->index;
		}
		if (MenuStack.top()->index >= MenuStack.top()->menucount) {
			MenuStack.top()->index = 0;
			bMenuChanged = true;
			MenuStack.top()->offset = 0;
		}
		// see if we need to scroll the menu
		if (MenuStack.top()->index - MenuStack.top()->offset > (MENU_LINES - 1)) {
			if (MenuStack.top()->offset < MenuStack.top()->menucount - MENU_LINES) {
				++MenuStack.top()->offset;
			}
		}
		break;
	case BTN_LEFT:
		if (bAllowMenuWrap || MenuStack.top()->index > 0) {
			--MenuStack.top()->index;
		}
		if (MenuStack.top()->index < 0) {
			MenuStack.top()->index = MenuStack.top()->menucount - 1;
			bMenuChanged = true;
			MenuStack.top()->offset = MenuStack.top()->menucount - MENU_LINES;
		}
		// see if we need to adjust the offset
		if (MenuStack.top()->offset && MenuStack.top()->index < MenuStack.top()->offset) {
			--MenuStack.top()->offset;
		}
		break;
	case BTN_LONG:
		tft.fillScreen(TFT_BLACK);
		bSettingsMode = false;
		DisplayCurrentFile();
		bMenuChanged = true;
		break;
	case BTN_SWIPEUP:
		if (MenuStack.top()->menucount > MENU_LINES) {
			MenuStack.top()->offset += MENU_LINES;
			MenuStack.top()->index += MENU_LINES;
			if (MenuStack.top()->offset > MenuStack.top()->menucount - MENU_LINES) {
				MenuStack.top()->offset = MenuStack.top()->menucount - MENU_LINES;
				MenuStack.top()->index = MenuStack.top()->menucount - MENU_LINES;
			}
		}
		break;
	case BTN_SWIPEDOWN:
		MenuStack.top()->offset -= MENU_LINES;
		MenuStack.top()->index -= MENU_LINES;
		if (MenuStack.top()->offset < 0) {
			MenuStack.top()->offset = 0;
			MenuStack.top()->index = 0;
		}
		break;
	default:
		if (button >= BTN_MENU) {
			int btn = button - BTN_MENU;
			//Serial.println("btn: " + String(btn));
			if (btn < MenuStack.top()->menucount) {
				MenuStack.top()->index = btn + MenuStack.top()->offset;
				CRotaryDialButton::pushButton(BTN_SELECT);
			}
		}
		else
			didsomething = false;
		break;
	}
	// check some conditions that should redraw the menu
	if (lastMenu != MenuStack.top()->index || lastOffset != MenuStack.top()->offset) {
		bMenuChanged = true;
		//Serial.println("menu changed");
	}
	// see if the recording status changed
	if (lastRecording != bRecordingMacro) {
		MenuStack.top()->index = 0;
		MenuStack.top()->offset = 0;
		bMenuChanged = true;
	}
	return didsomething;
}

// handle keys in run mode
bool HandleRunMode()
{
	bool didsomething = true;
	CRotaryDialButton::Button btn = ReadButton();
	switch (btn) {
	case BTN_SELECT:
		bCancelRun = bCancelMacro = false;
		ProcessFileOrTest();
		break;
	case BTN_RIGHT:
		if (bAllowMenuWrap || (CurrentFileIndex < FileNames.size() - 1))
			++CurrentFileIndex;
		if (CurrentFileIndex >= FileNames.size())
			CurrentFileIndex = 0;
		DisplayCurrentFile();
		break;
	case BTN_LEFT:
		if (bAllowMenuWrap || (CurrentFileIndex > 0))
			--CurrentFileIndex;
		if (CurrentFileIndex < 0)
			CurrentFileIndex = FileNames.size() - 1;
		DisplayCurrentFile();
		break;
		//case btnShowFiles:
		//	bShowBuiltInTests = !bShowBuiltInTests;
		//	GetFileNamesFromSD(currentFolder);
		//	DisplayCurrentFile();
		//	break;
	case BTN_LONG:
		tft.fillScreen(TFT_BLACK);
		bSettingsMode = true;
		break;
	case BTN_SWIPEUP:
		CurrentFileIndex += MENU_LINES - 1;
		CurrentFileIndex = constrain(CurrentFileIndex, 0, FileNames.size() - MENU_LINES);
		DisplayCurrentFile();
		break;
	case BTN_SWIPEDOWN:
		CurrentFileIndex -= MENU_LINES - 1;
		if (CurrentFileIndex < 0)
			CurrentFileIndex = 0;
		DisplayCurrentFile();
		break;
	default:
		if (btn >= BTN_MENU) {
			btn = (CRotaryDialButton::Button)(btn - BTN_MENU);
			//Serial.println("run touch: " + String(btn));
			if (btn + CurrentFileIndex < FileNames.size()) {
				CurrentFileIndex += btn;
				CRotaryDialButton::pushButton(BTN_SELECT);
			}
		}
		else {
			didsomething = false;
		}
		break;
	}
	return didsomething;
}

// check buttons and return if one pressed
enum CRotaryDialButton::Button ReadButton()
{
	m5.update();
	// check for the on board button 35
	static bool bButton35 = false;
	// check enter button, like longpress
	if (bButton35 && digitalRead(35)) {
		bButton35 = false;
	}
	if (!bButton35 && digitalRead(35) == 0) {
		// debounce
		delay(30);
		if (digitalRead(35) == 0) {
			bButton35 = true;
			CRotaryDialButton::pushButton(CRotaryDialButton::BTN_LONGPRESS);
		}
	}
	enum CRotaryDialButton::Button retValue = BTN_NONE;
	// read the next button, or NONE it none there
	retValue = CRotaryDialButton::dequeue();
	// check the M5 touch buttons
	if (m5.BtnA.event == E_LONGPRESSING) {
		retValue = BTN_LONG;
	}
	if (m5.BtnA.event == E_TAP) {
		retValue = BTN_SELECT;
	}
	if (m5.BtnB.event == E_TAP) {
		retValue = BTN_LEFT;
	}
	if (m5.BtnC.event == E_TAP) {
		retValue = BTN_RIGHT;
	}
	for (int ix = 0; ix < btnMenuArray.size(); ++ix) {
		if (btnMenuArray[ix] && btnMenuArray[ix]->event == E_TAP) {
			retValue = (CRotaryDialButton::Button)(BTN_MENU + ix);
			//Serial.println("touch: " + String(ix) + " " + String(retValue));
			break;
		}
	}
	if (m5.background.event == E_LONGPRESSING) {
		retValue = BTN_LONG;
	}
	if (swipeDown.wasDetected()) {
		retValue = BTN_SWIPEDOWN;
	}
	if (swipeUp.wasDetected()) {
		retValue = BTN_SWIPEUP;
	}
	return retValue;
}

// just check for longpress and cancel if it was there
bool CheckCancel()
{
	// if it has been set, just return true
	if (bCancelRun || bCancelMacro)
		return true;
	int button = ReadButton();
	if (button) {
		if (button == BTN_LONG) {
			bCancelMacro = bCancelRun = true;
			return true;
		}
	}
	return false;
}

void setupSDcard()
{
	bSdCardValid = false;
#if USE_STANDARD_SD
	//gpio_set_direction((gpio_num_t)SDcsPin, GPIO_MODE_OUTPUT);
	//delay(50);
	//SPIClass(1);
	//spiSDCard.begin(SDSckPin, SDMisoPin, SDMosiPin, SDcsPin);	// SCK,MISO,MOSI,CS
	//delay(20);

	//if (!SD.begin(SDcsPin, spiSDCard)) {
	//	//Serial.println("Card Mount Failed");
	//	return;
	//}
	uint8_t cardType = SD.cardType();

	if (cardType == CARD_NONE) {
		//Serial.println("No SD card attached");
		return;
	}
#else
#define SD_CONFIG SdSpiConfig(SDcsPin, /*DEDICATED_SPI*/SHARED_SPI, SD_SCK_MHZ(10))
	SPI.begin(SDSckPin, SDMisoPin, SDMosiPin, SDcsPin);	// SCK,MISO,MOSI,CS
	if (!SD.begin(SD_CONFIG)) {
		Serial.println("SD initialization failed.");
		uint8_t err = SD.card()->errorCode();
		Serial.println("err: " + String(err));
		return;
	}
	//Serial.println("Mounted SD card");
	//SD.printFatType(&Serial);

	//uint64_t cardSize = (uint64_t)SD.clusterCount() * SD.bytesPerCluster() / (1024 * 1024 * 1024);
	//Serial.printf("SD Card Size: %llu GB\n", cardSize);
#endif
	bSdCardValid = GetFileNamesFromSD(currentFolder);
}

// return the pixel
CRGB IRAM_ATTR getRGBwithGamma() {
	if (bGammaCorrection) {
		b = gammaB[readByte(false)];
		g = gammaG[readByte(false)];
		r = gammaR[readByte(false)];
	}
	else {
		b = readByte(false);
		g = readByte(false);
		r = readByte(false);
	}
	return CRGB(r, g, b);
}

void fixRGBwithGamma(byte* rp, byte* gp, byte* bp) {
	if (bGammaCorrection) {
		*gp = gammaG[*gp];
		*bp = gammaB[*bp];
		*rp = gammaR[*rp];
	}
}

// up to 32 bouncing balls
void TestBouncingBalls() {
	CRGB colors[] = {
		CRGB::White,
		CRGB::Red,
		CRGB::Green,
		CRGB::Blue,
		CRGB::Yellow,
		CRGB::Cyan,
		CRGB::Magenta,
		CRGB::Grey,
		CRGB::RosyBrown,
		CRGB::RoyalBlue,
		CRGB::SaddleBrown,
		CRGB::Salmon,
		CRGB::SandyBrown,
		CRGB::SeaGreen,
		CRGB::Seashell,
		CRGB::Sienna,
		CRGB::Silver,
		CRGB::SkyBlue,
		CRGB::SlateBlue,
		CRGB::SlateGray,
		CRGB::SlateGrey,
		CRGB::Snow,
		CRGB::SpringGreen,
		CRGB::SteelBlue,
		CRGB::Tan,
		CRGB::Teal,
		CRGB::Thistle,
		CRGB::Tomato,
		CRGB::Turquoise,
		CRGB::Violet,
		CRGB::Wheat,
		CRGB::WhiteSmoke,
	};

	BouncingColoredBalls(nBouncingBallsCount, colors);
	FastLED.clear(true);
}

void BouncingColoredBalls(int balls, CRGB colors[]) {
	time_t startsec = time(NULL);
	float Gravity = -9.81;
	int StartHeight = 1;

	float* Height = (float*)calloc(balls, sizeof(float));
	float* ImpactVelocity = (float*)calloc(balls, sizeof(float));
	float* TimeSinceLastBounce = (float*)calloc(balls, sizeof(float));
	int* Position = (int*)calloc(balls, sizeof(int));
	long* ClockTimeSinceLastBounce = (long*)calloc(balls, sizeof(long));
	float* Dampening = (float*)calloc(balls, sizeof(float));
	float ImpactVelocityStart = sqrt(-2 * Gravity * StartHeight);

	for (int i = 0; i < balls; i++) {
		ClockTimeSinceLastBounce[i] = millis();
		Height[i] = StartHeight;
		Position[i] = 0;
		ImpactVelocity[i] = ImpactVelocityStart;
		TimeSinceLastBounce[i] = 0;
		Dampening[i] = 0.90 - float(i) / pow(balls, 2);
	}

	long percent;
	int colorChangeCounter = 0;
	bool done = false;
	while (!done) {
		if (CheckCancel()) {
			done = true;
			break;
		}
		for (int i = 0; i < balls; i++) {
			if (CheckCancel()) {
				done = true;
				break;
			}
			TimeSinceLastBounce[i] = millis() - ClockTimeSinceLastBounce[i];
			Height[i] = 0.5 * Gravity * pow(TimeSinceLastBounce[i] / nBouncingBallsDecay, 2.0) + ImpactVelocity[i] * TimeSinceLastBounce[i] / nBouncingBallsDecay;

			if (Height[i] < 0) {
				Height[i] = 0;
				ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
				ClockTimeSinceLastBounce[i] = millis();

				if (ImpactVelocity[i] < 0.01) {
					ImpactVelocity[i] = ImpactVelocityStart;
				}
			}
			Position[i] = round(Height[i] * (STRIPLENGTH - 1) / StartHeight);
		}

		for (int i = 0; i < balls; i++) {
			int ix;
			if (CheckCancel()) {
				done = true;
				break;
			}
			ix = (i + nBouncingBallsFirstColor) % 32;
			SetPixel(Position[i], colors[ix]);
		}
		if (nBouncingBallsChangeColors && colorChangeCounter++ > (nBouncingBallsChangeColors * 100)) {
			++nBouncingBallsFirstColor;
			colorChangeCounter = 0;
		}
		FastLED.show();
		delayMicroseconds(50);
		FastLED.clear();
	}
	free(Height);
	free(ImpactVelocity);
	free(TimeSinceLastBounce);
	free(Position);
	free(ClockTimeSinceLastBounce);
	free(Dampening);
}

#define BARBERSIZE 10
#define BARBERCOUNT 40
void BarberPole()
{
CRGB:CRGB red, white, blue;
	byte r, g, b;
	r = 255, g = 0, b = 0;
	fixRGBwithGamma(&r, &g, &b);
	red = CRGB(r, g, b);
	r = 255, g = 255, b = 255;
	fixRGBwithGamma(&r, &g, &b);
	white = CRGB(r, g, b);
	r = 0, g = 0, b = 255;
	fixRGBwithGamma(&r, &g, &b);
	blue = CRGB(r, g, b);
	bool done = false;
	for (int loop = 0; !done; ++loop) {
		if (CheckCancel()) {
			done = true;
			break;
		}
		for (int ledIx = 0; ledIx < STRIPLENGTH; ++ledIx) {
			if (CheckCancel()) {
				done = true;
				break;
			}
			// figure out what color
			switch (((ledIx + loop) % BARBERCOUNT) / BARBERSIZE) {
			case 0: // red
				SetPixel(ledIx, red);
				break;
			case 1: // white
			case 3:
				SetPixel(ledIx, white);
				break;
			case 2: // blue
				SetPixel(ledIx, blue);
				break;
			}
		}
		FastLED.show();
		delay(nFrameHold);
	}
}

// checkerboard
void CheckerBoard()
{
	int width = nCheckboardBlackWidth + nCheckboardWhiteWidth;
	int times = 0;
	CRGB color1 = CRGB::Black, color2 = CRGB::White;
	int addPixels = 0;
	bool done = false;
	while (!done) {
		for (int y = 0; y < STRIPLENGTH; ++y) {
			SetPixel(y, ((y + addPixels) % width) < nCheckboardBlackWidth ? color1 : color2);
		}
		FastLED.show();
		int count = nCheckerboardHoldframes;
		while (count-- > 0) {
			delay(nFrameHold);
			if (CheckCancel()) {
				done = true;
				break;
			}
		}
		if (bCheckerBoardAlternate && (times++ % 2)) {
			// swap colors
			CRGB temp = color1;
			color1 = color2;
			color2 = temp;
		}
		addPixels += nCheckerboardAddPixels;
		if (CheckCancel()) {
			done = true;
			break;
		}
	}
}

void RandomBars()
{
	ShowRandomBars(bRandomBarsBlacks);
}

// show random bars of lights with optional blacks between
void ShowRandomBars(bool blacks)
{
	time_t start = time(NULL);
	byte r, g, b;
	srand(millis());
	char line[40];
	bool done = false;
	for (int pass = 0; !done; ++pass) {
		if (blacks && (pass % 2)) {
			// odd numbers, clear
			FastLED.clear(true);
		}
		else {
			// even numbers, show bar
			r = random(0, 255);
			g = random(0, 255);
			b = random(0, 255);
			fixRGBwithGamma(&r, &g, &b);
			// fill the strip color
			FastLED.showColor(CRGB(r, g, b));
		}
		int count = nRandomBarsHoldframes;
		while (count-- > 0) {
			delay(nFrameHold);
			if (CheckCancel()) {
				done = true;
				break;
			}
		}
	}
}

// running dot
void RunningDot()
{
	for (int colorvalue = 0; colorvalue <= 3; ++colorvalue) {
		// RGBW
		byte r, g, b;
		switch (colorvalue) {
		case 0: // red
			r = 255;
			g = 0;
			b = 0;
			break;
		case 1: // green
			r = 0;
			g = 255;
			b = 0;
			break;
		case 2: // blue
			r = 0;
			g = 0;
			b = 255;
			break;
		case 3: // white
			r = 255;
			g = 255;
			b = 255;
			break;
		}
		fixRGBwithGamma(&r, &g, &b);
		char line[10];
		for (int ix = 0; ix < STRIPLENGTH; ++ix) {
			if (CheckCancel()) {
				break;
			}
			if (ix > 0) {
				SetPixel(ix - 1, CRGB::Black);
			}
			SetPixel(ix, CRGB(r, g, b));
			FastLED.show();
			delay(nFrameHold);
		}
		// remember the last one, turn it off
		SetPixel(STRIPLENGTH - 1, CRGB::Black);
		FastLED.show();
	}
	FastLED.clear(true);
}

void OppositeRunningDots()
{
	for (int mode = 0; mode <= 3; ++mode) {
		if (CheckCancel())
			break;;
		// RGBW
		byte r, g, b;
		switch (mode) {
		case 0: // red
			r = 255;
			g = 0;
			b = 0;
			break;
		case 1: // green
			r = 0;
			g = 255;
			b = 0;
			break;
		case 2: // blue
			r = 0;
			g = 0;
			b = 255;
			break;
		case 3: // white
			r = 255;
			g = 255;
			b = 255;
			break;
		}
		fixRGBwithGamma(&r, &g, &b);
		for (int ix = 0; ix < STRIPLENGTH; ++ix) {
			if (CheckCancel())
				return;
			if (ix > 0) {
				SetPixel(ix - 1, CRGB::Black);
				SetPixel(STRIPLENGTH - ix + 1, CRGB::Black);
			}
			SetPixel(STRIPLENGTH - ix, CRGB(r, g, b));
			SetPixel(ix, CRGB(r, g, b));
			FastLED.show();
			delay(nFrameHold);
		}
	}
}

void Sleep(MenuItem* menu)
{
	++nBootCount;
	//rtc_gpio_pullup_en(BTNPUSH);
	esp_sleep_enable_ext0_wakeup((gpio_num_t)DIAL_BTN, LOW);
	esp_deep_sleep_start();
}

void LightBar(MenuItem* menu)
{
	tft.fillScreen(TFT_BLACK);
	DisplayLine(0, "LED Light Bar");
	DisplayLine(3, "Rotate Dial to Change");
	DisplayLine(4, "Click to Set Operation");
	DisplayLedLightBar();
	FastLED.clear(true);
	// these were set by CheckCancel() in DisplayAllColor() and need to be cleared
	bCancelMacro = bCancelRun = false;
}

// utility for DisplayLedLightBar()
void FillLightBar()
{
	int offset = bDisplayAllFromMiddle ? (144 - nDisplayAllPixelCount) / 2 : 0;
	if (!bDisplayAllFromMiddle && bUpsideDown)
		offset = 144 - nDisplayAllPixelCount;
	FastLED.clear();
	if (bDisplayAllRGB)
		fill_solid(leds + offset, nDisplayAllPixelCount, CRGB(nDisplayAllRed, nDisplayAllGreen, nDisplayAllBlue));
	else
		fill_solid(leds + offset, nDisplayAllPixelCount, CHSV(nDisplayAllHue, nDisplayAllSaturation, nDisplayAllBrightness));
	FastLED.show();
}

// Used LEDs as a light bar
void DisplayLedLightBar()
{
	DisplayLine(1, "");
	FillLightBar();
	// show until cancelled, but check for rotations of the knob
	CRotaryDialButton::Button btn;
	int what = 0;	// 0 for hue, 1 for saturation, 2 for brightness, 3 for pixels, 4 for increment
	int increment = 10;
	bool bChange = true;
	while (true) {
		if (bChange) {
			String line;
			switch (what) {
			case 0:
				if (bDisplayAllRGB)
					line = "Red: " + String(nDisplayAllRed);
				else
					line = "HUE: " + String(nDisplayAllHue);
				break;
			case 1:
				if (bDisplayAllRGB)
					line = "Green: " + String(nDisplayAllGreen);
				else
					line = "Saturation: " + String(nDisplayAllSaturation);
				break;
			case 2:
				if (bDisplayAllRGB)
					line = "Blue: " + String(nDisplayAllBlue);
				else
					line = "Brightness: " + String(nDisplayAllBrightness);
				break;
			case 3:
				line = "Pixels: " + String(nDisplayAllPixelCount);
				break;
			case 4:
				line = "From: " + String((bDisplayAllFromMiddle ? "Middle" : "End"));
				break;
			case 5:
				line = " (step size: " + String(increment) + ")";
				break;
			}
			DisplayLine(2, line);
		}
		btn = ReadButton();
		bChange = true;
		switch (btn) {
		case BTN_NONE:
			bChange = false;
			break;
		case BTN_RIGHT:
			switch (what) {
			case 0:
				if (bDisplayAllRGB)
					nDisplayAllRed += increment;
				else
					nDisplayAllHue += increment;
				break;
			case 1:
				if (bDisplayAllRGB)
					nDisplayAllGreen += increment;
				else
					nDisplayAllSaturation += increment;
				break;
			case 2:
				if (bDisplayAllRGB)
					nDisplayAllBlue += increment;
				else
					nDisplayAllBrightness += increment;
				break;
			case 3:
				nDisplayAllPixelCount += increment;
				break;
			case 4:
				bDisplayAllFromMiddle = true;
				break;
			case 5:
				increment *= 10;
				break;
			}
			break;
		case BTN_LEFT:
			switch (what) {
			case 0:
				if (bDisplayAllRGB)
					nDisplayAllRed -= increment;
				else
					nDisplayAllHue -= increment;
				break;
			case 1:
				if (bDisplayAllRGB)
					nDisplayAllGreen -= increment;
				else
					nDisplayAllSaturation -= increment;
				break;
			case 2:
				if (bDisplayAllRGB)
					nDisplayAllBlue -= increment;
				else
					nDisplayAllBrightness -= increment;
				break;
			case 3:
				nDisplayAllPixelCount -= increment;
				break;
			case 4:
				bDisplayAllFromMiddle = false;
				break;
			case 5:
				increment /= 10;
				break;
			}
			break;
		case BTN_SELECT:
			// switch to the next selection, wrapping around if necessary
			what = ++what % 6;
			break;
		case BTN_LONG:
			// put it back, we don't want it
			CRotaryDialButton::pushButton(btn);
			break;
		}
		if (CheckCancel())
			return;
		if (bChange) {
			nDisplayAllPixelCount = constrain(nDisplayAllPixelCount, 1, 144);
			increment = constrain(increment, 1, 100);
			if (bDisplayAllRGB) {
				if (bAllowRollover) {
					if (nDisplayAllRed < 0)
						nDisplayAllRed = RollDownRollOver(increment);
					if (nDisplayAllRed > 255)
						nDisplayAllRed = 0;
					if (nDisplayAllGreen < 0)
						nDisplayAllGreen = RollDownRollOver(increment);
					if (nDisplayAllGreen > 255)
						nDisplayAllGreen = 0;
					if (nDisplayAllBlue < 0)
						nDisplayAllBlue = RollDownRollOver(increment);
					if (nDisplayAllBlue > 255)
						nDisplayAllBlue = 0;
				}
				else {
					nDisplayAllRed = constrain(nDisplayAllRed, 0, 255);
					nDisplayAllGreen = constrain(nDisplayAllGreen, 0, 255);
					nDisplayAllBlue = constrain(nDisplayAllBlue, 0, 255);
				}
				FillLightBar();
			}
			else {
				if (bAllowRollover) {
					if (nDisplayAllHue < 0)
						nDisplayAllHue = RollDownRollOver(increment);
					if (nDisplayAllHue > 255)
						nDisplayAllHue = 0;
					if (nDisplayAllSaturation < 0)
						nDisplayAllSaturation = RollDownRollOver(increment);
					if (nDisplayAllSaturation > 255)
						nDisplayAllSaturation = 0;
				}
				else {
					nDisplayAllHue = constrain(nDisplayAllHue, 0, 255);
					nDisplayAllSaturation = constrain(nDisplayAllSaturation, 0, 255);
				}
				nDisplayAllBrightness = constrain(nDisplayAllBrightness, 0, 255);
				FillLightBar();
			}
		}
		delay(10);
	}
}

// handle rollover when -ve
// inc 1 gives 255, inc 10 gives 250, inc 100 gives 200
int RollDownRollOver(int inc)
{
	if (inc == 1)
		return 255;
	int retval = 256;
	retval -= retval % inc;
	return retval;
}

void TestTwinkle() {
	TwinkleRandom(nFrameHold, bTwinkleOnlyOne);
}
void TwinkleRandom(int SpeedDelay, boolean OnlyOne) {
	time_t start = time(NULL);
	bool done = false;
	while (!done) {
		SetPixel(random(STRIPLENGTH), CRGB(random(0, 255), random(0, 255), random(0, 255)));
		FastLED.show();
		delay(SpeedDelay);
		if (OnlyOne) {
			FastLED.clear(true);
		}
		if (CheckCancel()) {
			done = true;
			break;
		}
	}
}

void TestCylon()
{
	CylonBounce(nCylonEyeRed, nCylonEyeGreen, nCylonEyeBlue, nCylonEyeSize, nFrameHold, 50);
}
void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay)
{
	for (int i = 0; i < STRIPLENGTH - EyeSize - 2; i++) {
		if (CheckCancel()) {
			break;
		}
		FastLED.clear();
		SetPixel(i, CRGB(red / 10, green / 10, blue / 10));
		for (int j = 1; j <= EyeSize; j++) {
			SetPixel(i + j, CRGB(red, green, blue));
		}
		SetPixel(i + EyeSize + 1, CRGB(red / 10, green / 10, blue / 10));
		FastLED.show();
		delay(SpeedDelay);
	}
	delay(ReturnDelay);
	for (int i = STRIPLENGTH - EyeSize - 2; i > 0; i--) {
		if (CheckCancel()) {
			break;
		}
		FastLED.clear();
		SetPixel(i, CRGB(red / 10, green / 10, blue / 10));
		for (int j = 1; j <= EyeSize; j++) {
			SetPixel(i + j, CRGB(red, green, blue));
		}
		SetPixel(i + EyeSize + 1, CRGB(red / 10, green / 10, blue / 10));
		FastLED.show();
		delay(SpeedDelay);
	}
	FastLED.clear(true);
}

void TestMeteor() {
	meteorRain(nMeteorRed, nMeteorGreen, nMeteorBlue, nMeteorSize, 64, true, 30);
}

void meteorRain(byte red, byte green, byte blue, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay)
{
	FastLED.clear(true);

	for (int i = 0; i < STRIPLENGTH + STRIPLENGTH; i++) {
		if (CheckCancel())
			break;;
		// fade brightness all LEDs one step
		for (int j = 0; j < STRIPLENGTH; j++) {
			if (CheckCancel())
				break;
			if ((!meteorRandomDecay) || (random(10) > 5)) {
				fadeToBlack(j, meteorTrailDecay);
			}
		}
		// draw meteor
		for (int j = 0; j < meteorSize; j++) {
			if (CheckCancel())
				break;
			if ((i - j < STRIPLENGTH) && (i - j >= 0)) {
				SetPixel(i - j, CRGB(red, green, blue));
			}
		}
		FastLED.show();
		delay(SpeedDelay);
	}
}

void TestConfetti()
{
	time_t start = time(NULL);
	gHue = 0;
	bool done = false;
	while (!done) {
		EVERY_N_MILLISECONDS(nFrameHold) {
			if (bConfettiCycleHue)
				++gHue;
			confetti();
			FastLED.show();
		}
		if (CheckCancel()) {
			done = true;
			break;
		}
	}
	// wait for timeout so strip will be blank
	delay(100);
}

void confetti()
{
	// random colored speckles that blink in and fade smoothly
	fadeToBlackBy(leds, STRIPLENGTH, 10);
	int pos = random16(STRIPLENGTH);
	leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void TestJuggle()
{
	bool done = false;
	while (!done) {
		EVERY_N_MILLISECONDS(nFrameHold) {
			juggle();
			FastLED.show();
		}
		if (CheckCancel()) {
			done = true;
			break;
		}
	}
}

void juggle()
{
	// eight colored dots, weaving in and out of sync with each other
	fadeToBlackBy(leds, STRIPLENGTH, 20);
	byte dothue = 0;
	uint16_t index;
	for (int i = 0; i < 8; i++) {
		index = beatsin16(i + 7, 0, STRIPLENGTH);
		// use AdjustStripIndex to get the right one
		SetPixel(index, leds[AdjustStripIndex(index)] | CHSV(dothue, 255, 255));
		//leds[beatsin16(i + 7, 0, STRIPLENGTH)] |= CHSV(dothue, 255, 255);
		dothue += 32;
	}
}

void TestSine()
{
	gHue = nSineStartingHue;
	bool done = false;
	while (!done) {
		EVERY_N_MILLISECONDS(nFrameHold) {
			sinelon();
			FastLED.show();
		}
		if (CheckCancel()) {
			done = true;
			break;
		}
	}
}
void sinelon()
{
	// a colored dot sweeping back and forth, with fading trails
	fadeToBlackBy(leds, STRIPLENGTH, 20);
	int pos = beatsin16(nSineSpeed, 0, STRIPLENGTH);
	leds[AdjustStripIndex(pos)] += CHSV(gHue, 255, 192);
	if (bSineCycleHue)
		++gHue;
}

void TestBpm()
{
	gHue = 0;
	bool done = false;
	while (!done) {
		EVERY_N_MILLISECONDS(nFrameHold) {
			bpm();
			FastLED.show();
		}
		if (CheckCancel()) {
			done = true;
			break;
		}
	}
}

void bpm()
{
	// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
	CRGBPalette16 palette = PartyColors_p;
	uint8_t beat = beatsin8(nBpmBeatsPerMinute, 64, 255);
	for (int i = 0; i < STRIPLENGTH; i++) { //9948
		SetPixel(i, ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10)));
	}
	if (bBpmCycleHue)
		++gHue;
}

void FillRainbow(struct CRGB* pFirstLED, int numToFill,
	uint8_t initialhue,
	int deltahue)
{
	CHSV hsv;
	hsv.hue = initialhue;
	hsv.val = 255;
	hsv.sat = 240;
	for (int i = 0; i < numToFill; i++) {
		pFirstLED[AdjustStripIndex(i)] = hsv;
		hsv.hue += deltahue;
	}
}

void TestRainbow()
{
	gHue = nRainbowInitialHue;
	FillRainbow(leds, STRIPLENGTH, gHue, nRainbowHueDelta);
	FadeInOut(nRainbowFadeTime * 100, true);
	bool done = false;
	while (!done) {
		EVERY_N_MILLISECONDS(nFrameHold) {
			if (bRainbowCycleHue)
				++gHue;
			FillRainbow(leds, STRIPLENGTH, gHue, nRainbowHueDelta);
			if (bRainbowAddGlitter)
				addGlitter(80);
			FastLED.show();
		}
		if (CheckCancel()) {
			done = true;
			FastLED.setBrightness(nStripBrightness);
			break;
		}
	}
	FadeInOut(nRainbowFadeTime * 100, false);
	FastLED.setBrightness(nStripBrightness);
}

// create a user defined stripe set
// it consists of a list of stripes, each of which have a width and color
// there can be up to 10 of these
#define NUM_STRIPES 20
struct {
	int start;
	int length;
	CHSV color;
} Stripes[NUM_STRIPES];

void TestStripes()
{
	FastLED.setBrightness(nStripBrightness);
	// let's fill in some data
	for (int ix = 0; ix < NUM_STRIPES; ++ix) {
		Stripes[ix].start = ix * 20;
		Stripes[ix].length = 12;
		Stripes[ix].color = CHSV(0, 0, 255);
	}
	int pix = 0;	// pixel address
	FastLED.clear(true);
	for (int ix = 0; ix < NUM_STRIPES; ++ix) {
		pix = Stripes[ix].start;
		// fill in each block of pixels
		for (int len = 0; len < Stripes[ix].length; ++len) {
			SetPixel(pix++, CRGB(Stripes[ix].color));
		}
	}
	FastLED.show();
	bool done = false;
	while (!done) {
		if (CheckCancel()) {
			done = true;
			break;
		}
		delay(1000);
	}
}

// alternating white and black lines
void TestLines()
{
	FastLED.setBrightness(nStripBrightness);
	FastLED.clear(true);
	bool bWhite = true;
	for (int pix = 0; pix < STRIPLENGTH; ++pix) {
		// fill in each block of pixels
		for (int len = 0; len < (bWhite ? nLinesWhite : nLinesBlack); ++len) {
			SetPixel(pix++, bWhite ? CRGB::White : CRGB::Black);
		}
		bWhite = !bWhite;
	}
	FastLED.show();
	bool done = false;
	while (!done) {
		if (CheckCancel()) {
			done = true;
			break;
		}
		delay(1000);
		// might make this work to toggle blacks and whites eventually
		//for (int ix = 0; ix < STRIPLENGTH; ++ix) {
		//	leds[ix] = (leds[ix] == CRGB::White) ? CRGB::Black : CRGB::White;
		//}
		FastLED.show();
	}
	FastLED.clear(true);
}

// time is in mSec
void FadeInOut(int time, bool in)
{
	if (in) {
		for (int i = 0; i <= nStripBrightness; ++i) {
			FastLED.setBrightness(i);
			FastLED.show();
			delay(time / nStripBrightness);
		}
	}
	else {
		for (int i = nStripBrightness; i >= 0; --i) {
			FastLED.setBrightness(i);
			FastLED.show();
			delay(time / nStripBrightness);
		}
	}
}

void addGlitter(fract8 chanceOfGlitter)
{
	if (random8() < chanceOfGlitter) {
		leds[random16(STRIPLENGTH)] += CRGB::White;
	}
}

void fadeToBlack(int ledNo, byte fadeValue) {
	// FastLED
	leds[ledNo].fadeToBlackBy(fadeValue);
}

// run file or built-in
void ProcessFileOrTest()
{
	String line;
	// let's see if this is a folder command
	String tmp = FileNames[CurrentFileIndex];
	if (tmp[0] == NEXT_FOLDER_CHAR) {
		FileIndexStack.push(CurrentFileIndex);
		tmp = tmp.substring(1);
		// change folder, reload files
		currentFolder += tmp + "/";
		GetFileNamesFromSD(currentFolder);
		DisplayCurrentFile();
		return;
	}
	else if (tmp[0] == PREVIOUS_FOLDER_CHAR) {
		tmp = tmp.substring(1);
		tmp = tmp.substring(0, tmp.length() - 1);
		if (tmp.length() == 0)
			tmp = "/";
		// change folder, reload files
		currentFolder = tmp;
		GetFileNamesFromSD(currentFolder);
		CurrentFileIndex = FileIndexStack.top();
		FileIndexStack.pop();
		DisplayCurrentFile();
		return;
	}
	if (bRecordingMacro) {
		strcpy(FileToShow, FileNames[CurrentFileIndex].c_str());
		WriteOrDeleteConfigFile(String(nCurrentMacro), false, false);
	}
	bIsRunning = true;
	nProgress = 0;
	// clear the rest of the lines
	for (int ix = 1; ix < MENU_LINES; ++ix)
		DisplayLine(ix, "");
	//DisplayCurrentFile();
	if (startDelay) {
		// set a timer
		nTimerSeconds = startDelay;
		while (nTimerSeconds && !CheckCancel()) {
			line = "Start Delay: " + String(nTimerSeconds / 10) + "." + String(nTimerSeconds % 10);
			DisplayLine(1, line);
			delay(100);
			--nTimerSeconds;
		}
		DisplayLine(2, "");
	}
	int chainCount = bChainFiles ? FileCountOnly(CurrentFileIndex) : 1;
	int chainRepeatCount = bChainFiles ? nChainRepeats : 1;
	int lastFileIndex = CurrentFileIndex;
	// don't allow chaining for built-ins, although maybe we should
	if (bShowBuiltInTests) {
		chainCount = 1;
		chainRepeatCount = 1;
	}
	// set the basic LED info
	FastLED.setTemperature(CRGB(whiteBalance.r, whiteBalance.g, whiteBalance.b));
	FastLED.setBrightness(nStripBrightness);
	line = "";
	while (chainRepeatCount-- > 0) {
		while (chainCount-- > 0) {
			DisplayCurrentFile();
			if (bChainFiles && !bShowBuiltInTests) {
				line = "Files: " + String(chainCount + 1);
				DisplayLine(3, line);
				line = "";
			}
			// process the repeats and waits for each file in the list
			for (nRepeatsLeft = repeatCount; nRepeatsLeft > 0; nRepeatsLeft--) {
				// fill the progress bar
				if (!bShowBuiltInTests)
					ShowProgressBar(0);
				if (repeatCount > 1) {
					line = "Repeats: " + String(nRepeatsLeft) + " ";
				}
				if (!bShowBuiltInTests && nChainRepeats > 1) {
					line += "Chains: " + String(chainRepeatCount + 1);
				}
				DisplayLine(2, line);
				if (bShowBuiltInTests) {
					DisplayLine(3, "Running (long cancel)");
					// run the test
					(*BuiltInFiles[CurrentFileIndex].function)();
				}
				else {
					if (nRepeatCountMacro > 1 && bRunningMacro) {
						DisplayLine(3, String("Macro Repeats: ") + String(nMacroRepeatsLeft));
					}
					// output the file
					SendFile(FileNames[CurrentFileIndex]);
				}
				if (bCancelRun) {
					break;
				}
				if (!bShowBuiltInTests)
					ShowProgressBar(0);
				if (nRepeatsLeft > 1) {
					if (repeatDelay) {
						FastLED.clear(true);
						// start timer
						nTimerSeconds = repeatDelay;
						while (nTimerSeconds > 0 && !CheckCancel()) {
							line = "Repeat Delay: " + String(nTimerSeconds / 10) + "." + String(nTimerSeconds % 10);
							DisplayLine(1, line);
							line = "";
							delay(100);
							--nTimerSeconds;
						}
						//DisplayLine(2, "");
					}
				}
			}
			if (bCancelRun) {
				chainCount = 0;
				break;
			}
			if (bShowBuiltInTests)
				break;
			// see if we are chaining, if so, get the next file, if a folder we're done
			if (bChainFiles) {
				// grab the next file
				if (CurrentFileIndex < FileNames.size() - 1)
					++CurrentFileIndex;
				if (IsFolder(CurrentFileIndex))
					break;
				// handle any chain delay
				for (int dly = nChainDelay; dly > 0 && !CheckCancel(); --dly) {
					line = "Chain Delay: " + String(dly / 10) + "." + String(dly % 10);
					DisplayLine(1, line);
					delay(100);
				}
			}
			line = "";
			// clear
			FastLED.clear(true);
		}
		if (bCancelRun) {
			chainCount = 0;
			chainRepeatCount = 0;
			bCancelRun = false;
			break;
		}
		// start again
		CurrentFileIndex = lastFileIndex;
		chainCount = bChainFiles ? FileCountOnly(CurrentFileIndex) : 1;
		if (repeatDelay && (nRepeatsLeft > 1) || chainRepeatCount >= 1) {
			FastLED.clear(true);
			// start timer
			nTimerSeconds = repeatDelay;
			while (nTimerSeconds > 0 && !CheckCancel()) {
				line = "Repeat Delay: " + String(nTimerSeconds / 10) + "." + String(nTimerSeconds % 10);
				DisplayLine(1, line);
				line = "";
				delay(100);
				--nTimerSeconds;
			}
		}
	}
	if (bChainFiles)
		CurrentFileIndex = lastFileIndex;
	FastLED.clear(true);
	tft.fillScreen(TFT_BLACK);
	bIsRunning = false;
	if (!bRunningMacro)
		DisplayCurrentFile();
	nProgress = 0;
	// clear buttons
	CRotaryDialButton::clear();
}

void SendFile(String Filename) {
	// see if there is an associated config file
	String cfFile = MakeMIWFilename(Filename, true);
	SettingsSaveRestore(true, 0);
	ProcessConfigFile(cfFile);
	String fn = currentFolder + Filename;
	dataFile = SD.open(fn);
	// if the file is available send it to the LED's
	if (dataFile.available()) {
		for (int cnt = 0; cnt < (bMirrorPlayImage ? 2 : 1); ++cnt) {
			ReadAndDisplayFile(cnt == 0);
			bReverseImage = !bReverseImage; // note this will be restored by SettingsSaveRestore
			dataFile.seek(0);
			FastLED.clear(true);
			int wait = nMirrorDelay;
			while (wait-- > 0) {
				delay(100);
			}
			if (CheckCancel())
				break;
		}
		dataFile.close();
	}
	else {
		WriteMessage("open fail: " + fn, true, 5000);
		return;
	}
	ShowProgressBar(100);
	SettingsSaveRestore(false, 0);
}

// some useful BMP constants
#define MYBMP_BF_TYPE           0x4D42	// "BM"
#define MYBMP_BI_RGB            0L
//#define MYBMP_BI_RLE8           1L
//#define MYBMP_BI_RLE4           2L
//#define MYBMP_BI_BITFIELDS      3L

void IRAM_ATTR ReadAndDisplayFile(bool doingFirstHalf) {
	static int totalSeconds;
	if (doingFirstHalf)
		totalSeconds = -1;

	// clear the file cache buffer
	readByte(true);
	uint16_t bmpType = readInt();
	uint32_t bmpSize = readLong();
	uint16_t bmpReserved1 = readInt();
	uint16_t bmpReserved2 = readInt();
	uint32_t bmpOffBits = readLong();
	//Serial.println("\nBMPtype: " + String(bmpType) + " offset: " + String(bmpOffBits));

	/* Check file header */
	if (bmpType != MYBMP_BF_TYPE) {
		WriteMessage(String("Invalid BMP:\n") + currentFolder + FileNames[CurrentFileIndex], true);
		return;
	}

	/* Read info header */
	uint32_t imgSize = readLong();
	uint32_t imgWidth = readLong();
	uint32_t imgHeight = readLong();
	uint16_t imgPlanes = readInt();
	uint16_t imgBitCount = readInt();
	uint32_t imgCompression = readLong();
	uint32_t imgSizeImage = readLong();
	uint32_t imgXPelsPerMeter = readLong();
	uint32_t imgYPelsPerMeter = readLong();
	uint32_t imgClrUsed = readLong();
	uint32_t imgClrImportant = readLong();

	//Serial.println("imgSize: " + String(imgSize));
	//Serial.println("imgWidth: " + String(imgWidth));
	//Serial.println("imgHeight: " + String(imgHeight));
	//Serial.println("imgPlanes: " + String(imgPlanes));
	//Serial.println("imgBitCount: " + String(imgBitCount));
	//Serial.println("imgCompression: " + String(imgCompression));
	//Serial.println("imgSizeImage: " + String(imgSizeImage));
	/* Check info header */
	if (imgWidth <= 0 || imgHeight <= 0 || imgPlanes != 1 ||
		imgBitCount != 24 || imgCompression != MYBMP_BI_RGB || imgSizeImage == 0)
	{
		WriteMessage(String("Unsupported, must be 24bpp:\n") + currentFolder + FileNames[CurrentFileIndex], true);
		return;
	}

	int displayWidth = imgWidth;
	if (imgWidth > STRIPLENGTH) {
		displayWidth = STRIPLENGTH;           //only display the number of led's we have
	}

	/* compute the line length */
	uint32_t lineLength = imgWidth * 3;
	// fix for padding to 4 byte words
	if ((lineLength % 4) != 0)
		lineLength = (lineLength / 4 + 1) * 4;

	// Note:  
	// The x,r,b,g sequence below might need to be changed if your strip is displaying
	// incorrect colors.  Some strips use an x,r,b,g sequence and some use x,r,g,b
	// Change the order if needed to make the colors correct.
	// init the fade settings in SetPixel
	SetPixel(0, TFT_BLACK, -1, (int)imgHeight);
	long secondsLeft = 0, lastSeconds = 0;
	char num[50];
	int percent;
	unsigned minLoopTime = 0; // the minimum time it takes to process a line
	bool bLoopTimed = false;
	// also remember that height and width are effectively swapped since we rotated the BMP image CCW for ease of reading and displaying here
	for (int y = bReverseImage ? imgHeight - 1 : 0; bReverseImage ? y >= 0 : y < imgHeight; bReverseImage ? --y : ++y) {
		// approximate time left
		if (bReverseImage)
			secondsLeft = ((long)y * (nFrameHold + minLoopTime) / 1000L) + 1;
		else
			secondsLeft = ((long)(imgHeight - y) * (nFrameHold + minLoopTime) / 1000L) + 1;
		// mark the time for timing the loop
		if (!bLoopTimed) {
			minLoopTime = millis();
		}
		if (bMirrorPlayImage) {
			if (totalSeconds == -1)
				totalSeconds = secondsLeft;
			if (doingFirstHalf) {
				secondsLeft += totalSeconds;
			}
		}
		if (secondsLeft != lastSeconds) {
			lastSeconds = secondsLeft;
			sprintf(num, "File Seconds: %d", secondsLeft);
			DisplayLine(1, num);
		}
		percent = map(bReverseImage ? imgHeight - y : y, 0, imgHeight, 0, 100);
		if (bMirrorPlayImage) {
			percent /= 2;
			if (!doingFirstHalf) {
				percent += 50;
			}
		}
		if (((percent % 5) == 0) || percent > 90) {
			ShowProgressBar(percent);
		}
		int bufpos = 0;
		CRGB pixel;
		FileSeekBuf((uint32_t)bmpOffBits + (y * lineLength));
		//uint32_t offset = (bmpOffBits + (y * lineLength));
		//dataFile.seekSet(offset);
		for (int x = displayWidth - 1; x >= 0; --x) {
			// this reads three bytes
			pixel = getRGBwithGamma();
			// see if we want this one
			if (bScaleHeight && (x * displayWidth) % imgWidth) {
				continue;
			}
			SetPixel(x, pixel, y);
		}
		// see how long it took to get here
		if (!bLoopTimed) {
			minLoopTime = millis() - minLoopTime;
			bLoopTimed = true;
			// if fixed time then we need to calculate the framehold value
			if (bFixedTime) {
				// divide the time by the number of frames
				nFrameHold = 1000 * nFixedImageTime / imgHeight;
				nFrameHold -= minLoopTime;
				nFrameHold = max(nFrameHold, 0);
			}
		}
		// wait for timer to expire before we show the next frame
		while (bStripWaiting) {
			delayMicroseconds(100);
			// we should maybe check the cancel key here to handle slow frame rates?
		}
		// now show the lights
		FastLED.show();
		// set a timer while we go ahead and load the next frame
		bStripWaiting = true;
		esp_timer_start_once(oneshot_LED_timer, nFrameHold * 1000);
		// check keys
		if (CheckCancel())
			break;
		if (bManualFrameAdvance) {
			// check if frame advance button requested
			if (nFramePulseCount) {
				for (int ix = nFramePulseCount; ix; --ix) {
					// wait for press
					while (digitalRead(FRAMEBUTTON)) {
						if (CheckCancel())
							break;
						delay(10);
					}
					// wait for release
					while (!digitalRead(FRAMEBUTTON)) {
						if (CheckCancel())
							break;
						delay(10);
					}
				}
			}
			else {
				// by button click or rotate
				int btn;
				for (;;) {
					btn = ReadButton();
					if (btn == BTN_NONE)
						continue;
					else if (btn == BTN_LONG)
						CRotaryDialButton::pushButton(BTN_LONG);
					else if (btn == BTN_LEFT) {
						// backup a line, use 2 because the for loop does one when we're done here
						if (bReverseImage) {
							y += 2;
							if (y > imgHeight)
								y = imgHeight;
						}
						else {
							y -= 2;
							if (y < -1)
								y = -1;
						}
						break;
					}
					else
						break;
					if (CheckCancel())
						break;
					delay(10);
				}
			}
		}
		if (bCancelRun)
			break;
	}
	// all done
	readByte(true);
}

// put the current file on the display
// Note that menu is not used, it is called with NULL sometimes
void ShowBmp(MenuItem*)
{
	if (bShowBuiltInTests)
		return;
	String fn = currentFolder + FileNames[CurrentFileIndex];
	// make sure this is a bmp file, if not just quietly go away
	String tmp = fn.substring(fn.length() - 3);
	tmp.toLowerCase();
	if (tmp.compareTo("bmp")) {
		return;
	}
	bool bSawButton0 = !digitalRead(0);
	uint16_t* scrBuf;
	scrBuf = (uint16_t*)calloc(320 * 144, sizeof(uint16_t));
	if (scrBuf == NULL) {
		WriteMessage("Not enough memory", true, 5000);
		return;
	}
	bool bOldGamma = bGammaCorrection;
	bGammaCorrection = false;
	dataFile = SD.open(fn);
	// if the file is available send it to the LED's
	if (!dataFile.available()) {
		free(scrBuf);
		WriteMessage("failed to open: " + currentFolder + FileNames[CurrentFileIndex], true);
		return;
	}
	tft.fillScreen(TFT_BLACK);
	// clear the file cache buffer
	readByte(true);
	uint16_t bmpType = readInt();
	uint32_t bmpSize = readLong();
	uint16_t bmpReserved1 = readInt();
	uint16_t bmpReserved2 = readInt();
	uint32_t bmpOffBits = readLong();

	/* Check file header */
	if (bmpType != MYBMP_BF_TYPE) {
		free(scrBuf);
		WriteMessage(String("Invalid BMP:\n") + currentFolder + FileNames[CurrentFileIndex], true);
		return;
	}

	/* Read info header */
	uint32_t imgSize = readLong();
	uint32_t imgWidth = readLong();
	uint32_t imgHeight = readLong();
	uint16_t imgPlanes = readInt();
	uint16_t imgBitCount = readInt();
	uint32_t imgCompression = readLong();
	uint32_t imgSizeImage = readLong();
	uint32_t imgXPelsPerMeter = readLong();
	uint32_t imgYPelsPerMeter = readLong();
	uint32_t imgClrUsed = readLong();
	uint32_t imgClrImportant = readLong();

	/* Check info header */
	if (imgWidth <= 0 || imgHeight <= 0 || imgPlanes != 1 ||
		imgBitCount != 24 || imgCompression != MYBMP_BI_RGB || imgSizeImage == 0)
	{
		free(scrBuf);
		WriteMessage(String("Unsupported, must be 24bpp:\n") + currentFolder + FileNames[CurrentFileIndex], true);
		return;
	}

	int displayWidth = imgWidth;
	if (imgWidth > STRIPLENGTH) {
		displayWidth = STRIPLENGTH;           //only display the number of led's we have
	}

	/* compute the line length */
	uint32_t lineLength = imgWidth * 3;
	// fix for padding to 4 byte words
	if ((lineLength % 4) != 0)
		lineLength = (lineLength / 4 + 1) * 4;
	bool done = false;
	bool redraw = true;
	bool allowScroll = imgHeight > 320;
	// offset for showing the image
	int imgOffset = 0;
	int oldImgOffset;
	bool bShowingSize = false;
	// show some info
	float walk = (float)imgHeight / (float)imgWidth;
	DisplayLine(5, "" + String(walk, 2) + " meters " + String(walk * 3.28084, 1) + " feet");
	DisplayLine(6, "Size: " + String(imgWidth) + " x " + String(imgHeight));
	// calculate display time
	float dspTime = bFixedTime ? nFixedImageTime : (imgHeight * nFrameHold / 1000.0 + imgHeight * .008);
	DisplayLine(7, "About " + String((int)round(dspTime)) + " Seconds");
	while (!done) {
		if (redraw) {
			// loop through the image, y is the image width, and x is the image height
			for (int y = imgOffset; y < (imgHeight > 320 ? 320 : imgHeight) + imgOffset; ++y) {
				int bufpos = 0;
				CRGB pixel;
				// get to start of pixel data for this column
				FileSeekBuf((uint32_t)bmpOffBits + (y * lineLength));
				for (int x = displayWidth - 1; x >= 0; --x) {
					// this reads three bytes
					pixel = getRGBwithGamma();
					// add to the display memory
					int row = x - 5;
					int col = y - imgOffset;
					if (row >= 0 && row < 144) {
						uint16_t color = tft.color565(pixel.r, pixel.g, pixel.b);
						uint16_t sbcolor;
						// the memory image colors are byte swapped
						swab(&color, &sbcolor, 2);
						scrBuf[(143 - row) * 320 + col] = sbcolor;
					}
				}
			}
			oldImgOffset = imgOffset;
			// got it all, go show it
			tft.pushRect(0, 0, 320, 144, scrBuf);
		}
		if (bSawButton0) {
			while (digitalRead(0) == 0)
				;
			bSawButton0 = false;
			delay(30);
		}
		switch (ReadButton()) {
		case CRotaryDialButton::BTN_LEFT:
			if (allowScroll) {
				imgOffset -= 320;
				imgOffset = max(0, imgOffset);
			}
			break;
		case CRotaryDialButton::BTN_RIGHT:
			if (allowScroll) {
				imgOffset += 320;
				imgOffset = min((int32_t)imgHeight - 320, imgOffset);
			}
			break;
		case CRotaryDialButton::BTN_LONGPRESS:
			done = true;
			break;
		//case CRotaryDialButton::BTN_CLICK:
		//	if (bShowingSize) {
		//		bShowingSize = false;
		//		redraw = true;
		//	}
		//	else {
		//		tft.fillScreen(TFT_BLACK);
		//		//DisplayLine(0, currentFolder);
		//		//DisplayLine(4, FileNames[CurrentFileIndex]);
		//		float walk = (float)imgHeight / (float)imgWidth;
		//		DisplayLine(5, "" + String(walk, 2) + " meters " + String(walk * 3.28084, 1) + " feet");
		//		DisplayLine(6, "Size: " + String(imgWidth) + " x " + String(imgHeight));
		//		// calculate display time
		//		float dspTime = bFixedTime ? nFixedImageTime : (imgHeight * nFrameHold / 1000.0 + imgHeight * .008);
		//		DisplayLine(7, "About " + String((int)round(dspTime)) + " Seconds");
		//		bShowingSize = true;
		//		redraw = false;
		//	}
		//	break;
		}
		if (oldImgOffset != imgOffset) {
			redraw = true;
		}
		// check the 0 button
		if (digitalRead(0) == 0) {
			// debounce, don't want this seen again in the main loop
			delay(30);
			done = true;
		}
		delay(2);
	}
	// all done
	free(scrBuf);
	dataFile.close();
	readByte(true);
	bGammaCorrection = bOldGamma;
	tft.fillScreen(TFT_BLACK);
}

void DisplayLine(int line, String text, int indent, int16_t color)
{
	if (bPauseDisplay)
		return;
	tft.textdatum = TL_DATUM;
	int charHeight = tft.fontHeight();
	int y = line * charHeight + (bSettingsMode && !bRunningMacro ? 0 : 8);
	tft.fillRect(indent, y, tft.width(), charHeight, TFT_BLACK);
	tft.setTextColor(color);
	tft.drawString(text, indent, y);
}

// the star is used to indicate active menu line
void DisplayMenuLine(int line, int displine, String text)
{
	bool hilite = MenuStack.top()->index == line;
	String mline = (hilite ? "*" : " ") + text;
	if (displine < MENU_LINES)
		DisplayLine(displine, mline, 0, hilite ? menuLineActiveColor : menuLineColor);
}

uint32_t IRAM_ATTR readLong() {
	uint32_t retValue;
	byte incomingbyte;

	incomingbyte = readByte(false);
	retValue = (uint32_t)((byte)incomingbyte);

	incomingbyte = readByte(false);
	retValue += (uint32_t)((byte)incomingbyte) << 8;

	incomingbyte = readByte(false);
	retValue += (uint32_t)((byte)incomingbyte) << 16;

	incomingbyte = readByte(false);
	retValue += (uint32_t)((byte)incomingbyte) << 24;

	return retValue;
}

uint16_t IRAM_ATTR readInt() {
	byte incomingbyte;
	uint16_t retValue;

	incomingbyte = readByte(false);
	retValue += (uint16_t)((byte)incomingbyte);

	incomingbyte = readByte(false);
	retValue += (uint16_t)((byte)incomingbyte) << 8;

	return retValue;
}
byte filebuf[512];
int fileindex = 0;
int filebufsize = 0;
uint32_t filePosition = 0;

int IRAM_ATTR readByte(bool clear) {
	//int retbyte = -1;
	if (clear) {
		filebufsize = 0;
		fileindex = 0;
		return 0;
	}
	// TODO: this needs to align with 512 byte boundaries, maybe
	if (filebufsize == 0 || fileindex >= sizeof(filebuf)) {
		filePosition = dataFile.position();
		//// if not on 512 boundary yet, just return a byte
		//if ((filePosition % 512) && filebufsize == 0) {
		//    //Serial.println("not on 512");
		//    return dataFile.read();
		//}
		// read a block
//        Serial.println("block read");
		do {
			filebufsize = dataFile.read(filebuf, sizeof(filebuf));
		} while (filebufsize < 0);
		fileindex = 0;
	}
	return filebuf[fileindex++];
	//while (retbyte < 0) 
	//    retbyte = dataFile.read();
	//return retbyte;
}


// make sure we are the right place
void IRAM_ATTR FileSeekBuf(uint32_t place)
{
	if (place < filePosition || place >= filePosition + filebufsize) {
		// we need to read some more
		filebufsize = 0;
		dataFile.seek(place);
	}
}

// count the actual files, at a given starting point
int FileCountOnly(int start)
{
	int count = 0;
	// ignore folders, at the end
	for (int files = start; files < FileNames.size(); ++files) {
		if (!IsFolder(files))
			++count;
	}
	return count;
}

// return true if current file is folder
bool IsFolder(int index)
{
	return FileNames[index][0] == NEXT_FOLDER_CHAR
		|| FileNames[index][0] == PREVIOUS_FOLDER_CHAR;
}

// show the current file
void DisplayCurrentFile(bool path)
{
	//String name = FileNames[CurrentFileIndex];
	//String upper = name;
	//upper.toUpperCase();
 //   if (upper.endsWith(".BMP"))
 //       name = name.substring(0, name.length() - 4);
	tft.setTextColor(menuLineActiveColor);
	if (bShowBuiltInTests) {
		DisplayLine(0, FileNames[CurrentFileIndex]);
	}
	else {
		if (bSdCardValid) {
			DisplayLine(0, ((path && bShowFolder) ? currentFolder : "") + FileNames[CurrentFileIndex] + (bMirrorPlayImage ? "><" : ""));
		}
		else {
			WriteMessage("No SD Card or Files", true);
		}
	}
	if (!bIsRunning && bShowNextFiles) {
		for (int ix = 1; ix < MENU_LINES; ++ix) {
			if (ix + CurrentFileIndex >= FileNames.size()) {
				DisplayLine(ix, "", 0, menuLineColor);
			}
			else {
				DisplayLine(ix, "   " + FileNames[CurrentFileIndex + ix], 0, menuLineColor);
			}
		}
	}
	tft.setTextColor(menuLineActiveColor);
	// for debugging keypresses
	//DisplayLine(3, String(nButtonDowns) + " " + nButtonUps);
}

void ShowProgressBar(int percent)
{
	nProgress = percent;
	if (!bShowProgress || bPauseDisplay)
		return;
	static int lastpercent;
	if (lastpercent && (lastpercent == percent))
		return;
	if (percent == 0) {
		tft.fillRect(0, 0, tft.width() - 1, 6, TFT_BLACK);
	}
	DrawProgressBar(0, 0, tft.width() - 1, 6, percent);
	lastpercent = percent;
}

// display message on first line
void WriteMessage(String txt, bool error, int wait)
{
	tft.fillScreen(TFT_BLACK);
	if (error) {
		txt = "**" + txt + "**";
		tft.setTextColor(TFT_RED);
	}
	tft.setCursor(0, tft.fontHeight());
	tft.setTextWrap(true);
	tft.print(txt);
	delay(wait);
	tft.setTextColor(TFT_WHITE);
}

// create the associated MIW name
String MakeMIWFilename(String filename, bool addext)
{
	String cfFile = filename;
	cfFile = cfFile.substring(0, cfFile.lastIndexOf('.'));
	if (addext)
		cfFile += String(".MIW");
	return cfFile;
}

// look for the file in the list
// return -1 if not found
int LookUpFile(String name)
{
	int ix = 0;
	for (auto nm : FileNames) {
		if (name.equalsIgnoreCase(nm)) {
			return ix;
		}
		++ix;
	}
	return -1;
}

// process the lines in the config file
bool ProcessConfigFile(String filename)
{
	bool retval = true;
	String filepath = ((bRunningMacro || bRecordingMacro) ? String("/") : currentFolder) + filename;
#if USE_STANDARD_SD
	SDFile rdfile;
#else
	SDFile rdfile;
#endif
	rdfile = SD.open(filepath);
	if (rdfile.available()) {
		String line, command, args;
		while (line = rdfile.readStringUntil('\n'), line.length()) {
			if (CheckCancel())
				break;
			// read the lines and do what they say
			int ix = line.indexOf('=', 0);
			if (ix > 0) {
				command = line.substring(0, ix);
				command.trim();
				command.toUpperCase();
				args = line.substring(ix + 1);
				args.trim();
				// loop through the var list looking for a match
				for (int which = 0; which < sizeof(SettingsVarList) / sizeof(*SettingsVarList); ++which) {
					if (command.compareTo(SettingsVarList[which].name) == 0) {
						switch (SettingsVarList[which].type) {
						case vtInt:
						{
							int val = args.toInt();
							int min = SettingsVarList[which].min;
							int max = SettingsVarList[which].max;
							if (min != max) {
								val = constrain(val, min, max);
							}
							*(int*)(SettingsVarList[which].address) = val;
						}
						break;
						case vtBool:
							args.toUpperCase();
							*(bool*)(SettingsVarList[which].address) = args[0] == 'T';
							break;
						case vtBuiltIn:
						{
							bool bLastBuiltIn = bShowBuiltInTests;
							args.toUpperCase();
							bool value = args[0] == 'T';
							if (value != bLastBuiltIn) {
								ToggleFilesBuiltin(NULL);
							}
						}
						break;
						case vtShowFile:
						{
							// get the folder and set it first
							String folder;
							String name;
							int ix = args.lastIndexOf('/');
							folder = args.substring(0, ix + 1);
							name = args.substring(ix + 1);
							int oldFileIndex = CurrentFileIndex;
							// save the old folder if necessary
							String oldFolder;
							if (!bShowBuiltInTests && !currentFolder.equalsIgnoreCase(folder)) {
								oldFolder = currentFolder;
								currentFolder = folder;
								GetFileNamesFromSD(folder);
							}
							// search for the file in the list
							int which = LookUpFile(name);
							if (which >= 0) {
								CurrentFileIndex = which;
								// call the process routine
								strcpy(FileToShow, name.c_str());
								tft.fillScreen(TFT_BLACK);
								ProcessFileOrTest();
							}
							if (oldFolder.length()) {
								currentFolder = oldFolder;
								GetFileNamesFromSD(currentFolder);
							}
							CurrentFileIndex = oldFileIndex;
						}
						break;
						case vtRGB:
						{
							// handle the RBG colors
							CRGB* cp = (CRGB*)(SettingsVarList[which].address);
							cp->r = args.toInt();
							args = args.substring(args.indexOf(',') + 1);
							cp->g = args.toInt();
							args = args.substring(args.indexOf(',') + 1);
							cp->b = args.toInt();
						}
						break;
						default:
							break;
						}
						// we found it, so carry on
						break;
					}
				}
			}
		}
		rdfile.close();
	}
	else
		retval = false;
	return retval;
}

// read the files from the card or list the built-ins
// look for start.MIW, and process it, but don't add it to the list
bool GetFileNamesFromSD(String dir) {
	// start over
	// first empty the current file names
	FileNames.clear();
	if (nBootCount == 0)
		CurrentFileIndex = 0;
	if (bShowBuiltInTests) {
		for (int ix = 0; ix < (sizeof(BuiltInFiles) / sizeof(*BuiltInFiles)); ++ix) {
			FileNames.push_back(String(BuiltInFiles[ix].text));
		}
	}
	else {
		String startfile;
		if (dir.length() > 1)
			dir = dir.substring(0, dir.length() - 1);
#if USE_STANDARD_SD
		File root = SD.open(dir);
		File file;
#else
		SDFile root = SD.open(dir, O_RDONLY);
		SDFile file;
#endif
		String CurrentFilename = "";
		if (!root) {
			//Serial.println("Failed to open directory: " + dir);
			//Serial.println("error: " + String(root.getError()));
			//SD.errorPrint("fail");
			return false;
		}
		if (!root.isDirectory()) {
			//Serial.println("Not a directory: " + dir);
			return false;
		}

		file = root.openNextFile();
		if (dir != "/") {
			// add an arrow to go back
			String sdir = currentFolder.substring(0, currentFolder.length() - 1);
			sdir = sdir.substring(0, sdir.lastIndexOf("/"));
			if (sdir.length() == 0)
				sdir = "/";
			FileNames.push_back(String(PREVIOUS_FOLDER_CHAR));
		}
		while (file) {
#if USE_STANDARD_SD
			CurrentFilename = file.name();
#else
			char fname[100];
			file.getName(fname, sizeof(fname));
			CurrentFilename = fname;
#endif
			// strip path
			CurrentFilename = CurrentFilename.substring(CurrentFilename.lastIndexOf('/') + 1);
			//Serial.println("name: " + CurrentFilename);
			if (CurrentFilename != "System Volume Information") {
				if (file.isDirectory()) {
					FileNames.push_back(String(NEXT_FOLDER_CHAR) + CurrentFilename);
				}
				else {
					String uppername = CurrentFilename;
					uppername.toUpperCase();
					if (uppername.endsWith(".BMP")) { //find files with our extension only
						//Serial.println("name: " + CurrentFilename);
						FileNames.push_back(CurrentFilename);
					}
					else if (uppername == "START.MIW") {
						startfile = CurrentFilename;
					}
				}
			}
			file.close();
			file = root.openNextFile();
		}
		root.close();
		std::sort(FileNames.begin(), FileNames.end(), CompareNames);
		// see if we need to process the auto start file
		if (startfile.length())
			ProcessConfigFile(startfile);
	}
	return true;
}

// compare strings for sort ignoring case
bool CompareNames(const String& a, const String& b)
{
	String a1 = a, b1 = b;
	a1.toUpperCase();
	b1.toUpperCase();
	// force folders to sort last
	if (a1[0] == NEXT_FOLDER_CHAR)
		a1[0] = '0x7e';
	if (b1[0] == NEXT_FOLDER_CHAR)
		b1[0] = '0x7e';
	// force previous folder to sort first
	if (a1[0] == PREVIOUS_FOLDER_CHAR)
		a1[0] = '0' - 1;
	if (b1[0] == PREVIOUS_FOLDER_CHAR)
		b1[0] = '0' - 1;
	return a1.compareTo(b1) < 0;
}

// save and restore important settings, two sets are available
// 0 is used by file display, and 1 is used when running macros
bool SettingsSaveRestore(bool save, int set)
{
	static void* memptr[2] = { NULL, NULL };
	if (save) {
		// get some memory and save the values
		if (memptr[set])
			free(memptr[set]);
		memptr[set] = malloc(sizeof saveValueList);
		if (!memptr[set])
			return false;
	}
	void* blockptr = memptr[set];
	if (memptr[set] == NULL) {
		return false;
	}
	for (int ix = 0; ix < (sizeof saveValueList / sizeof * saveValueList); ++ix) {
		if (save) {
			memcpy(blockptr, saveValueList[ix].val, saveValueList[ix].size);
		}
		else {
			memcpy(saveValueList[ix].val, blockptr, saveValueList[ix].size);
		}
		blockptr = (void*)((byte*)blockptr + saveValueList[ix].size);
	}
	if (!save) {
		// if it was saved, restore it and free the memory
		if (memptr[set]) {
			free(memptr[set]);
			memptr[set] = NULL;
		}
	}
	return true;
}

void EraseStartFile(MenuItem* menu)
{
	WriteOrDeleteConfigFile("", true, true);
}

void SaveStartFile(MenuItem* menu)
{
	WriteOrDeleteConfigFile("", false, true);
}

void EraseAssociatedFile(MenuItem* menu)
{
	WriteOrDeleteConfigFile(FileNames[CurrentFileIndex].c_str(), true, false);
}

void SaveAssociatedFile(MenuItem* menu)
{
	WriteOrDeleteConfigFile(FileNames[CurrentFileIndex].c_str(), false, false);
}

void LoadAssociatedFile(MenuItem* menu)
{
	String name = FileNames[CurrentFileIndex];
	name = MakeMIWFilename(name, true);
	if (ProcessConfigFile(name)) {
		WriteMessage(String("Processed:\n") + name);
	}
	else {
		WriteMessage(String("Failed reading:\n") + name, true);
	}
}

void LoadStartFile(MenuItem* menu)
{
	String name = "START.MIW";
	if (ProcessConfigFile(name)) {
		WriteMessage(String("Processed:\n") + name);
	}
	else {
		WriteMessage("Failed reading:\n" + name, true);
	}
}

// create the config file, or remove it
// startfile true makes it use the start.MIW file, else it handles the associated name file
bool WriteOrDeleteConfigFile(String filename, bool remove, bool startfile)
{
	bool retval = true;
	String filepath;
	if (startfile) {
		filepath = currentFolder + String("START.MIW");
	}
	else {
		filepath = ((bRecordingMacro || bRunningMacro) ? String("/") : currentFolder) + MakeMIWFilename(filename, true);
	}
	if (remove) {
		if (!SD.exists(filepath.c_str()))
			WriteMessage(String("Not Found:\n") + filepath);
		else if (SD.remove(filepath.c_str())) {
			WriteMessage(String("Erased:\n") + filepath);
		}
		else {
			WriteMessage(String("Failed to erase:\n") + filepath, true);
		}
	}
	else {
		String line;
#if USE_STANDARD_SD
		File file = SD.open(filepath.c_str(), bRecordingMacro ? FILE_APPEND : FILE_WRITE);
#else
		SDFile file = SD.open(filepath.c_str(), bRecordingMacro ? (O_APPEND | O_WRITE | O_CREAT) : (O_WRITE | O_TRUNC | O_CREAT));
#endif
		if (file) {
			// loop through the var list
			for (int ix = 0; ix < sizeof(SettingsVarList) / sizeof(*SettingsVarList); ++ix) {
				switch (SettingsVarList[ix].type) {
				case vtBuiltIn:
					line = String(SettingsVarList[ix].name) + "=" + String(*(bool*)(SettingsVarList[ix].address) ? "TRUE" : "FALSE");
					break;
				case vtShowFile:
					if (*(char*)(SettingsVarList[ix].address)) {
						line = String(SettingsVarList[ix].name) + "=" + (bShowBuiltInTests ? "" : currentFolder) + String((char*)(SettingsVarList[ix].address));
					}
					break;
				case vtInt:
					line = String(SettingsVarList[ix].name) + "=" + String(*(int*)(SettingsVarList[ix].address));
					break;
				case vtBool:
					line = String(SettingsVarList[ix].name) + "=" + String(*(bool*)(SettingsVarList[ix].address) ? "TRUE" : "FALSE");
					break;
				case vtRGB:
				{
					// handle the RBG colors
					CRGB* cp = (CRGB*)(SettingsVarList[ix].address);
					line = String(SettingsVarList[ix].name) + "=" + String(cp->r) + "," + String(cp->g) + "," + String(cp->b);
				}
				break;
				default:
					line = "";
					break;
				}
				if (line.length())
					file.println(line);
			}
			file.close();
			WriteMessage(String("Saved:\n") + filepath);
		}
		else {
			retval = false;
			WriteMessage(String("Failed to write:\n") + filepath, true);
		}
	}
	return retval;
}

// save some settings in the eeprom
// return true if valid, false if failed
bool SaveSettings(bool save, bool bOnlySignature, bool bAutoloadOnlyFlag)
{
	EEPROM.begin(1024);
	bool retvalue = true;
	int blockpointer = 0;
	for (int ix = 0; ix < (sizeof(saveValueList) / sizeof(*saveValueList)); blockpointer += saveValueList[ix++].size) {
		if (save) {
			EEPROM.writeBytes(blockpointer, saveValueList[ix].val, saveValueList[ix].size);
			if (ix == 0 && bOnlySignature) {
				break;
			}
			if (ix == 1 && bAutoloadOnlyFlag) {
				break;
			}
		}
		else {  // load
			if (ix == 0) {
				// check signature
				char svalue[sizeof(signature)];
				memset(svalue, 0, sizeof(svalue));
				size_t bytesread = EEPROM.readBytes(0, svalue, sizeof(signature));
				if (strcmp(svalue, signature)) {
					WriteMessage("bad eeprom signature\nrepairing...", true);
					return SaveSettings(true);
				}
				if (bOnlySignature) {
					return true;
				}
			}
			else {
				EEPROM.readBytes(blockpointer, saveValueList[ix].val, saveValueList[ix].size);
			}
			if (ix == 1 && bAutoloadOnlyFlag) {
				return true;
			}
		}
	}
	if (save) {
		retvalue = EEPROM.commit();
	}
	else {
		int savedFileIndex = CurrentFileIndex;
		// we don't know the folder path, so just reset the folder level
		currentFolder = "/";
		setupSDcard();
		CurrentFileIndex = savedFileIndex;
		// make sure file index isn't too big
		if (CurrentFileIndex >= FileNames.size()) {
			CurrentFileIndex = 0;
		}
		// set the brightness values since they might have changed
		SetDisplayBrightness(nDisplayBrightness);
		// don't need to do this here since it is always set right before running
		//FastLED.setBrightness(nStripBrightness);
	}
	// don't display if only loading the autoload flag
	if (save || !bAutoloadOnlyFlag) {
		WriteMessage(String(save ? (bAutoloadOnlyFlag ? "Autoload Saved" : "Settings Saved") : "Settings Loaded"), false, 1000);
	}
	EEPROM.end();
	return retvalue;
}

// save the eeprom settings
void SaveEepromSettings(MenuItem* menu)
{
	SaveSettings(true);
}

// load eeprom settings
void LoadEepromSettings(MenuItem* menu)
{
	SaveSettings(false);
}

// save the macro with the current settings
void SaveMacro(MenuItem* menu)
{
	bRecordingMacro = true;
	WriteOrDeleteConfigFile(String(nCurrentMacro), false, false);
	bRecordingMacro = false;
}

// saves and restores settings
void RunMacro(MenuItem* menu)
{
	bCancelMacro = false;
	for (nMacroRepeatsLeft = nRepeatCountMacro; nMacroRepeatsLeft; --nMacroRepeatsLeft) {
		MacroLoadRun(menu, true);
		if (bCancelMacro) {
			break;
		}
		tft.fillScreen(TFT_BLACK);
		for (int wait = nRepeatWaitMacro; nMacroRepeatsLeft > 1 && wait; --wait) {
			if (CheckCancel()) {
				nMacroRepeatsLeft = 0;
				break;
			}
			DisplayLine(4, "#" + String(nCurrentMacro) + String(" Wait: ") + String(wait / 10) + "." + String(wait % 10) + " Repeat: " + String(nMacroRepeatsLeft - 1));
			delay(100);
		}
	}
	bCancelMacro = false;
}

// like run, but doesn't restore settings
void LoadMacro(MenuItem* menu)
{
	MacroLoadRun(menu, false);
}

void MacroLoadRun(MenuItem* menu, bool save)
{
	bool oldShowBuiltins;
	if (save) {
		oldShowBuiltins = bShowBuiltInTests;
		SettingsSaveRestore(true, 1);
	}
	bRunningMacro = true;
	bRecordingMacro = false;
	String line = String(nCurrentMacro) + ".miw";
	if (!ProcessConfigFile(line)) {
		line += " not found";
		WriteMessage(line, true);
	}
	bRunningMacro = false;
	if (save) {
		// need to handle if the builtins was changed
		if (oldShowBuiltins != bShowBuiltInTests) {
			ToggleFilesBuiltin(NULL);
		}
		SettingsSaveRestore(false, 1);
	}
}

void DeleteMacro(MenuItem* menu)
{
	WriteOrDeleteConfigFile(String(nCurrentMacro), true, false);
}

// show some LED's with and without white balance adjust
void ShowWhiteBalance(MenuItem* menu)
{
	for (int ix = 0; ix < 32; ++ix) {
		SetPixel(ix, CRGB(255, 255, 255));
	}
	FastLED.setTemperature(CRGB(255, 255, 255));
	FastLED.show();
	delay(2000);
	FastLED.clear(true);
	delay(50);
	FastLED.setTemperature(CRGB(whiteBalance.r, whiteBalance.g, whiteBalance.b));
	FastLED.show();
	delay(3000);
	FastLED.clear(true);
}

// reverse the strip index order for the lower strip, the upper strip is normal
// also check to make sure it isn't out of range
int AdjustStripIndex(int ix)
{
	switch (stripsMode) {
	case 0:	// bottom reversed, top normal, both wired in the middle
		if (ix < NUM_LEDS) {
			ix = (NUM_LEDS - 1 - ix);
		}
		break;
	case 1:	// bottom and top normal, chained, so nothing to do
		break;
	case 2:	// top reversed, bottom normal, no connection in the middle
		if (ix >= NUM_LEDS) {
			ix = (NUM_LEDS - 1 - ix);
		}
		break;
	}
	// make sure it isn't too big or too small
	ix = constrain(ix, 0, STRIPLENGTH - 1);
	return ix;
}

// write a pixel to the correct location
// pixel doubling is handled here
// e.g. pixel 0 will be 0 and 1, 1 will be 2 and 3, etc
// if upside down n will be n and n-1, n-1 will be n-1 and n-2
// column = -1 to init fade in/out values
void IRAM_ATTR SetPixel(int ix, CRGB pixel, int column, int totalColumns)
{
	static int fadeStep;
	static int fadeColumns;
	static int lastColumn;
	static int maxColumn;
	static int fade;
	if (nFadeInOutFrames) {
		// handle fading
		if (column == -1) {
			fadeColumns = min(totalColumns / 2, nFadeInOutFrames);
			maxColumn = totalColumns;
			fadeStep = 255 / fadeColumns;
			//Serial.println("fadeStep: " + String(fadeStep) + " fadeColumns: " + String(fadeColumns) + " maxColumn: " + String(maxColumn));
			lastColumn = -1;
			fade = 255;
			return;
		}
		// when the column changes check if we are in the fade areas
		if (column != lastColumn) {
			int realColumn = bReverseImage ? maxColumn - 1 - column : column;
			if (realColumn <= fadeColumns) {
				// calculate the fade amount
				fade = realColumn * fadeStep;
				fade = constrain(fade, 0, 255);
				// fading up
				//Serial.println("UP col: " + String(realColumn) + " fade: " + String(fade));
			}
			else if (realColumn >= maxColumn - 1 - fadeColumns) {
				// calculate the fade amount
				fade = (maxColumn - 1 - realColumn) * fadeStep;
				fade = constrain(fade, 0, 255);
				// fading down
				//Serial.println("DOWN col: " + String(realColumn) + " fade: " + String(fade));
			}
			else
				fade = 255;
			lastColumn = column;
		}
	}
	else {
		// no fade
		fade = 255;
	}
	int ix1, ix2;
	if (bUpsideDown) {
		if (bDoublePixels) {
			ix1 = AdjustStripIndex(STRIPLENGTH - 1 - 2 * ix);
			ix2 = AdjustStripIndex(STRIPLENGTH - 2 - 2 * ix);
		}
		else {
			ix1 = AdjustStripIndex(STRIPLENGTH - 1 - ix);
		}
	}
	else {
		if (bDoublePixels) {
			ix1 = AdjustStripIndex(2 * ix);
			ix2 = AdjustStripIndex(2 * ix + 1);
		}
		else {
			ix1 = AdjustStripIndex(ix);
		}
	}
	if (fade != 255) {
		pixel = pixel.nscale8_video(fade);
		//Serial.println("col: " + String(column) + " fade: " + String(fade));
	}
	leds[ix1] = pixel;
	if (bDoublePixels)
		leds[ix2] = pixel;
}

// grow and shrink a rainbow type pattern
#define PI_SCALE 2
#define TWO_HUNDRED_PI (628*PI_SCALE)
void RainbowPulse()
{
	int element = 0;
	int last_element = 0;
	int highest_element = 0;
	//Serial.println("second: " + String(bSecondStrip));
	//Serial.println("Len: " + String(STRIPLENGTH));
	for (int i = 0; i < TWO_HUNDRED_PI; i++) {
		element = round((STRIPLENGTH - 1) / 2 * (-cos(i / (PI_SCALE * 100.0)) + 1));
		//Serial.println("elements: " + String(element) + " " + String(last_element));
		if (element > last_element) {
			SetPixel(element, CHSV(element * nRainbowPulseColorScale + nRainbowPulseStartColor, nRainbowPulseSaturation, 255));
			FastLED.show();
			highest_element = max(highest_element, element);
		}
		if (CheckCancel()) {
			break;
		}
		delayMicroseconds(nRainbowPulsePause * 10);
		if (element < last_element) {
			// cleanup the highest one
			SetPixel(highest_element, CRGB::Black);
			SetPixel(element, CRGB::Black);
			FastLED.show();
		}
		last_element = element;
	}
}

/*
	Write a wedge in time, from the middle out
*/
void TestWedge()
{
	int midPoint = STRIPLENGTH / 2 - 1;
	for (int ix = 0; ix < STRIPLENGTH / 2; ++ix) {
		SetPixel(midPoint + ix, CRGB(nWedgeRed, nWedgeGreen, nWedgeBlue));
		SetPixel(midPoint - ix, CRGB(nWedgeRed, nWedgeGreen, nWedgeBlue));
		if (!bWedgeFill) {
			if (ix > 1) {
				SetPixel(midPoint + ix - 1, CRGB::Black);
				SetPixel(midPoint - ix + 1, CRGB::Black);
			}
			else {
				SetPixel(midPoint, CRGB::Black);
			}
		}
		FastLED.show();
		delay(nFrameHold);
		if (CheckCancel()) {
			return;
		}
	}
	FastLED.clear(true);
}

//#define NUM_LEDS 22
//#define DATA_PIN 5
//#define TWO_HUNDRED_PI 628
//#define TWO_THIRDS_PI 2.094
//
//void loop()
//{
//	int val1 = 0;
//	int val2 = 0;
//	int val3 = 0;
//	for (int i = 0; i < TWO_HUNDRED_PI; i++) {
//		val1 = round(255 / 2.0 * (sin(i / 100.0) + 1));
//		val2 = round(255 / 2.0 * (sin(i / 100.0 + TWO_THIRDS_PI) + 1));
//		val3 = round(255 / 2.0 * (sin(i / 100.0 - TWO_THIRDS_PI) + 1));
//
//		leds[7] = CHSV(0, 255, val1);
//		leds[8] = CHSV(96, 255, val2);
//		leds[9] = CHSV(160, 255, val3);
//
//		FastLED.show();
//
//		delay(1);
//	}
//}
// #########################################################################
// Fill screen with a rainbow pattern
// #########################################################################
byte red = 31;
byte green = 0;
byte blue = 0;
byte state = 0;
unsigned int colour = red << 11; // Colour order is RGB 5+6+5 bits each

void rainbow_fill()
{
	// The colours and state are not initialised so the start colour changes each time the function is called

	for (int i = 319; i >= 0; i--) {
		// Draw a vertical line 1 pixel wide in the selected colour
		tft.drawFastHLine(0, i, tft.width(), colour); // in this example tft.width() returns the pixel width of the display
		// This is a "state machine" that ramps up/down the colour brightnesses in sequence
		switch (state) {
		case 0:
			green++;
			if (green == 64) {
				green = 63;
				state = 1;
			}
			break;
		case 1:
			red--;
			if (red == 255) {
				red = 0;
				state = 2;
			}
			break;
		case 2:
			blue++;
			if (blue == 32) {
				blue = 31;
				state = 3;
			}
			break;
		case 3:
			green--;
			if (green == 255) {
				green = 0;
				state = 4;
			}
			break;
		case 4:
			red++;
			if (red == 32) {
				red = 31;
				state = 5;
			}
			break;
		case 5:
			blue--;
			if (blue == 255) {
				blue = 0;
				state = 0;
			}
			break;
		}
		colour = red << 11 | green << 5 | blue;
	}
}

// draw a progress bar
void DrawProgressBar(int x, int y, int dx, int dy, int percent)
{
	tft.drawRoundRect(x, y, dx, dy, 2, menuLineActiveColor);
	int fill = (dx - 2) * percent / 100;
	// fill the filled part
	tft.fillRect(x + 1, y + 1, fill, dy - 2, TFT_GREEN);
	// blank the empty part
	tft.fillRect(x + 1 + fill, y + 1, dx - 2 - fill, dy - 2, TFT_BLACK);
}

void ReportCouldNotCreateFile(String target) {
	SendHTML_Header();
	webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>");
	webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
	append_page_footer();
	SendHTML_Content();
	SendHTML_Stop();
}

SDFile UploadFile; // I would need some Help here, Martin
void handleFileUpload() { // upload a new file to the Filing system
	HTTPUpload& uploadfile = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
											  // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
	if (uploadfile.status == UPLOAD_FILE_START)
	{
		String filename = uploadfile.filename;
		String filepath = String("/");
		if (!filename.startsWith("/")) filename = "/" + filename;
		Serial.print("Upload File Name: "); Serial.println(filename);
		SD.remove(filename);                         // Remove a previous version, otherwise data is appended the file again
		UploadFile = SD.open(filename, FILE_WRITE);  // Open the file for writing in SPIFFS (create it, if doesn't exist)
		//UploadFile = SD.open(filename, O_WRITE | O_CREAT);
		filename = String();
	}
	else if (uploadfile.status == UPLOAD_FILE_WRITE)
	{
		if (UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
	}
	else if (uploadfile.status == UPLOAD_FILE_END)
	{
		if (UploadFile)          // If the file was successfully created
		{
			UploadFile.close();   // Close the file again
			Serial.print("Upload Size: "); Serial.println(uploadfile.totalSize);
			webpage = "";
			append_page_header();
			webpage += F("<h3>File was successfully uploaded</h3>");
			webpage += F("<h2>Uploaded File Name: "); webpage += uploadfile.filename + "</h2>";
			webpage += F("<h2>File Size: "); webpage += file_size(uploadfile.totalSize) + "</h2><br>";
			append_page_footer();
			server.send(200, "text/html", webpage);
			// reload the file list, if not showing built-ins
			if (!bShowBuiltInTests) {
				GetFileNamesFromSD(currentFolder);
			}
		}
		else
		{
			ReportCouldNotCreateFile("upload");
		}
	}
}


void append_page_header() {
	webpage = F("<!DOCTYPE html><html>");
	webpage += F("<head>");
	webpage += F("<title>MagicImageWand</title>");
	webpage += F("<meta name='viewport' content='user-scalable=yes,initial-scale=1.0,width=device-width'>");
	webpage += F("<style>");
	webpage += F("body{max-width:98%;margin:0 auto;font-family:arial;font-size:100%;text-align:center;color:black;background-color:#888888;}");
	webpage += F("ul{list-style-type:none;margin:0.1em;padding:0;border-radius:0.17em;overflow:hidden;background-color:#EEEEEE;font-size:1em;}");
	webpage += F("li{float:left;border-radius:0.17em;border-right:0.06em solid #bbb;}last-child {border-right:none;font-size:85%}");
	webpage += F("li a{display: block;border-radius:0.17em;padding:0.44em 0.44em;text-decoration:none;font-size:65%}");
	webpage += F("li a:hover{background-color:#DDDDDD;border-radius:0.17em;font-size:85%}");
	webpage += F("section {font-size:0.88em;}");
	webpage += F("h1{color:white;border-radius:0.5em;font-size:1em;padding:0.2em 0.2em;background:#444444;}");
	webpage += F("h2{color:orange;font-size:1.0em;}");
	webpage += F("h3{font-size:0.8em;}");
	webpage += F("table{font-family:arial,sans-serif;font-size:0.9em;border-collapse:collapse;width:100%;}");
	webpage += F("th,td {border:0.06em solid #dddddd;text-align:left;padding:0.3em;border-bottom:0.06em solid #dddddd;}");
	webpage += F("tr:nth-child(odd) {background-color:#eeeeee;}");
	webpage += F(".rcorners_n {border-radius:0.2em;background:#CCCCCC;padding:0.3em 0.3em;width:100%;color:white;font-size:75%;}");
	webpage += F(".rcorners_m {border-radius:0.2em;background:#CCCCCC;padding:0.3em 0.3em;width:100%;color:white;font-size:75%;}");
	webpage += F(".rcorners_w {border-radius:0.2em;background:#CCCCCC;padding:0.3em 0.3em;width:100%;color:white;font-size:75%;}");
	webpage += F(".column{float:left;width:100%;height:100%;}");
	webpage += F(".row:after{content:'';display:table;clear:both;}");
	webpage += F("*{box-sizing:border-box;}");
	webpage += F("footer{background-color:#AAAAAA; text-align:center;padding:0.3em 0.3em;border-radius:0.375em;font-size:60%;}");
	webpage += F("button{border-radius:0.5em;background:#666666;padding:0.3em 0.3em;width:45%;color:white;font-size:100%;}");
	webpage += F(".buttons {border-radius:0.5em;background:#666666;padding:0.3em 0.3em;width:45%;color:white;font-size:80%;}");
	webpage += F(".buttonsm{border-radius:0.5em;background:#666666;padding:0.3em 0.3em;width45%; color:white;font-size:70%;}");
	webpage += F(".buttonm {border-radius:0.5em;background:#666666;padding:0.3em 0.3em;width:45%;color:white;font-size:70%;}");
	webpage += F(".buttonw {border-radius:0.5em;background:#666666;padding:0.3em 0.3em;width:45%;color:white;font-size:70%;}");
	webpage += F("a{font-size:75%;}");
	webpage += F("p{font-size:75%;}");
	webpage += F("</style></head><body><h1>MIW Server<br>"); webpage + "</h1>";
}

void append_page_footer() {
	webpage += "<ul>";
	webpage += "<li><a href='/'>Home</a></li>";
	webpage += "<li><a href='/download'>Download</a></li>";
	webpage += "<li><a href='/upload'>Upload</a></li>";
	webpage += "<li><a href='/settings'>Settings</a></li>";
	webpage += "</ul>";
	webpage += "<footer>MagicImageWand ";
	webpage += myVersion;
	webpage += "</footer>";
	webpage += "</body></html>";
}

void SendHTML_Header() {
	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves. 
	append_page_header();
	server.sendContent(webpage);
	webpage = "";
}

void SendHTML_Content() {
	server.sendContent(webpage);
	webpage = "";
}

void SendHTML_Stop() {
	server.sendContent("");
	server.client().stop(); // Stop is needed because no content length was sent
}

void HomePage() {
	SendHTML_Header();
	webpage += "<a href='/download'><button style=\"width:auto\">Download</button></a>";
	webpage += "<a href='/upload'><button style=\"width:auto\">Upload</button></a>";
	webpage += "<a href='/settings'><button style=\"width:auto\">Settings</button></a>";
	append_page_footer();
	SendHTML_Content();
	SendHTML_Stop();
}


void SelectInput(String heading1, String command, String arg_calling_name) {
	SendHTML_Header();
	webpage += F("<h3>"); webpage += heading1 + "</h3>";
	for (String var : FileNames)
	{
		webpage += String("<p>") + var;
	}
	webpage += F("<FORM action='/"); webpage += command + "' method='post'>";
	webpage += F("<input type='text' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
	webpage += F("<type='submit' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
	webpage += F("<a href='/'>[Back]</a>");
	append_page_footer();
	SendHTML_Content();
	SendHTML_Stop();
}

void File_Download() { // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
	if (server.args() > 0) { // Arguments were received
		if (server.hasArg("download")) SD_file_download(server.arg(0));
	}
	else SelectInput("Enter filename to download", "download", "download");
}

void ReportFileNotPresent(String target) {
	SendHTML_Header();
	webpage += F("<h3>File does not exist</h3>");
	webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
	append_page_footer();
	SendHTML_Content();
	SendHTML_Stop();
}

void ReportSDNotPresent() {
	SendHTML_Header();
	webpage += F("<h3>No SD Card present</h3>");
	webpage += F("<a href='/'>[Back]</a><br><br>");
	append_page_footer();
	SendHTML_Content();
	SendHTML_Stop();
}

void SD_file_download(String filename) {
	if (bSdCardValid) {
		SDFile download = SD.open("/" + filename);
		if (download) {
			server.sendHeader("Content-Type", "text/text");
			server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
			server.sendHeader("Connection", "close");
			//server.streamFile(download, "application/octet-stream");
			server.streamFile(download, "image/bmp");
			download.close();
		}
		else ReportFileNotPresent("download");
	}
	else ReportSDNotPresent();
}

void IncreaseRepeatButton() {
	// This can be for sure made into a universal function like IncreaseButton(Setting, Value)
	webpage += String("&nbsp;<a href='/settings/increpeat'><strong>&#8679;</strong></a>");
}

void DecreaseRepeatButton() {
	// This can be for sure made into a universal function like DecreaseButton(Setting, Value)
	webpage += String("&nbsp;<a href='/settings/decrepeat'><strong>&#8681;</strong></a>");
}

void ShowSettings() {
	append_page_header();
	webpage += "<h3>Current Settings</h3>";
	webpage += String("<p>Current File: ") + currentFolder + FileNames[CurrentFileIndex];
	if (bFixedTime) {
		webpage += String("<p>Fixed Image Time: ") + String(nFixedImageTime) + " S";
	}
	else {
		webpage += String("<p>Column Time: ") + String(nFrameHold) + " mS";
	}
	webpage += String("<p>Repeat Count: ") + String(repeatCount);
	IncreaseRepeatButton();
	DecreaseRepeatButton();
	webpage += String("<p>LED Brightness: ") + String(nStripBrightness);
	append_page_footer();
	server.send(200, "text/html", webpage);
}

void File_Upload() {
	append_page_header();
	webpage += F("<h3>Select File to Upload</h3>");
	webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
	webpage += F("<input class='buttons' style='width:75%' type='file' name='fupload' id = 'fupload' value=''><br>");
	webpage += F("<br><button class='buttons' style='width:75%' type='submit'>Upload File</button><br>");
	webpage += F("<a href='/'>[Back]</a><br><br>");
	append_page_footer();
	server.send(200, "text/html", webpage);
}
