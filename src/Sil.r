/*
 * Minimal Resources for Sil
 *
 * Header files and reasons for their inclusion:
 *
 *  MacTypes.r - 'STR ' and 'STR#'
 *  Finder.r   - 'BNDL' and 'FREF'
 *  Dialogs.r  - 'ALRT', 'DITL' and 'DLOG'
 *  Menus.r    - 'MENU' and 'MBAR'
 *  Processes.r - 'SIZE'
 */

#include "Types.r"

/*
 * Signature - Who am I?
 * ID should always be 0.
 */
#define SilSignature 'Sil1'

type SilSignature as 'STR ';

resource SilSignature(0, "Owner resource", purgeable)
{
	"Sil 1.3.0"
};

/*
 * For Finder (File--Get Info)
 */
resource 'vers' (1) {
	/* Version 1.3.0 */
	0x01, 0x03,

	/* final release, in which case the following # is irrelevant */
	release, 0x00,

	/* We use this when we aren't sure :) */
	verUS,

	/* Short version string */
	"1.3.0",

	/* What really gets displayed as version info */
	"1.3.0"
};


resource 'vers' (2) {
	/* Version 1.3.0 */
	0x01, 0x03,

	/* final release, in which case the following # is irrelevant */
	release, 0x00,

	/* We use this when we aren't sure :) */
	verUS,

	/* Short version string */
	"1.3.0",

	/* What really gets displayed just below the icon */
	/* Say whatever you like here, for example, "It ROCKS!" :) */
	"Sil"
};


#ifndef __MWERKS__

/*
 * Program's characteristics
 * ID should always be -1.
 * In CodeWarrior, use the 'SIZE flags' pull-down menu in the PPC Target panel
 * to set these.
 */
resource 'SIZE' (-1)
{
	/* obsolete "save screen" flag */
	reserved,

	/* accept suspend/resume events */
	acceptSuspendResumeEvents,

	/* obsolete "disable option" flag */
	reserved,

	/* can use background null events */
	canBackground,

	/* activates own windows in response to OS events */
	doesActivateOnFGSwitch,

	/* app has a user interface */
	backgroundAndForeground,

	/* don't return mouse events in front window on resume */
	dontGetFrontClicks,

	/* applications use this */
	ignoreAppDiedEvents,

	/* works with 24- or 32-bit addresses */
	is32BitCompatible,

	/* can use high-level events */
	isHighLevelEventAware,

	/* only local high-level events */
	onlyLocalHLEvents,

	/* can't use stationery documents */
	notStationeryAware,

	/* can't use inline services */
	dontUseTextEditServices,

	/* all windows redrawn when monitor(s) change */
	notDisplayManagerAware,

	reserved,

	reserved,

	/* preferred memory size */
	3 * 1024 * 1024,

	/* minimum memory size */
	3 * 1024 * 1024
};

#endif /* !__MWERKS__ */


#ifndef MACH_O_CARBON

/*
 * File types used by Sil
 * In OS X, you can use the property list alternatively.
 */
resource 'FREF' (128, purgeable)
{
	/* file type */
	'APPL',

	/* icon local ID */
	0,

	/* file name */
	""
};

resource 'FREF' (129, purgeable)
{
	/* file type */
	'SAVE',

	/* icon local ID */
	1,

	/* file name */
	""
};

resource 'FREF' (130, purgeable)
{
	/* file type */
	'TEXT',

	/* icon local ID */
	2,

	/* file name */
	""
};

resource 'FREF' (131, purgeable)
{
	/* file type */
	'DATA',

	/* icon local ID */
	3,

	/* file name */
	""
};


/*
 * Bundle information (icon-file type associations)
 */
resource 'BNDL' (128, purgeable)
{
	/* our signature */
	SilSignature,

	/* resource ID of signature resource: should always be 0 */
	0,

	{
		/* mapping local IDs in 'FREF's to 'ICN#' IDs */
		'ICN#',
		{
			/* local ID 0 -> ICN# 128 */
			0, 128,

			/* local ID 1 -> ICN# 129 */
			1, 129,

			/* local ID 2 -> ICN# 130 */
			2, 130,

			/* local ID 3 -> ICN# 131 */
			3, 131
		},

		/* local res IDs for 'FREF's: no duplicates */
		'FREF',
		{
			/* local ID 0 -> FREF 128 */
			0, 128,

			/* local ID 1 -> FREF 129 */
			1, 129,

			/* local ID 2 -> FREF 130 */
			2, 130,

			/* local ID 3 -> FREF 131 */
			3, 131
		}
	}
};

#endif /* !MACH_O_CARBON */


/*
 * Menu definitions
 */
resource 'MENU' (128, preload)
{
	/* menu ID */
	128,

	/* use standard definition proc */
	textMenuProc,

	/* everything but the divider is enabled */
	0b11111111111111111111111111111101,
	/* or we can use 0... */

	/* enable the title */
	enabled,

	/* menu title */
	apple,

	/* its contents */
	{
		/* First item */
		"About Sil...", noicon, nokey, nomark, plain;

		/* Second item - divider */
		"-", noicon, nokey, nomark, plain;
	}
};

resource 'MENU' (129, preload)
{
	/* menu ID */
	129,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000000000000,

	/* enable the title */
	enabled,

	/* menu title */
	"File",

	/* its contents */
	{
		/* item #1 */
		"New", noicon, "N", nomark, plain;

		/* item #2 */
		"Open", noicon, "O", nomark, plain;

		/* item #3 */
		"Import", noicon, "I", nomark, plain;

		/* item #4 */
		"Close", noicon, "W", nomark, plain;

		/* item #5 */
		"Save", noicon, "S", nomark, plain;

		/* item #6 */
		"-", noicon, nokey, nomark, plain;

#if !defined(CARBON) && !defined(MACH_O_CARBON)

		/* item #7 */
		"Exit", noicon, "E", nomark, plain;

#endif /* !CARBON && !MACH_O_CARBON */

		/* item #8 */
		"Quit", noicon, "Q", nomark, plain;
	}
};

resource 'MENU' (130, preload)
{
	/* menu ID */
	130,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000000000000,

	/* enable the title */
	enabled,

	/* menu title */
	"Edit",

	/* its contents */
	{
		/* item #1 */
		"Undo", noicon, "Z", nomark, plain;

		/* item #2 */
		"-", noicon, nokey, nomark, plain;

		/* item #3 */
		"Cut", noicon, "X", nomark, plain;

		/* item #4 */
		"Copy", noicon, "C", nomark, plain;

		/* item #5 */
		"Paste", noicon, "V", nomark, plain;

		/* item #6 */
		"Clear", noicon, nokey, nomark, plain;
	}
};

resource 'MENU' (131, preload)
{
	/* menu ID */
	131,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000000000000,

	/* enable the title */
	enabled,

	/* menu title */
	"Font",

	/* its contents */
	{
		/* item #1 */
		"Bold", noicon, nokey, nomark, plain;

		/* item #2 */
		"Wide", noicon, nokey, nomark, plain;

		/* item #3 */
		"-", noicon, nokey, nomark, plain;

		/* the rest are supplied by the program */
	}
};

