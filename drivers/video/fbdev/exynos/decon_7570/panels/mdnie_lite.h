/*
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef _MDNIE_LITE_H_
#define _MDNIE_LITE_H_

enum mdnie_mode {
	MODE_DYNAMIC = 0,
	MODE_STANDARD,
	MODE_NATURAL,
	MODE_MOVIE,
	MODE_AUTO,
	MODE_MAX,
};

enum mdnie_scenario {
	SCENARIO_UI = 0,
	SCENARIO_GALLERY,
	SCENARIO_VIDEO,
	SCENARIO_VTCALL,
	SCENARIO_CAMERA,
	SCENARIO_BROWSER,
	SCENARIO_NEGATIVE,
	SCENARIO_EMAIL,
	SCENARIO_EBOOK,
	SCENARIO_GRAY,
	SCENARIO_CURTAIN,
	SCENARIO_GRAY_NEGATIVE,
	SCENARIO_MAX,
};

enum mdnie_negative {
	NEGATIVE_OFF = 0,
	NEGATIVE_ON,
};

enum mdnie_outdoor {
	OUTDOOR_OFF = 0,
	OUTDOOR_ON,
	OUTDOOR_MAX,
};

struct mdnie_lite_device {
	enum mdnie_mode background;
	enum mdnie_scenario scenario;
	enum mdnie_outdoor outdoor;
	struct device *dev;
};
#endif /*_MDNIE_LITE_H_*/