resource 'MENU' (132, preload)
{
	/* menu ID */
	132,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000000000000,

	/* enable the title */
	enabled,

	/* menu title */
	"Size",

	/* its contents */
	{
		/* Let the program fill it in */
	}
};

resource 'MENU' (133, preload)
{
	/* menu ID */
	133,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000011111111,

	/* enable the title */
	enabled,

	/* menu title */
	"Windows",

	/* its contents */
	{
		/* Let the program create them for us */
	}
};

resource 'MENU' (134, preload)
{
	/* menu ID */
	134,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000000000000,

	/* enable the title */
	enabled,

	/* menu title */
	"Special",

	/* its contents */
	{
		/* First item */
		"Sound", noicon, nokey, nomark, plain;

		/* Second item - 0x90 == 144 */
		"Graphics", noicon, hierarchicalMenu, "\0x90", plain;

		/* Third item - 0x91 == 145 */
		"TileWidth", noicon, hierarchicalMenu, "\0x91", plain;

		/* Fourth item - 0x92 == 146 */
		"TileHeight", noicon, hierarchicalMenu, "\0x92", plain;

		/* Third item */
		"-", noicon, nokey, nomark, plain;

		/* Fourth item */
		"Fiddle", noicon, nokey, nomark, plain;

		/* Fifth item */
		"Wizard", noicon, nokey, nomark, plain;
	}
};

/* Graphics submenu */
resource 'MENU' (144, preload)
{
	/* menu ID */
	144,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000000000000,

	/* enable the title */
	enabled,

	/* menu title (ignored) */
	"Graphics",

	/* menu items */
	{
		/* first item */
		"None", noicon, nokey, nomark, plain;

		/* second item */
		"8x8", noicon, nokey, nomark, plain;

		/* third item */
		"16x16", noicon, nokey, nomark, plain;

		/* forth item */
		"32x32", noicon, nokey, nomark, plain;

		/* fifth item */
		"-", noicon, nokey, nomark, plain;

		/* sixth item */
		"Double width tiles", noicon, nokey, nomark, plain;
	}	
};

resource 'MENU' (145, preload)
{
	/* menu ID */
	145,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000000000000,

	/* enable the title */
	enabled,

	/* menu title */
	"TileWidth",

	/* its contents */
	{
		/* Let the program create them for us */
	}
};

resource 'MENU' (146, preload)
{
	/* menu ID */
	146,

	/* use standard definition proc */
	textMenuProc,

	/* let the program enable/disable them */
	0b00000000000000000000000000000000,

	/* enable the title */
	enabled,

	/* menu title */
	"TileHeight",

	/* its contents */
	{
		/* Let the program create them for us */
	}
};

/* Menu bar definition */
resource 'MBAR' (128, preload)
{
	{ 128, 129, 130, 131, 132, 133, 134 }
};



/*
 * DITLs - dialogue item lists
 */
resource 'DITL' (128, purgeable)
{
	{
		/** item #1 **/

		/* bounding rect - top, left, bottom, right */
		{ -4, 0, 225, 308 },

		/* type */
		UserItem
		{
			/* enable flag */
			enabled
		},

		/** item #2 **/

		/* bounding rect - top, left, bottom, right */
		{ 20, 120, 47, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"Sil  1.3.0"
		},

		/** item #3 **/

		/* bounding rect - top, left, bottom, right */
		{ 38, 86, 55, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			""
		},

		/** item #4 **/

		/* bounding rect - top, left, bottom, right */
		{ 56, 87, 72, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"half sick of shadows"
		},

		/** item #5 **/

		/* bounding rect - top, left, bottom, right */
		{ 71, 70, 88, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"(half@amirrorclear.net)"
		},

		/** item #6 **/

		/* bounding rect - top, left, bottom, right */
		{ 105, 79, 122, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"Original Copyright by"
		},

		/** item #7 **/

		/* bounding rect - top, left, bottom, right */
		{ 123, 90, 139, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"Robert A. Koeneke"
		},

		/** item #8 **/

		/* bounding rect - top, left, bottom, right */
		{ 139, 96, 155, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"James E. Wilson"
		},


		/** item #9 **/

		/* bounding rect - top, left, bottom, right */
		{ 155, 107, 171, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"Ben Harrison"
		},

		/** item #10 **/

		/* bounding rect - top, left, bottom, right */
		{ 171, 70, 187, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"and many other people..."
		},

		/** item #10 **/

		/* bounding rect - top, left, bottom, right */
		{ 194, 86, 211, 255 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"Macintosh Version"
		},
	}
};


resource 'DITL' (129, purgeable)
{
	{
		/** item #1 **/

		/* bounding rect */
		{ 45, 353, 65, 411 },

		/* type */
		Button
		{
			/* enable flag */
			enabled,

			/* title */
			"OK"
		},

		/** item #2 **/

		/* bounding rect */
		{ 19, 68, 90, 339 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"^0"
		},

		/** item #3 **/

		/* bounding rect */
		{ 38, 21, 70, 53 },

		/* type */
		Icon
		{
			/* enable flag */
			disabled,

			/* 'ICON' ID */
			128
		}
	}
};


#if !defined(CARBON) && !defined(MACH_O_CARBON)

/*
 * XXX XXX XXX
 * The controversial "quit without saving" dialogue.
 * This was Ben's doing and, being such, had a logical
 * reason to be allowed - frequent and often
 * disasterous system crashes. It may not hold true
 * now that OS X is based on Mach + BSD. If you
 * like to keep it, you may wish to add
 * warning to prevent uninformed abuses, though.
 */
resource 'DITL' (130, purgeable)
{
	{
		/** item #1 **/

		/* bounding rect */
		{ 81, 136, 101, 194 },

		/* type */
		Button
		{
			/* enable flag */
			enabled,

			/* title */
			"No"
		},

		/** item #2 **/

		/* bounding rect */
		{ 81, 37, 101, 95 },

		/* type */
		Button
		{
			/* enable flag */
			enabled,

			/* title */
			"Yes"
		},

		/** item #3 **/

		/* bounding rect */
		{ 24, 43, 56, 188 },

		/* type */
		StaticText
		{
			/* enable flag */
			disabled,

			/* title */
			"Do you really want to quit without saving?"
		}		
	}
};

#endif /* !CARBON && !MACH_O_CARBON */


resource 'DLOG' (128, purgeable)
{
	/* bounding rect */
	{ 112, 202, 341, 512 },

	/* procID */
	dBoxProc,

	/* visibility */
	invisible,

	/* has closebox? */
	noGoAway,

	/* refCon */
	0x0,

	/* 'DITL' ID */
	128,

	/* title */
	"",

#if DLOG_RezTemplateVersion == 1

	/* position */
	centerMainScreen

#endif /* DLOG_RezTemplateVersion == 1 */
};


resource 'ALRT' (128, purgeable)
{
	/* bounding rect */
	{ 40, 40, 150, 471 },

	/* 'DITL' ID */
	129,

	/* bold outline, draw alert and beeps */
	{
		/* stage 4 */
		OK, visible, sound1;

		/* stage 3 */
		OK, visible, sound1;

		/* stage 2 */
		OK, visible, sound1;

		/* stage 1 */
		OK, visible, sound1;
	},

#if ALRT_RezTemplateVersion == 1

	/* centered to parent window */
	centerParentWindow

#endif /* ALRT_RezTemplateVersion == 1 */
};

resource 'ALRT' (129, purgeable)
{
	/* bounding rect */
	{ 40, 40, 150, 471 },

	/* 'DITL' ID */
	129,

	/* bold outline, draw alert and beeps */
	{
		/* stage 4 */
		OK, visible, sound1;

		/* stage 3 */
		OK, visible, sound1;

		/* stage 2 */
		OK, visible, sound1;

		/* stage 1 */
		OK, visible, sound1;
	},

#if ALRT_RezTemplateVersion == 1

	/* centered to parent window */
	centerParentWindow

#endif /* ALRT_RezTemplateVersion == 1 */
};

resource 'ALRT' (130, purgeable)
{
	/* bounding rect */
	{ 144, 154, 283, 384 },

	/* 'DITL' ID */
	130,

	/* bold outline, draw alert and beeps */
	{
		/* stage 4 */
		OK, visible, sound1;

		/* stage 3 */
		OK, visible, sound1;

		/* stage 2 */
		OK, visible, sound1;

		/* stage 1 */
		OK, visible, sound1;
	},

#if ALRT_RezTemplateVersion == 1

	/* centered to parent window */
	centerParentWindow

#endif /* ALRT_RezTemplateVersion == 1 */
};


/*
 * Icons - bitmap data
 */

/* Alert symbol */
data 'ICON' (128, purgeable) {
	$"0001 8000 0003 C000 0003 C000 0006 6000"
	$"0006 6000 000C 3000 000C 3000 0018 1800"
	$"0019 9800 0033 CC00 0033 CC00 0063 C600"
	$"0063 C600 00C3 C300 00C3 C300 0183 C180"
	$"0183 C180 0303 C0C0 0303 C0C0 0603 C060"
	$"0601 8060 0C01 8030 0C00 0030 1800 0018"
	$"1801 8018 3003 C00C 3003 C00C 6001 8006"
	$"6000 0006 C000 0003 FFFF FFFF 7FFF FFFE"
};


#ifndef MACH_O_CARBON

/*
 * The JRRT icons we all know and love
 */
data 'icl4' (129, purgeable) {
	$"000F FFFF FFFF FFFF FFFF FFF0 0000 0000"
	$"000F 0000 0000 0000 0000 0CFF 0000 0000"
	$"000F 000F FFFF FFFF 0000 0CF0 F000 0000"
	$"000F 000F 0F0F 0F0F 0000 0CF0 0F00 0000"
	$"000F 0FFF FFFF FFFF FFFF FCF0 00F0 0000"
	$"000F 0F0F 0F0F 0F0F 0F0F 0CF0 000F 0000"
	$"000F 0FFF FFFF FFFF FFFF FCFF FFFF F000"
	$"000F 000F 0F0F 0F0F 0000 00CC CCCC FC00"
	$"000F 000F FFFF FFFF 0000 0000 0FF0 FC00"
	$"000F 0000 000F 0F00 0000 0000 0F00 FC00"
	$"000F 0000 000F FF00 0FFF FFFF FFF0 FC00"
	$"000F 0000 000F 0F00 0F0F 0F0F 0F00 FC00"
	$"000F 0FFF FFFF FFFF FFFF FFFF FFF0 FC00"
	$"000F 0F0F 0F0F 0F0F 0F00 0000 0F00 FC00"
	$"000F 0FFF FFFF FFFF F000 FFC0 0000 FC00"
	$"000F 0F0F 0000 0000 00FF FC0F C000 FC00"
	$"000F 0FFF 0000 00FC 000F FC0C 0FC0 FC00"
	$"000F 0F0F 0000 00FF FFFF FFFF FFC0 FC00"
	$"000F 0FFF FFFF 0FFC CFFF FFFC CFFC FC00"
	$"000F 0F0F 0F0F 0CC0 FC0F FC0F CCC0 FC00"
	$"000F 0FFF FFFF 0000 FC0F FC0F C000 FC00"
	$"000F 0F0F 0F0F 0F00 0FFF FFFC 0000 FC00"
	$"000F 0FFF FFFF FFF0 00FF FFC0 0000 FC00"
	$"000F 0F0F 0F0F 0F00 0FCF FCF0 0000 FC00"
	$"000F 0FFF FFFF FF0F FC0F FC0F FC00 FC00"
	$"000F 0F0F 0F0F 0F0C C00F FC0C C000 FC00"
	$"000F 0FFF FFFF FF00 0FCF FC00 0000 FC00"
	$"000F 0F0F 0F0F 0F0F 0C0F FC00 0000 FC00"
	$"000F 0FFF FFFF FFFF 000F CFFC 0000 FC00"
	$"000F 0F0F 0F0F 0F0F 0FFC 0CC0 0000 FC00"
	$"000F 0000 0000 0000 0CC0 0000 0000 FC00"
	$"000F FFFF FFFF FFFF FFFF FFFF FFFF FC00"
};

data 'icl4' (130, purgeable) {
	$"000F FFFF FFFF FFFF FFFF FFF0 0000 0000"
	$"000F 0000 0000 0000 0000 0CFF 0000 0000"
	$"000F 0000 0000 0000 0000 0CF0 F000 0000"
	$"000F 0000 FFFF FFFF FFFF CCF0 0F00 0000"
	$"000F 0000 CCCC CCCC CCCC CCF0 00F0 0000"
	$"000F 0000 0000 0000 0000 0CF0 000F 0000"
	$"000F 00FF FFFF FFFF FFFF CCFF FFFF F000"
	$"000F 00CC CCCC CCCC CCCC C0CC CCCC FC00"
	$"000F 0000 0000 0000 0000 0000 0000 FC00"
	$"000F 00FF FFFF FFFF FFFF FFFF FFFC FC00"
	$"000F 00CC CCCC CCCC CCCC CCCC CCCC FC00"
	$"000F 0000 0000 0000 0000 0000 0000 FC00"
	$"000F 00FF FFFF FFFF FFFF FFFF FFFC FC00"
	$"000F 00CC CCCC CCCC CCCC CCCC CCCC FC00"
	$"000F 0000 0000 0000 0000 FFC0 0000 FC00"
	$"000F 00FF FFFF C000 00FF FC0F C000 FC00"
	$"000F 00CC CCCC C0FC 000F FC0C 0FC0 FC00"
	$"000F 0000 0000 00FF FFFF FFFF FFC0 FC00"
	$"000F 00FF FFFC 0FFC CFFF FFFC CFFC FC00"
	$"000F 00CC CCCC 0CC0 FC0F FC0F CCC0 FC00"
	$"000F 0000 0000 0000 FC0F FC0F C000 FC00"
	$"000F 0000 FFFF FC00 0FFF FFFC 0000 FC00"
	$"000F 0000 CCCC CC00 00FF FFC0 0000 FC00"
	$"000F 0000 0000 0000 0FCF FCF0 0000 FC00"
	$"000F 00FF FFFF FC0F FC0F FC0F FC00 FC00"
	$"000F 00CC CCCC CC0C C00F FC0C C000 FC00"
	$"000F 0000 0000 0000 0FCF FC00 0000 FC00"
	$"000F 00FF FFFF FFC0 0C0F FC00 0000 FC00"
	$"000F 00CC CCCC CCC0 000F CFFC 0000 FC00"
	$"000F 0000 0000 0000 0FFC 0CC0 0000 FC00"
	$"000F 0000 0000 0000 0CC0 0000 0000 FC00"
	$"000F FFFF FFFF FFFF FFFF FFFF FFFF FC00"
};

data 'icl4' (128, purgeable) {
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"F333 3333 3FFF 000F ED00 FFF3 3333 333F"
	$"F333 333F F000 00FE D0ED 00FF F333 333F"
	$"F333 33FF 0000 00DD 0FED 0FDF FF33 333F"
	$"F333 3F00 0000 00FF 0ED0 00FD 00F3 333F"
	$"F333 F000 0000 00CF FED0 00D0 000F 333F"
	$"F33F 000F 0000 000F FED0 0000 0E00 F33F"
	$"F3FF 00FF FFFF FFFF FFFF FFFF FFE0 FF3F"
	$"F3F0 0FFF FFFF FFFF FFFF FFFF FFFE 0F3F"
	$"FF00 FEED DDDD DDDF FEDD DDDD DDFF EDFF"
	$"FF00 FDD0 000F FFFF FEFF FF00 0000 EDFF"
	$"FF00 D000 00FF DDDF FEDD DFF0 0000 D0FF"
	$"F000 0000 0FFD 000F FED0 00FE D000 000F"
	$"F000 0000 0FFD 000F FED0 00FE D000 000F"
	$"F000 0000 0FFD 000F FED0 00FE D000 000F"
	$"F000 0000 00FF D00F FED0 0FFD 0000 000F"
	$"F000 0000 000F FFFF FEFF FFD0 0000 000F"
	$"F000 0000 000D DDFF FEFD DD00 0000 000F"
	$"F000 0000 0000 0FFF FEFF 0000 0000 000F"
	$"F000 0000 0000 FFDF FEDF F000 0000 000F"
	$"FF00 0000 000F FD0F FED0 FF00 0000 00FF"
	$"FF00 000F FFFF D00F FED0 0FFF FFD0 00FF"
	$"FF00 0000 FFFD 000F FED0 00FF FD00 00FF"
	$"F3F0 0000 DDD0 000F FED0 00DD D000 0F3F"
	$"F3FF 0000 0000 F00F FED0 0000 0000 FF3F"
	$"F33F 0000 000F DFDF FED0 0000 0000 F33F"
	$"F333 F000 0000 FD0F FFD0 0000 000F 333F"
	$"F333 3F00 0000 D00F FFF0 FFD0 00F3 333F"
	$"F333 33FF 00F0 00FF FDFF FD00 FF33 333F"
	$"F333 333F F00F FFFF D00D D00F F333 333F"
	$"F333 3333 3FFF FFDD 0000 FFF3 3333 333F"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
};

data 'icl4' (131, purgeable) {
	$"000F FFFF FFFF FFFF FFFF FFF0 0000 0000"
	$"000F 0000 0000 0000 0000 00FF 0000 0000"
	$"000F 0F00 0F00 FF00 0FF0 00FC F000 0000"
	$"000F 0FF0 FF0F 00F0 F00F 00FC 0F00 0000"
	$"000F 0F0F 0F0F 00F0 F000 00FC 00F0 0000"
	$"000F 0F00 0F0F FFF0 F000 00FC 000F 0000"
	$"000F 0F00 0F0F 00F0 F00F 00FF FFFF F000"
	$"000F 0F00 0F0F 00F0 0FF0 000C CCCC FC00"
	$"000F 0000 0000 0000 0000 0000 0000 FC00"
	$"000F FFFF FFFF FFFF FFFF FFFF FFFF FC00"
	$"000F CCCC CCCC CCCC CCCC CCCC CCCC FC00"
	$"000F 0000 0000 0000 0000 0000 0000 FC00"
	$"000F 0000 0000 0000 0000 0000 0000 FC00"
	$"000F 0000 0000 0000 0000 0000 0000 FC00"
	$"000F 0000 0000 0000 0000 FFC0 0000 FC00"
	$"000F 0000 0000 0000 00FF FC0F C000 FC00"
	$"000F 0000 0000 00FC 000F FC0C 0FC0 FC00"
	$"000F 0000 0000 00FF FFFF FFFF FFC0 FC00"
	$"000F 0000 0000 0FFC CFFF FFFC CFFC FC00"
	$"000F 0000 0000 0CC0 FC0F FC0F CCC0 FC00"
	$"000F 0000 0000 0000 FC0F FC0F C000 FC00"
	$"000F 0000 0000 0000 0FFF FFFC 0000 FC00"
	$"000F 0000 0000 0000 00FF FFC0 0000 FC00"
	$"000F 0000 0000 0000 0FCF FCF0 0000 FC00"
	$"000F 0000 0000 000F FC0F FC0F FC00 FC00"
	$"000F 0000 0000 000C C00F FC0C C000 FC00"
	$"000F 0000 0000 0000 0FCF FC00 0000 FC00"
	$"000F 0000 0000 0000 0C0F FC00 0000 FC00"
	$"000F 0000 0000 0000 000F CFFC 0000 FC00"
	$"000F 0000 0000 0000 0FFC 0CC0 0000 FC00"
	$"000F 0000 0000 0000 0CC0 0000 0000 FC00"
	$"000F FFFF FFFF FFFF FFFF FFFF FFFF FC00"
};

data 'ICN#' (128, purgeable) {
	$"FFFF DFFF FFF1 8FFF FF83 23FF FF00 657F"
	$"FC03 423F F801 C01F F101 C04F F3FF FFEF"
	$"E7FF FFF7 CE01 C03B C81F FC0B C031 C603"
	$"8061 C301 8061 C301 8061 C301 8031 C601"
	$"801F FC01 8003 E001 8007 F001 800D D801"
	$"C019 CC03 C1F1 C7C3 C0E1 C383 E001 C007"
	$"F009 C00F F015 C00F F809 C01F FC01 EC3F"
	$"FF23 B8FF FF9F 01FF FFFC 0FFF FFF3 FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
};

data 'ICN#' (129, purgeable) {
	$"1FFF FE00 1000 0300 11FF 0280 1155 0240"
	$"17FF FA20 1555 5210 17FF FBF8 1155 0008"
	$"11FF 0068 1014 0048 101C 7FE8 1014 5548"
	$"17FF FFE8 1555 4008 17FF 8C08 1500 3908"
	$"1702 1848 1503 FFC8 17F6 7E68 1550 9908"
	$"17F0 9908 1554 7E08 17FE 3C08 1554 5A08"
	$"17FD 9988 1554 1808 17FC 5808 1555 1808"
	$"17FF 1608 1555 6008 1000 0008 1FFF FFF8"
	$"1FFF FE00 1FFF FF00 1FFF FF80 1FFF FFC0"
	$"1FFF FFE0 1FFF FFF0 1FFF FFF8 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
};

data 'ICN#' (130, purgeable) {
	$"1FFF FE00 1000 0300 1000 0280 10FF F240"
	$"1000 0220 1000 0210 13FF F3F8 1000 0008"
	$"1000 0008 13FF FFE8 1000 0008 1000 0008"
	$"13FF FFE8 1000 0008 1000 0C08 13F0 3908"
	$"1002 1848 1003 FFC8 13E6 7E68 1000 9908"
	$"1000 9908 10F8 7E08 1000 3C08 1000 5A48"
	$"13F9 9988 1000 1808 1000 5808 13FC 1808"
	$"1000 1608 1000 6008 1000 0008 1FFF FFF8"
	$"1FFF FE00 1FFF FF00 1FFF FF80 1FFF FFC0"
	$"1FFF FFE0 1FFF FFF0 1FFF FFF8 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
};

data 'ICN#' (131, purgeable) {
	$"1FFF FE00 1000 0300 144C 6280 16D2 9240"
	$"1552 8220 145E 8210 1452 93F8 1452 6008"
	$"1000 0008 1FFF FFF8 1000 0008 1000 0008"
	$"1000 0008 1000 0008 1000 0C08 1000 3908"
	$"1002 1848 1003 FFC8 1006 7E68 1000 9908"
	$"1000 9908 1000 7E08 1000 3C08 1000 5A48"
	$"1001 9988 1000 1808 1000 5808 1000 1808"
	$"1000 1608 1000 6008 1000 0008 1FFF FFF8"
	$"1FFF FE00 1FFF FF00 1FFF FF80 1FFF FFC0"
	$"1FFF FFE0 1FFF FFF0 1FFF FFF8 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
	$"1FFF FFFC 1FFF FFFC 1FFF FFFC 1FFF FFFC"
};

data 'ics#' (128, purgeable) {
	$"FFFF F99F F18F FFFF E7E3 8991 8991 87E1"
	$"83C1 85A1 9999 C183 E587 F18F F97F FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
};

data 'ics#' (129, purgeable) {
	$"7FF0 4018 5FD4 555E 5FC2 4412 5FFA 5502"
	$"5F12 557E 5F3A 5556 5F3A 551A 4022 7FFE"
	$"7FF0 7FF8 7FFC 7FFE 7FFE 7FFE 7FFE 7FFE"
	$"7FFE 7FFE 7FFE 7FFE 7FFE 7FFE 7FFE 7FFE"
};

data 'ics#' (130, purgeable) {
	$"7FF0 4018 5FD4 401E 5FC2 4002 5FFA 4002"
	$"5F12 407E 5F3A 4056 5F3A 401A 4022 7FFE"
	$"7FF0 7FF8 7FFC 7FFE 7FFF 7FFF 7FFF 7FFF"
	$"7FFF 7FFF 7FFF 7FFF 7FFF 7FFF 7FFF 7FFF"
};

data 'ics#' (131, purgeable) {
	$"7FF0 4018 5FD4 5F9E 57C2 4002 7FFE 4002"
	$"4012 407E 403A 4056 403A 401A 4022 7FFE"
	$"7FF0 7FF8 7FFC 7FFE 7FFF 7FFF 7FFF 7FFF"
	$"7FFF 7FFF 7FFF 7FFF 7FFF 7FFF 7FFF 7FFF"
};

data 'icl8' (129, purgeable) {
	$"0000 00FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FF00 0000 0000 0000 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 F5F5 F5F7 FFFF 0000 0000 0000 0000"
	$"0000 00FF F5F5 F5FF FFFF FFFF FFFF FFFF"
	$"F5F5 F5F5 F5F7 FFF5 FF00 0000 0000 0000"
	$"0000 00FF F5F5 F5FF F5FF F5FF F5FF F5FF"
	$"F5F5 F5F5 F5F7 FFF5 F5FF 0000 0000 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFF7 FFF5 F5F5 FF00 0000 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5FF F5FF"
	$"F5FF F5FF F5F7 FFF5 F5F5 F5FF 0000 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFF7 FFFF FFFF FFFF FF00 0000"
	$"0000 00FF F5F5 F5FF F5FF F5FF F5FF F5FF"
	$"F5F5 F5F5 F5F5 F7F7 F7F7 F7F7 FFF7 0000"
	$"0000 00FF F5F5 F5FF FFFF FFFF FFFF FFFF"
	$"F5F5 F5F5 F5F5 F5F5 F5FF FFF5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5FF F5FF F5F5"
	$"F5F5 F5F5 F5F5 F5F5 F5FF F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5FF FFFF F5F5"
	$"F5FF FFFF FFFF FFFF FFFF FFF5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5FF F5FF F5F5"
	$"F5FF F5FF F5FF F5FF F5FF F5F5 FFF7 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFF5 FFF7 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5FF F5FF"
	$"F5FF F5F5 F5F5 F5F5 F5FF F5F5 FFF7 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF FFFF FFFF"
	$"FFF5 F5F5 FFFF F8F6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5FF F5FF F5F5 F5F5 F5F5 F5F5"
	$"F5F5 FFFF FFF8 F7FF F8F6 F5F5 FFF7 0000"
	$"0000 00FF F5FF FFFF F5F5 F5F5 F5F5 FFF8"
	$"F6F5 F5FF FFF8 F6F8 F6FF F8F6 FFF7 0000"
	$"0000 00FF F5FF F5FF F5F5 F5F5 F5F5 FFFF"
	$"FFFF FFFF FFFF FFFF FFFF F8F6 FFF7 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF F5FF FFF8"
	$"F8FF FFFF FFFF FFF8 F6FF FFF8 FFF7 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5F8 F8F6"
	$"FFF8 F6FF FFF8 F6FF F8F6 F6F5 FFF7 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF F5F6 F6F5"
	$"FFF8 F6FF FFF8 F6FF F8F6 F5F5 FFF7 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5FF F5F5"
	$"F5FF FFFF FFFF FFF8 F6F5 F5F5 FFF7 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF FFFF FFF5"
	$"F5F5 FFFF FFFF F8F6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5FF F5F5"
	$"F5FF F8FF FFF8 FFF5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF FFFF F5FF"
	$"FFF8 F6FF FFF8 F6FF FFF8 F5F5 FFF7 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5FF F5F8"
	$"F8F6 F5FF FFF8 F6F8 F8F5 F5F5 FFF7 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF FFFF F5F6"
	$"F6FF F8FF FFF8 F6F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5FF F5FF"
	$"F5F8 F6FF FFF8 F6F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5FF FFFF FFFF FFFF FFFF FFFF"
	$"F5F6 F5FF F8FF FFF8 F6F5 F5F5 FFF7 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5FF F5FF"
	$"F5FF FFF8 F7F8 F8F6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F8 F8F7 F6F6 F6F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFF7 0000"
};

data 'icl8' (128, purgeable) {
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF F9FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFD7 D7D7 D7D7 D7D7 D7FF FFFF 0000 00FF"
	$"FCF9 F7F7 FFFF FFD7 D7D7 D7D7 D7D7 6BFF"
	$"FFD7 4747 4747 4747 FF00 0000 F5F5 FFFC"
	$"F9F7 FCF8 F7F5 FFFF FF47 4747 4747 6BFF"
	$"FFD7 4747 4747 FFFF 00F5 F5F5 F5F5 F9F9"
	$"F7FF FCF8 F7FF F7FF F9FF 4747 4747 6BFF"
	$"FFD7 4747 47FF 0000 F5F5 F5F5 F5F5 FFFF"
	$"F7FC F9F7 F5F5 FFF9 F7F5 FF47 4747 6BFF"
	$"FFD7 4747 FF00 F5F5 F5F5 F5F5 F5F5 F7FF"
	$"FFFC F9F7 F5F5 F9F7 F5F5 F5FF 4747 6BFF"
	$"FFD7 47FF 00F5 F5FF F5F5 F5F5 F5F5 F5FF"
	$"FFFC F9F7 F5F5 F5F5 F5FC F5F5 FF47 6BFF"
	$"FFD7 47FF 00F5 FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FCF5 FF47 6BFF"
	$"FFD7 FF00 F5FF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFC F5FF 6BFF"
	$"FFFF 00F7 FFFC FCF9 F9F9 F9F9 F9F9 F9FF"
	$"FFFC F9F9 F9F9 F9F9 F9F9 FFFF FCF9 FFFF"
	$"FFFF 00F7 FCF9 F9F7 F7F7 F7FF FFFF FFFF"
	$"FFFC FFFF FFFF F7F7 F7F7 F7F7 FCF9 FFFF"
	$"FFFF 00F7 F9F7 F7F5 F5F5 FFFF F8F8 F8FF"
	$"FFFC F9F9 F9FF FFF7 F7F5 F5F7 F9F7 FFFF"
	$"FFF5 F5F5 F7F5 F5F5 F5FF FFF9 F7F7 F7FF"
	$"FFFC F9F7 F7F7 FFFC F8F7 F5F5 F7F7 F8FF"
	$"FF00 F5F5 F5F5 F5F5 F5FF FFF9 F7F5 F5FF"
	$"FFFC F9F7 F5F5 FFFC F8F7 F5F5 F5F5 F8FF"
	$"FF00 F5F5 F5F5 F5F5 F5FF FFF9 F7F5 F5FF"
	$"FFFC F9F7 F5F5 FFFC F8F7 F5F5 F5F5 F8FF"
	$"FF00 F5F5 F5F5 F5F5 F5F5 FFFF F9F7 F5FF"
	$"FFFC F9F7 F5FF FFF8 F7F5 F5F5 F5F5 F8FF"
	$"FF00 F5F5 F5F5 F5F5 F5F5 F5FF FFFF FFFF"
	$"FFFC FFFF FFFF F8F7 F5F5 F5F5 F5F5 F8FF"
	$"FF00 F5F5 F5F5 F5F5 F5F5 F5F9 F9F9 FFFF"
	$"FFFC FFF9 F9F9 F7F5 F5F5 F5F5 F5F5 F8FF"
	$"FF00 F5F5 F5F5 F5F5 F5F5 F5F5 F5FF FFFF"
	$"FFFC FFFF F7F7 F5F5 F5F5 F5F5 F5F5 F8FF"
	$"FF00 F5F5 F5F5 F5F5 F5F5 F5F5 FFFF F9FF"
	$"FFFC F9FF FFF5 F5F5 F5F5 F5F5 F5F5 F8FF"
	$"FFFF F5F5 F5F5 F5F5 F5F5 F5FF FFF9 F7FF"
	$"FFFC F9F7 FFFF F5F5 F5F5 F5F5 F5F8 FFFF"
	$"FFFF F5F5 F5F5 F7FF FFFF FFFF F9F7 F5FF"
	$"FFFC F9F7 F7FF FFFF FFFF F9F5 F5F8 FFFF"
	$"FFFF F5F5 F5F5 F5F7 FFFF FFF9 F7F5 F5FF"
	$"FFFC F9F7 F5F7 FFFF FFF9 F7F5 F5F8 FFFF"
	$"FFD7 FFF5 F5F5 F5F7 F9F9 F9F7 F5F5 F5FF"
	$"FFFC F9F7 F5F5 F9F9 F9F7 F5F5 F8FF 6BFF"
	$"FFD7 47FF F5F5 F5F5 F7F7 F7F5 FFF6 F5FF"
	$"FFFC F9F7 F5F5 F5F7 F7F5 F5F8 FF47 6BFF"
	$"FFD7 47FF F5F5 F5F5 F5F5 F5FF F7FF F8FF"
	$"FFFC F9F7 F5F5 F5F5 F5F5 F5F8 FF47 6BFF"
	$"FFD7 4747 FFF5 F5F5 F5F5 F5F5 FFF8 F7FF"
	$"FFFF F9F7 F5F5 F5F5 F5F5 F8FF 4747 6BFF"
	$"FFD7 4747 47FF F5F5 F5F5 F5F5 F8F7 F5FF"
	$"FFFF FFF7 FFFF F9F7 F8F8 FF47 4747 6BFF"
	$"FFD7 4747 4747 FFFF F5F5 FFF5 F7F5 FFFF"
	$"FFF8 FFFF FFF9 F7F8 FFFF 4747 4747 6BFF"
	$"FFD7 4747 4747 4747 FFF5 F5FF FFFF FFFF"
	$"F8F6 F5F9 F9F7 F5FF 4747 4747 4747 6BFF"
	$"FFD7 6B6B 6B6B 6B6B 6BFF FFFF FFFF F8F8"
	$"F8F8 F8F5 FFFF FF6B 6B6B 6B6B 6B6B 6BFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF F8F8 FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
};

data 'icl8' (130, purgeable) {
	$"0000 00FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FF00 0000 0000 0000 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 F5F5 F5F8 FFFF 0000 0000 0000 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 F5F5 F5F8 FFF6 FF00 0000 0000 0000"
	$"0000 00FF F5F5 F5F5 FFFF FFFF FFFF FFFF"
	$"FFFF FFFF F8F8 FFF6 F6FF 0000 0000 0000"
	$"0000 00FF F5F5 F5F5 F8F8 F8F8 F8F8 F8F8"
	$"F8F8 F8F8 F8F8 FFF6 F6F6 FF00 0000 0000"
	$"0000 00FF F5F5 F5F5 F6F6 F6F6 F6F6 F6F6"
	$"F6F6 F6F6 F6F8 FFF6 F6F6 F6FF 0000 0000"
	$"0000 00FF F5F5 FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF F8F8 FFFF FFFF FFFF FF00 0000"
	$"0000 00FF F5F5 F8F8 F8F8 F8F8 F8F8 F8F8"
	$"F8F8 F8F8 F8F5 F8F8 F8F8 F8F8 FFF7 0000"
	$"0000 00FF F5F5 F6F6 F6F6 F6F6 F6F6 F6F6"
	$"F6F6 F6F6 F6F6 F6F6 F6F6 F6F6 FFF7 0000"
	$"0000 00FF F5F5 FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFF8 FFF7 0000"
	$"0000 00FF F5F5 F8F8 F8F8 F8F8 F8F8 F8F8"
	$"F8F8 F8F8 F8F8 F8F8 F8F8 F8F8 FFF7 0000"
	$"0000 00FF F5F5 F6F6 F6F6 F6F6 F6F6 F6F6"
	$"F6F6 F6F6 F6F6 F6F6 F6F6 F6F6 FFF7 0000"
	$"0000 00FF F5F5 FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFF8 FFF7 0000"
	$"0000 00FF F5F5 F8F8 F8F8 F8F8 F8F8 F8F8"
	$"F8F8 F8F8 F8F8 F8F8 F8F8 F8F8 FFF7 0000"
	$"0000 00FF F5F5 F6F6 F6F6 F6F6 F6F6 F6F6"
	$"F6F6 F6F6 FFFF F8F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 FFFF FFFF FFFF F8F5 F5F5"
	$"F5F5 FFFF FFF8 F6FF F8F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F8F8 F8F8 F8F8 F8F5 FFF8"
	$"F5F5 F5FF FFF8 F5F8 F5FF F8F6 FFF7 0000"
	$"0000 00FF F5F5 F6F6 F6F6 F6F6 F6F5 FFFF"
	$"FFFF FFFF FFFF FFFF FFFF F8F6 FFF7 0000"
	$"0000 00FF F5F5 FFFF FFFF FFF8 F6FF FFF8"
	$"F8FF FFFF FFFF FFF8 F8FF FFF8 FFF7 0000"
	$"0000 00FF F5F5 F8F8 F8F8 F8F8 F6F8 F8F5"
	$"FFF8 F6FF FFF8 F6FF F8F8 F8F6 FFF7 0000"
	$"0000 00FF F5F5 F6F6 F6F6 F6F6 F6F5 F5F5"
	$"FFF8 F6FF FFF8 F6FF F8F6 F6F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 FFFF FFFF FFF8 F6F5"
	$"F5FF FFFF FFFF FFF8 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F8F8 F8F8 F8F8 F6F5"
	$"F5F5 FFFF FFFF F8F6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F6F6 F6F6 F6F6 F6F5"
	$"F5FF F8FF FFF8 FFF6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 FFFF FFFF FFFF FFF8 F6FF"
	$"FFF8 F6FF FFF8 F6FF FFF8 F6F5 FFF7 0000"
	$"0000 00FF F5F5 F8F8 F8F8 F8F8 F8F8 F6F8"
	$"F8F6 F5FF FFF8 F6F8 F8F6 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F6F6 F6F6 F6F6 F6F6 F6F5"
	$"F6FF F8FF FFF8 F6F6 F6F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 FFFF FFFF FFFF FFFF F8F6"
	$"F5F8 F6FF FFF8 F6F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F8F8 F8F8 F8F8 F8F8 F8F6"
	$"F5F6 F5FF F8FF FFF8 F6F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F6F6 F6F6 F6F6 F6F6 F6F6"
	$"F5FF FFF8 F6F8 F8F6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F8 F8F6 F5F5 F6F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFF7 0000"
};

data 'icl8' (131, purgeable) {
	$"0000 00FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FF00 0000 0000 0000 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 F5F5 F5F5 FFFF 0000 0000 0000 0000"
	$"0000 00FF F5FF F5F5 F5FF F5F5 FFFF F5F5"
	$"F5FF FFF5 F5F5 FFF8 FF00 0000 0000 0000"
	$"0000 00FF F5FF FFF5 FFFF F5FF 00F5 FFF5"
	$"FF00 00FF F5F5 FFF8 F6FF 0000 0000 0000"
	$"0000 00FF F5FF F5FF F5FF F5FF F5F5 FFF5"
	$"FF00 0000 F5F5 FFF8 F6F6 FF00 0000 0000"
	$"0000 00FF F5FF F5F5 F5FF F5FF FFFF FFF5"
	$"FF00 F5F5 F5F5 FFF8 F6F6 F6FF 0000 0000"
	$"0000 00FF F5FF F5F5 F5FF F5FF F5F5 FFF5"
	$"FF00 F5FF 00F5 FFFF FFFF FFFF FF00 0000"
	$"0000 00FF F5FF F5F5 F5FF F5FF F5F5 FFF5"
	$"00FF FF00 0000 F5F8 F8F8 F8F8 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F500 00F5 F5F5 F5F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFF7 0000"
	$"0000 00FF F8F8 F8F8 F8F8 F8F8 F8F8 F8F8"
	$"F8F8 F8F8 F8F8 F8F8 F8F8 F8F8 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 F5F5 F5F5 F5F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 F5F5 F5F5 F5F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 F5F5 F5F5 F5F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 F500 FFFF F8F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 FFFF FFF8 F6FF F8F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 FFF8"
	$"F5F5 F5FF FFF8 F5F8 F5FF F8F6 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 FFFF"
	$"FFFF FFFF FFFF FFFF FFFF F8F6 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5FF FFF8"
	$"F8FF FFFF FFFF FFF8 F8FF FFF8 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F8 F8F5"
	$"FFF8 F6FF FFF8 F6FF F8F8 F8F6 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"FFF8 F6FF FFF8 F6FF F8F6 F6F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5FF FFFF FFFF FFF8 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F5 FFFF FFFF F8F6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5FF F8FF FFF8 FFF6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5FF"
	$"FFF8 F6FF FFF8 F6FF FFF8 F6F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F8"
	$"F8F6 F5FF FFF8 F6F8 F8F6 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F6FF F8FF FFF8 F6F6 F6F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F8 F6FF FFF8 F6F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F6 F5FF F8FF FFF8 F6F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5FF FFF8 F6F8 F8F6 F5F5 F5F5 FFF7 0000"
	$"0000 00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5"
	$"F5F8 F8F6 F5F5 F6F5 F5F5 F5F5 FFF7 0000"
	$"0000 00FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFF7 0000"
};

data 'ics8' (128, purgeable) {
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FF47 4747 FFFF F6FF FFF8 FFFF 4747 47FF"
	$"FF47 47FF F6F6 F6FF FFF8 F6F6 FF47 47FF"
	$"FF47 FFFF FFFF FFFF FFFF FFFF FFFF 47FF"
	$"FF47 FFF8 F8FF FFFF FFFF FFF8 F8F8 47FF"
	$"FFFF F8F6 FFF8 F8FF FFF8 F8FF F8F6 FFFF"
	$"FFF6 F6F6 FFF8 F6FF FFF8 F6FF F8F6 F6FF"
	$"FFF6 F6F6 F6FF FFFF FFFF FFF8 F6F6 F6FF"
	$"FFF6 F6F6 F6F6 FFFF FFFF F8F6 F6F6 F6FF"
	$"FFF6 F6F6 F6FF F8FF FFF8 FFF6 F6F6 F6FF"
	$"FFFF F6FF FFF8 F6FF FFF8 F6FF FFF8 FFFF"
	$"FFFF F6F8 F8F6 F6FF FFF8 F6F8 F8F8 47FF"
	$"FF47 FFF6 F6FF F8FF FFF8 F6F6 F6FF 47FF"
	$"FF47 47FF F6F8 F6FF FFF8 F6F6 FF47 47FF"
	$"FF47 4747 FFFF F6FF F8FF FFFF 4747 47FF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF"
};

data 'ics8' (129, purgeable) {
	$"00FF FFFF FFFF FFFF FFFF FFFF 0000 0000"
	$"00FF F5F5 F5F5 F5F5 F5F5 F5FF FF00 0000"
	$"00FF F5FF FFFF FFFF FFFF F5FF F5FF 0000"
	$"00FF F5FF F5FF F5FF F5FF F5FF FFFF FF00"
	$"00FF F5FF FFFF FFFF FFFF F5F5 F5F5 FF00"
	$"00FF F5F5 F5FF F5F5 F5F5 F5FF F5F5 FF00"
	$"00FF F5FF FFFF FFFF FFFF FFFF FFF5 FF00"
	$"00FF F5FF F5FF F5FF F5F5 F5F5 F5F5 FF00"
	$"00FF F5FF FFFF FFFF F5F5 F5FF F5F5 FF00"
	$"00FF F5FF F5FF F5FF F5FF FFFF FFFF FF00"
	$"00FF F5FF FFFF FFFF F5F5 FFFF FF00 FF00"
	$"00FF F5FF F5FF F5FF F5FF 00FF 00FF FF00"
	$"00FF F5FF FFFF FFFF F5F5 FFFF FFF5 FF00"
	$"00FF F5FF F5FF F5FF F5F5 F5FF FFF5 FF00"
	$"00FF F5F5 F5F5 F5F5 F5F5 FFF5 F5F5 FF00"
	$"00FF FFFF FFFF FFFF FFFF FFFF FFFF FF00"
};

data 'ics8' (130, purgeable) {
	$"00FF FFFF FFFF FFFF FFFF FFFF 0000 0000"
	$"00FF F5F5 F5F5 F5F5 F5F5 F5FF FF00 0000"
	$"00FF F5FF FFFF FFFF FFFF F7FF F5FF 0000"
	$"00FF F5F7 F7F7 F7F7 F7F7 F7FF FFFF FF00"
	$"00FF F5FF FFFF FFFF FFFF F7F5 F5F5 FFF7"
	$"00FF F5F7 F7F7 F7F7 F7F7 F7F5 F5F5 FFF7"
	$"00FF F5FF FFFF FFFF FFFF FFFF FFF7 FFF7"
	$"00FF F5F7 F7F7 F7F7 F7F7 F7F7 F7F7 FFF7"
	$"00FF F5FF FFFF FFFF F7F5 F5FF F5F5 FFF7"
	$"00FF F5F7 F7F7 F7F7 F7FF FFFF FFFF FFF7"
	$"00FF F5FF FFFF FFFF F7F5 FFFF FFF5 FFF7"
	$"00FF F5F7 F7F7 F7F7 F7FF F5FF F5FF FFF7"
	$"00FF F5FF FFFF FFFF F7F5 FFFF FFF5 FFF7"
	$"00FF F5F7 F7F7 F7F7 F7F5 F5FF FFF5 FFF7"
	$"00FF F5F5 F5F5 F5F5 F5F5 FFF5 F5F5 FFF7"
	$"00FF FFFF FFFF FFFF FFFF FFFF FFFF FFF7"
};

data 'ics8' (131, purgeable) {
	$"00FF FFFF FFFF FFFF FFFF FFFF 0000 0000"
	$"00FF F5F5 F5F5 F5F5 F5F5 F5FF FF00 0000"
	$"00FF F5FF FFFF FFFF FFFF F5FF F5FF 0000"
	$"00FF F5FF FFFF FFFF FF00 F5FF FFFF FF00"
	$"00FF F5FF 00FF FFFF FFFF F5F5 F7F7 FFF7"
	$"00FF F5F5 F5F5 F5F5 F5F5 F5F5 F5F5 FFF7"
	$"00FF FFFF FFFF FFFF FFFF FFFF FFFF FFF7"
	$"00FF F7F7 F7F7 F7F7 F7F7 F7F7 F7F7 FFF7"
	$"00FF F5F5 F5F5 F5F5 F5F5 F5FF F5F5 FFF7"
	$"00FF F5F5 F5F5 F5F5 F5FF FFFF FFFF FFF7"
	$"00FF F5F5 F5F5 F5F5 F5F5 FFFF FFF5 FFF7"
	$"00FF F5F5 F5F5 F5F5 F5FF F5FF F5FF FFF7"
	$"00FF F5F5 F5F5 F5F5 F5F5 FFFF FFF5 FFF7"
	$"00FF F5F5 F5F5 F5F5 F5F5 F5FF FFF5 FFF7"
	$"00FF F5F5 F5F5 F5F5 F5F5 FFF5 F5F5 FFF7"
	$"00FF FFFF FFFF FFFF FFFF FFFF FFFF FFF7"
};

data 'ics4' (128, purgeable) {
	$"FFFF FFFF FFFF FFFF F333 FFCF FCFF 333F"
	$"F33F CCCF FCCC F33F F3FF FFFF FFFF FF3F"
	$"FFFC CFFF FFFC CCFF FFCC FCCF FCCF CCFF"
	$"FCCC FCCF FCCF CCCF FCCC CFFF FFFC CCCF"
	$"FCCC CCFF FFCC CCCF FCCC CFCF FCFC CCCF"
	$"FFCF FCCF FCCF FCFF FFCC CCCF FCCC CCFF"
	$"F3FC CFCF FCCC CF3F F33F CCCF FCCC F33F"
	$"F333 FFCF CFFF 333F FFFF FFFF FFFF FFFF"
};

data 'ics4' (129, purgeable) {
	$"0FFF FFFF FFFF 0000 0F00 0000 000F F000"
	$"0F0F FFFF FF0F 0F00 0F0F 0F0F 0F0F FFF0"
	$"0F0F FFFF FF00 00F0 0F00 0F00 000F 00F0"
	$"0F0F FFFF FFFF F0F0 0F0F 0F0F 0000 00F0"
	$"0F0F FFFF 000F 00F0 0F0F 0F0F 0FFF FFF0"
	$"0F0F FFFF 00FF F0F0 0F0F 0F0F 0F0F 0FF0"
	$"0F0F FFFF 00FF F0F0 0F0F 0F0F 000F F0F0"
	$"0F00 0000 00F0 00F0 0FFF FFFF FFFF FFF0"
};

data 'ics4' (130, purgeable) {
	$"0FFF FFFF FFFF 0000 0FCC CCCC CCCF F000"
	$"0FCF FFFF FFCF CF00 0FCC CCCC CCCF FFF0"
	$"0FCF FFFF FFCC CCFD 0FCC CCCC CCCC CCFD"
	$"0FCF FFFF FFFF FCFD 0FCC CCCC CCCC CCFD"
	$"0FCF FFFF CCCF CCFD 0FCC CCCC CFFF FFFD"
	$"0FCF FFFF CCFF FCFD 0FCC CCCC CFCF CFFD"
	$"0FCF FFFF CCFF FCFD 0FCC CCCC CCCF FCFD"
	$"0FCC CCCC CCFC CCFD 0FFF FFFF FFFF FFFD"
};

data 'ics4' (131, purgeable) {
	$"0FFF FFFF FFFF 0000 0FCC CCCC CCCF F000"
	$"0FCF FFFF FFCF CF00 0FCF FFFF FCCF FFF0"
	$"0FCF CFFF FFCC DDFD 0FCC CCCC CCCC CCFD"
	$"0FFF FFFF FFFF FFFD 0FCC CCCC CCCC CCFD"
	$"0FCC CCCC CCCF CCFD 0FCC CCCC CFFF FFFD"
	$"0FCC CCCC CCFF FCFD 0FCC CCCC CFCF CFFD"
	$"0FCC CCCC CCFF FCFD 0FCC CCCC CCCF FCFD"
	$"0FCC CCCC CCFC CCFD 0FFF FFFF FFFF FFFD"
};

#endif /* !MACH_O_CARBON */
