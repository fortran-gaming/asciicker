
// PLATFORM: MS-WINDOWS 

#include <Windows.h>
#pragma comment(lib,"OpenGL32.lib")

#include <stdint.h>

#include "gl.h"
#include "wglext.h"

#include "asciiid_platform.h"

static PlatformInterface platform_api;
static int mouse_b = 0;
static int mouse_x = 0;
static int mouse_y = 0;
static bool track = false;
static bool closing = false;

static LARGE_INTEGER coarse_perf, timer_freq;
static uint64_t coarse_micro;

static const unsigned char ki_to_vk[256] =
{
	0,

	VK_BACK,
	VK_TAB,
	VK_RETURN,

	VK_PAUSE,
	VK_ESCAPE,

	VK_SPACE,
	VK_PRIOR,
	VK_NEXT,
	VK_END,
	VK_HOME,
	VK_LEFT,
	VK_UP,
	VK_RIGHT,
	VK_DOWN,

	VK_PRINT,
	VK_INSERT,
	VK_DELETE,

	'0',
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',

	'A',
	'B',
	'C',
	'D',
	'E',
	'F',
	'G',
	'H',
	'I',
	'J',
	'K',
	'L',
	'M',
	'N',
	'O',
	'P',
	'Q',
	'R',
	'S',
	'T',
	'U',
	'V',
	'W',
	'X',
	'Y',
	'Z',

	VK_LWIN,
	VK_RWIN,
	VK_APPS,

	VK_NUMPAD0,
	VK_NUMPAD1,
	VK_NUMPAD2,
	VK_NUMPAD3,
	VK_NUMPAD4,
	VK_NUMPAD5,
	VK_NUMPAD6,
	VK_NUMPAD7,
	VK_NUMPAD8,
	VK_NUMPAD9,
	VK_MULTIPLY,
	VK_DIVIDE,
	VK_ADD,
	VK_SUBTRACT,
	VK_DECIMAL,
	VK_RETURN, // numpad!

	VK_F1,
	VK_F2,
	VK_F3,
	VK_F4,
	VK_F5,
	VK_F6,
	VK_F7,
	VK_F8,
	VK_F9,
	VK_F10,
	VK_F11,
	VK_F12,
	VK_F13,
	VK_F14,
	VK_F15,
	VK_F16,
	VK_F17,
	VK_F18,
	VK_F19,
	VK_F20,
	VK_F21,
	VK_F22,
	VK_F23,
	VK_F24,

	VK_CAPITAL,
	VK_NUMLOCK,
	VK_SCROLL,

	VK_LSHIFT,
	VK_RSHIFT,
	VK_LCONTROL,
	VK_RCONTROL,
	VK_LMENU,
	VK_RMENU,

	VK_OEM_1,		// ';:' for US
	VK_OEM_PLUS,	// '+' any country
	VK_OEM_COMMA,	// ',' any country
	VK_OEM_MINUS,	// '-' any country
	VK_OEM_PERIOD,	// '.' any country
	VK_OEM_2,		// '/?' for US
	VK_OEM_3,		// '`~' for US

	VK_OEM_4,       //  '[{' for US
	VK_OEM_6,		//  ']}' for US
	VK_OEM_5,		//  '\|' for US
	VK_OEM_7,		//  ''"' for US
	0,
};


static const unsigned char vk_to_ki[256] =
{
	0,					// 0x00
	0,					// VK_LBUTTON        0x01
	0,					// VK_RBUTTON        0x02
	0,					// VK_CANCEL         0x03
	0,					// VK_MBUTTON        0x04    /* NOT contiguous with L & RBUTTON */
	0,					// VK_XBUTTON1       0x05    /* NOT contiguous with L & RBUTTON */
	0,					// VK_XBUTTON2       0x06    /* NOT contiguous with L & RBUTTON */
	0,					// * 0x07 : reserved
	A3D_BACKSPACE,		// VK_BACK           0x08
	A3D_TAB,			// VK_TAB            0x09
	0,					// reserved
	0,					// reserved
	0,					// VK_CLEAR          0x0C
	A3D_ENTER,			// VK_RETURN         0x0D   - CHECK IF EXTENED then 124 (numpad ENTER)
	0,					// unassigned
	0,					// unassigned
	A3D_LSHIFT,			// VK_SHIFT          0x10 - CHECK IF EXTENED then 108 (VK_RSHIFT)
	A3D_LCTRL,			// VK_CONTROL        0x11 - CHECK IF EXTENED then 110 (VK_RCONTROL)
	A3D_LALT,			// VK_MENU           0x12 - CHECK IF EXTENED then 112 (VK_RMENU)
	A3D_PAUSE,			// VK_PAUSE          0x13
	A3D_CAPSLOCK,		// VK_CAPITAL        0x14
	0,					// VK_KANA           0x15
	0,					// * 0x16 : unassigned
	0,					// VK_JUNJA          0x17
	0,					// VK_FINAL          0x18
	0,					// VK_HANJA          0x19 // VK_KANJI          0x19
	0,					// 0x1A : unassigned
	A3D_ESCAPE,			// VK_ESCAPE         0x1B
	0,					// VK_CONVERT        0x1C
	0,					// VK_NONCONVERT     0x1D
	0,					// VK_ACCEPT         0x1E
	0,					// VK_MODECHANGE     0x1F
	A3D_SPACE,			// VK_SPACE          0x20
	A3D_PAGEUP,			// VK_PRIOR          0x21
	A3D_PAGEDOWN,		// VK_NEXT           0x22
	A3D_END,			// VK_END            0x23
	A3D_HOME,			// VK_HOME           0x24
	A3D_LEFT,			// VK_LEFT           0x25
	A3D_UP,				// VK_UP             0x26
	A3D_RIGHT,			// VK_RIGHT          0x27
	A3D_DOWN,			// VK_DOWN           0x28
	0,					// VK_SELECT         0x29
	A3D_PRINT,			// VK_PRINT          0x2A
	0,					// VK_EXECUTE        0x2B
	0,					// VK_SNAPSHOT       0x2C
	A3D_INSERT,			// VK_INSERT         0x2D
	A3D_DELETE,			// VK_DELETE         0x2E
	0,					// VK_HELP           0x2F
	A3D_0,				// VK_0				 0x30
	A3D_1,				// VK_1				 0x31
	A3D_2,				// VK_2				 0x32
	A3D_3,				// VK_3				 0x33
	A3D_4,				// VK_4				 0x34
	A3D_5,				// VK_5				 0x35
	A3D_6,				// VK_6				 0x36
	A3D_7,				// VK_7				 0x37
	A3D_8,				// VK_8				 0x38
	A3D_9,				// VK_9				 0x39
	0,					// * 0x3A - unassigned
	0,					// * 0x3B - unassigned
	0,					// * 0x3C - unassigned
	0,					// * 0x3D - unassigned
	0,					// * 0x3E - unassigned
	0,					// * 0x3F - unassigned
	0,					// - 0x40 : unassigned
	A3D_A,				// VK_A				 0x41
	A3D_B,				// VK_B				 0x42
	A3D_C,				// VK_C				 0x43
	A3D_D,				// VK_D				 0x44
	A3D_E,				// VK_E				 0x45
	A3D_F,				// VK_F				 0x46
	A3D_G,				// VK_G				 0x47
	A3D_H,				// VK_H				 0x48
	A3D_I,				// VK_I				 0x49
	A3D_J,				// VK_J				 0x4A
	A3D_K,				// VK_K              0x4B
	A3D_L,				// VK_L              0x4C
	A3D_M,				// VK_M              0x4D
	A3D_N,				// VK_N              0x4E
	A3D_O,				// VK_O              0x4F
	A3D_P,				// VK_P              0x50
	A3D_Q,				// VK_Q              0x51
	A3D_R,				// VK_R              0x52
	A3D_S,				// VK_S              0x53
	A3D_T,				// VK_T              0x54
	A3D_U,				// VK_U				 0x55
	A3D_V,				// VK_V				 0x56
	A3D_W,				// VK_W				 0x57
	A3D_X,				// VK_X				 0x58
	A3D_Y,				// VK_Y				 0x59
	A3D_Z,				// VK_Z              0x5A
	A3D_LWIN,			// VK_LWIN           0x5B
	A3D_RWIN,			// VK_RWIN           0x5C
	A3D_APPS,			// VK_APPS           0x5D
	0,					// * 0x5E : reserved
	0,					// VK_SLEEP          0x5F
	A3D_NUMPAD_0,		// VK_NUMPAD0        0x60
	A3D_NUMPAD_1,		// VK_NUMPAD1        0x61
	A3D_NUMPAD_2,		// VK_NUMPAD2        0x62
	A3D_NUMPAD_3,		// VK_NUMPAD3        0x63
	A3D_NUMPAD_4,		// VK_NUMPAD4        0x64
	A3D_NUMPAD_5,		// VK_NUMPAD5        0x65
	A3D_NUMPAD_6,		// VK_NUMPAD6        0x66
	A3D_NUMPAD_7,		// VK_NUMPAD7        0x67
	A3D_NUMPAD_8,		// VK_NUMPAD8        0x68
	A3D_NUMPAD_9,		// VK_NUMPAD9        0x69
	A3D_NUMPAD_MULTIPLY,// VK_MULTIPLY       0x6A
	A3D_NUMPAD_ADD,		// VK_ADD            0x6B
	0,					// VK_SEPARATOR      0x6C
	A3D_NUMPAD_SUBTRACT,// VK_SUBTRACT       0x6D
	A3D_NUMPAD_DECIMAL, // VK_DECIMAL        0x6E
	A3D_NUMPAD_DIVIDE,	// VK_DIVIDE         0x6F
	A3D_F1,				// VK_F1             0x70
	A3D_F2,				// VK_F2             0x71
	A3D_F3,				// VK_F3             0x72
	A3D_F4,				// VK_F4             0x73
	A3D_F5,				// VK_F5             0x74
	A3D_F6,				// VK_F6             0x75
	A3D_F7,				// VK_F7             0x76
	A3D_F8,				// VK_F8             0x77
	A3D_F9,				// VK_F9             0x78
	A3D_F10,			// VK_F10            0x79
	A3D_F11,			// VK_F11            0x7A
	A3D_F12,			// VK_F12            0x7B
	A3D_F13,			// VK_F13            0x7C
	A3D_F14,			// VK_F14            0x7D
	A3D_F15,			// VK_F15            0x7E
	A3D_F16,			// VK_F16            0x7F
	A3D_F17,			// VK_F17            0x80
	A3D_F18,			// VK_F18            0x81
	A3D_F19,			// VK_F19            0x82
	A3D_F20,			// VK_F20            0x83
	A3D_F21,			// VK_F21            0x84
	A3D_F22,			// VK_F22            0x85
	A3D_F23,			// VK_F23            0x86
	A3D_F24,			// VK_F24            0x87
	0,					// VK_NAVIGATION_VIEW     0x88 // reserved
	0,					// VK_NAVIGATION_MENU     0x89 // reserved
	0,					// VK_NAVIGATION_UP       0x8A // reserved
	0,					// VK_NAVIGATION_DOWN     0x8B // reserved
	0,					// VK_NAVIGATION_LEFT     0x8C // reserved
	0,					// VK_NAVIGATION_RIGHT    0x8D // reserved
	0,					// VK_NAVIGATION_ACCEPT   0x8E // reserved
	0,					// VK_NAVIGATION_CANCEL   0x8F // reserved
	A3D_NUMLOCK,		// VK_NUMLOCK        0x90
	A3D_SCROLLLOCK,		// VK_SCROLL         0x91
	0,					// VK_OEM_NEC_EQUAL  0x92   // '=' key on numpad AND VK_OEM_FJ_JISHO   0x92   // 'Dictionary' key
	0,					// VK_OEM_FJ_MASSHOU 0x93   // 'Unregister word' key
	0,					// VK_OEM_FJ_TOUROKU 0x94   // 'Register word' key
	0,					// VK_OEM_FJ_LOYA    0x95   // 'Left OYAYUBI' key
	0,					// VK_OEM_FJ_ROYA    0x96   // 'Right OYAYUBI' key
	0,					// 0x97 : unassigned
	0,					// 0x98 : unassigned
	0,					// 0x99 : unassigned
	0,					// 0x9A : unassigned
	0,					// 0x9B : unassigned
	0,					// 0x9C : unassigned
	0,					// 0x9D : unassigned
	0,					// 0x9E : unassigned
	0,					// 0x9F : unassigned
	A3D_LSHIFT,			// VK_LSHIFT         0xA0
	A3D_RSHIFT,			// VK_RSHIFT         0xA1
	A3D_LCTRL,			// VK_LCONTROL       0xA2
	A3D_RCTRL,			// VK_RCONTROL       0xA3
	A3D_LALT,			// VK_LMENU          0xA4
	A3D_RALT,			// VK_RMENU          0xA5
	0,					// VK_BROWSER_BACK        0xA6
	0,					// VK_BROWSER_FORWARD     0xA7
	0,					// VK_BROWSER_REFRESH     0xA8
	0,					// VK_BROWSER_STOP        0xA9
	0,					// VK_BROWSER_SEARCH      0xAA
	0,					// VK_BROWSER_FAVORITES   0xAB
	0,					// VK_BROWSER_HOME        0xAC
	0,					// VK_VOLUME_MUTE         0xAD
	0,					// VK_VOLUME_DOWN         0xAE
	0,					// VK_VOLUME_UP           0xAF
	0,					// VK_MEDIA_NEXT_TRACK    0xB0
	0,					// VK_MEDIA_PREV_TRACK    0xB1
	0,					// VK_MEDIA_STOP          0xB2
	0,					// VK_MEDIA_PLAY_PAUSE    0xB3
	0,					// VK_LAUNCH_MAIL         0xB4
	0,					// VK_LAUNCH_MEDIA_SELECT 0xB5
	0,					// VK_LAUNCH_APP1         0xB6
	0,					// VK_LAUNCH_APP2         0xB7
	0,					// 0xB8 : reserved
	0,					// 0xB9 : reserved
	A3D_OEM_COLON,		// VK_OEM_1          0xBA   // ';:' for US
	A3D_OEM_PLUS,		// VK_OEM_PLUS       0xBB   // '+' any country
	A3D_OEM_COMMA,		// VK_OEM_COMMA      0xBC   // ',' any country
	A3D_OEM_MINUS,		// VK_OEM_MINUS      0xBD   // '-' any country
	A3D_OEM_PERIOD,		// VK_OEM_PERIOD     0xBE   // '.' any country
	A3D_OEM_SLASH,		// VK_OEM_2          0xBF   // '/?' for US
	A3D_OEM_TILDE,		// VK_OEM_3          0xC0   // '`~' for US
	0,					// 0xC1 : reserved
	0,					// 0xC2 : reserved
	0,					//VK_GAMEPAD_A                         0xC3 // reserved
	0,					//VK_GAMEPAD_B                         0xC4 // reserved
	0,					//VK_GAMEPAD_X                         0xC5 // reserved
	0,					//VK_GAMEPAD_Y                         0xC6 // reserved
	0,					//VK_GAMEPAD_RIGHT_SHOULDER            0xC7 // reserved
	0,					//VK_GAMEPAD_LEFT_SHOULDER             0xC8 // reserved
	0,					//VK_GAMEPAD_LEFT_TRIGGER              0xC9 // reserved
	0,					//VK_GAMEPAD_RIGHT_TRIGGER             0xCA // reserved
	0,					//VK_GAMEPAD_DPAD_UP                   0xCB // reserved
	0,					//VK_GAMEPAD_DPAD_DOWN                 0xCC // reserved
	0,					//VK_GAMEPAD_DPAD_LEFT                 0xCD // reserved
	0,					//VK_GAMEPAD_DPAD_RIGHT                0xCE // reserved
	0,					//VK_GAMEPAD_MENU                      0xCF // reserved
	0,					//VK_GAMEPAD_VIEW                      0xD0 // reserved
	0,					//VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON    0xD1 // reserved
	0,					//VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON   0xD2 // reserved
	0,					//VK_GAMEPAD_LEFT_THUMBSTICK_UP        0xD3 // reserved
	0,					//VK_GAMEPAD_LEFT_THUMBSTICK_DOWN      0xD4 // reserved
	0,					//VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT     0xD5 // reserved
	0,					//VK_GAMEPAD_LEFT_THUMBSTICK_LEFT      0xD6 // reserved
	0,					//VK_GAMEPAD_RIGHT_THUMBSTICK_UP       0xD7 // reserved
	0,					//VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN     0xD8 // reserved
	0,					//VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT    0xD9 // reserved
	0,					//VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT     0xDA // reserved
	A3D_OEM_OPEN,		// VK_OEM_4          0xDB  //  '[{' for US
	A3D_OEM_BACKSLASH,	// VK_OEM_5          0xDC  //  '\|' for US
	A3D_OEM_CLOSE,		// VK_OEM_6          0xDD  //  ']}' for US
	A3D_OEM_QUOTATION,	// VK_OEM_7          0xDE  //  ''"' for US
	0,					// VK_OEM_8          0xDF
	0,					// 0xE0 : reserved
	0,					// VK_OEM_AX         0xE1  //  'AX' key on Japanese AX kbd
	0,					// VK_OEM_102        0xE2  //  "<>" or "\|" on RT 102-key kbd.
	0,					// VK_ICO_HELP       0xE3  //  Help key on ICO
	0,					// VK_ICO_00         0xE4  //  00 key on ICO
	0,					// VK_PROCESSKEY     0xE5
	0,					// VK_ICO_CLEAR      0xE6
	0,					// VK_PACKET         0xE7
	0,					// * 0xE8 : unassigned
	0,					// VK_OEM_RESET      0xE9
	0,					// VK_OEM_JUMP       0xEA
	0,					// VK_OEM_PA1        0xEB
	0,					// VK_OEM_PA2        0xEC
	0,					// VK_OEM_PA3        0xED
	0,					// VK_OEM_WSCTRL     0xEE
	0,					// VK_OEM_CUSEL      0xEF
	0,					// VK_OEM_ATTN       0xF0
	0,					// VK_OEM_FINISH     0xF1
	0,					// VK_OEM_COPY       0xF2
	0,					// VK_OEM_AUTO       0xF3
	0,					// VK_OEM_ENLW       0xF4
	0,					// VK_OEM_BACKTAB    0xF5
	0,					// VK_ATTN           0xF6
	0,					// VK_CRSEL          0xF7
	0,					// VK_EXSEL          0xF8
	0,					// VK_EREOF          0xF9
	0,					// VK_PLAY           0xFA
	0,					// VK_ZOOM           0xFB
	0,					// VK_NONAME         0xFC
	0,					// VK_PA1            0xFD
	0,					// VK_OEM_CLEAR      0xFE
	0					// 0xFF
};

LRESULT WINAPI a3dWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
	int mi = 0;
	int rep = 1;

	switch (m)
	{
		case WM_CREATE:
			closing = false;
			track = false;
			mouse_b = 0;

			QueryPerformanceFrequency(&timer_freq);
			QueryPerformanceCounter(&coarse_perf);
			coarse_micro = 0;

			// set timer to refresh coarse_perf & coarse_micro every minute
			// to prevent overflows in a3dGetTIme 
			SetTimer(h, 1, 60000, 0);
			return TRUE;

		case WM_ENTERMENULOOP:
		{
			SetTimer(h, 2, 0, 0);
			break;
		}

		case WM_ENTERSIZEMOVE:
		{
			SetTimer(h, 3, 0, 0);
			break;
		}

		case WM_EXITSIZEMOVE:
		{
			KillTimer(h, 3);
			break;
		}

		case WM_EXITMENULOOP:
		{
			KillTimer(h, 2);
			break;
		}

		case WM_TIMER:
		{
			if (w == 1)
			{
				LARGE_INTEGER c;
				QueryPerformanceCounter(&c);
				uint64_t diff = c.QuadPart - coarse_perf.QuadPart;
				uint64_t seconds = diff / timer_freq.QuadPart;
				coarse_perf.QuadPart += seconds * timer_freq.QuadPart;
				coarse_micro += seconds * 1000000;
			}
			else
			if (w == 2 || w == 3)
			{
				if (platform_api.render)
					platform_api.render();
			}
			break;
		}
		
		case WM_CLOSE:
			if (platform_api.close)
				platform_api.close();
			else
			{
				memset(&platform_api, 0, sizeof(PlatformInterface));
				closing = true;
			}
			return 0;

		case WM_SIZE:
			if (platform_api.resize)
				platform_api.resize(LOWORD(l),HIWORD(l));
			return 0;

		case WM_ERASEBKGND:
			return 0;

		case WM_PAINT:
			ValidateRect(h, 0);
			return 0;

		case WM_MOUSEWHEEL:
			rep = GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA;
			if (rep > 0)
				mi = MouseInfo::WHEEL_UP;
			else
			if (rep < 0)
			{
				mi = MouseInfo::WHEEL_DN;
				rep = -rep;
			}
			else
				return 0;
			break;

		case WM_SETFOCUS:
			if (platform_api.keyb_focus)
				platform_api.keyb_focus(true);
			break;

		case WM_KILLFOCUS:
			if (platform_api.keyb_focus)
				platform_api.keyb_focus(false);
			break;

		case WM_CHAR:
			if (platform_api.keyb_char)
				platform_api.keyb_char((wchar_t)w);
			break;

		case WM_KEYDOWN:
		case WM_KEYUP:
			if (platform_api.keyb_key && w < 256)
			{
				KeyInfo ki = (KeyInfo)vk_to_ki[w];
				if (!ki)
					break;

				if ((l >> 24) & 1) // enh
				{
					if (ki == A3D_LSHIFT)
						ki = A3D_RSHIFT;
					else
					if (ki == A3D_LCTRL)
						ki = A3D_RCTRL;
					else
					if (ki == A3D_LALT)
						ki = A3D_RALT;
					else
					if (ki == A3D_ENTER)
						ki = A3D_NUMPAD_ENTER;
				}

				platform_api.keyb_key(ki, m == WM_KEYDOWN);
			}
			break;

		/*
		case WM_CAPTURECHANGED:
		{
		}
		*/

		case WM_MOUSELEAVE:
			track = false;
			if (platform_api.mouse)
				platform_api.mouse(mouse_x, mouse_y, MouseInfo::LEAVE);
			break;

		case WM_MOUSEMOVE:
			mouse_x = (short)LOWORD(l);
			mouse_y = (short)HIWORD(l);
			mi = MouseInfo::MOVE;
			if (!track)
			{
				mi = MouseInfo::ENTER;
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.dwHoverTime = HOVER_DEFAULT;
				tme.hwndTrack = h;
				TrackMouseEvent(&tme);
				track = true;
			}
			break;

		case WM_LBUTTONDOWN:
			mouse_b |= MouseInfo::LEFT;
			mi = MouseInfo::LEFT_DN;
			SetCapture(h);
			break;

		case WM_LBUTTONUP:
			mouse_b &= ~MouseInfo::LEFT;
			mi = MouseInfo::LEFT_UP;
			if (0 == (w & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)))
				ReleaseCapture();
			break;

		case WM_RBUTTONDOWN:
			mouse_b |= MouseInfo::RIGHT;
			mi = MouseInfo::RIGHT_DN;
			SetCapture(h);
			break;

		case WM_RBUTTONUP:
			mouse_b &= ~MouseInfo::RIGHT;
			mi = MouseInfo::RIGHT_UP;
			if (0 == (w & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)))
				ReleaseCapture();
			break;

		case WM_MBUTTONDOWN:
			mouse_b |= MouseInfo::MIDDLE;
			mi = MouseInfo::MIDDLE_DN;
			SetCapture(h);
			break;

		case WM_MBUTTONUP:
			mouse_b &= ~MouseInfo::MIDDLE;
			mi = MouseInfo::MIDDLE_UP;
			if (0 == (w & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)))
				ReleaseCapture();
			break;
	}

	if (mi && platform_api.mouse)
	{
		if (w & MK_LBUTTON)
			mi |= MouseInfo::LEFT;
		if (w & MK_RBUTTON)
			mi |= MouseInfo::RIGHT;
		if (w & MK_MBUTTON)
			mi |= MouseInfo::MIDDLE;

		for (int i=0; i<rep; i++)
			platform_api.mouse(mouse_x, mouse_y, (MouseInfo)mi);

		return 0;
	}

	return DefWindowProc(h, m, w, l);
}

#include <stdio.h>
/*
LONG WINAPI a3dExceptionProc(EXCEPTION_POINTERS *ExceptionInfo)
{
	printf("Exception has been ignored!\n");
	return EXCEPTION_CONTINUE_EXECUTION;
}
*/

// creates window & initialized GL
bool a3dOpen(const PlatformInterface* pi, const GraphicsDesc* gd/*, const AudioDesc* ad*/)
{
	if (!pi || !gd)
		return false;

	WNDCLASS wc;
	wc.style = 0;
	wc.lpfnWndProc = a3dWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetModuleHandle(0);
	wc.hIcon = LoadIcon(0,IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"A3DWNDCLASS";

	if (!RegisterClass(&wc))
		return false;

	memset(&platform_api, 0, sizeof(PlatformInterface));

	DWORD styles = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	styles |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;

	HWND h = CreateWindow(wc.lpszClassName, L"", styles,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0, wc.hInstance, 0);

	if (!h)
	{
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 0;
	}

	styles |= WS_POPUP; // add it later (hack to make CW_USEDEFAULT working)
	SetWindowLong(h, GWL_STYLE, styles);

	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags =
		PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL |
		gd->flags & GraphicsDesc::DOUBLE_BUFFER ? PFD_DOUBLEBUFFER : 0 |
		gd->depth_bits ? 0 : PFD_DEPTH_DONTCARE;

	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = gd->color_bits;
	pfd.cAlphaBits = gd->alpha_bits;
	pfd.cDepthBits = gd->depth_bits;
	pfd.cStencilBits = gd->stencil_bits;

	HDC dc = GetDC(h);
	int pfi = ChoosePixelFormat(dc, &pfd);
	if (!pfi)
	{
		ReleaseDC(h, dc);
		DestroyWindow(h);
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}

	if (!SetPixelFormat(dc, pfi, &pfd))
	{
		ReleaseDC(h, dc);
		DestroyWindow(h);
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}

	HGLRC rc = wglCreateContext(dc);
	if (!rc)
	{
		ReleaseDC(h, dc);
		DestroyWindow(h);
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}

	if (!wglMakeCurrent(dc, rc))
	{
		wglDeleteContext(rc);
		ReleaseDC(h, dc);
		DestroyWindow(h);
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}

	// 
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 0;
	wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)(wglGetProcAddress("wglCreateContextAttribsARB"));

	int  contextAttribs[] = {
		WGL_CONTEXT_FLAGS_ARB, gd->flags & GraphicsDesc::DEBUG_CONTEXT ? WGL_CONTEXT_DEBUG_BIT_ARB : 0,
		WGL_CONTEXT_MAJOR_VERSION_ARB, GALOGEN_API_VER_MAJ,
		WGL_CONTEXT_MINOR_VERSION_ARB, GALOGEN_API_VER_MIN,
		WGL_CONTEXT_PROFILE_MASK_ARB, strcmp(GALOGEN_API_PROFILE,"core")==0 ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : 0,
		0
	};

	wglMakeCurrent(0, 0);
	wglDeleteContext(rc);

	rc = wglCreateContextAttribsARB(dc, 0/*shareRC*/, contextAttribs);
	if (!rc)
	{
		ReleaseDC(h, dc);
		DestroyWindow(h);
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}

	if (!wglMakeCurrent(dc, rc))
	{
		wglDeleteContext(rc);
		ReleaseDC(h, dc);
		DestroyWindow(h);
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}

	platform_api = *pi;

	if (platform_api.init)
		platform_api.init();

	// send initial notifications:
	RECT r;
	GetClientRect(h, &r);
	if (platform_api.resize)
		platform_api.resize(r.right,r.bottom);

	// LPTOP_LEVEL_EXCEPTION_FILTER old_exception_proc = SetUnhandledExceptionFilter(a3dExceptionProc);

	while (!closing)
	{
		MSG msg;
		while (!closing && PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (platform_api.render)
			platform_api.render();
	}

	wglMakeCurrent(0, 0);
	wglDeleteContext(rc);
	ReleaseDC(h, dc);
	DestroyWindow(h);
	UnregisterClass(wc.lpszClassName, wc.hInstance);

	// SetUnhandledExceptionFilter(old_exception_proc);

	return true;
}

void a3dClose()
{
	memset(&platform_api, 0, sizeof(PlatformInterface));
	closing = true;
}

uint64_t a3dGetTime()
{
	LARGE_INTEGER c;
	QueryPerformanceCounter(&c);
	uint64_t diff = c.QuadPart - coarse_perf.QuadPart;
	return coarse_micro + diff * 1000000 / timer_freq.QuadPart;
	// we can handle diff upto 3 minutes @ 100GHz clock
	// this is why we refresh coarse time every minute on WM_TIMER
}

void a3dSwapBuffers()
{
	SwapBuffers(wglGetCurrentDC());
}

bool a3dGetKeyb(KeyInfo ki)
{
	return 0 != (1 & (GetKeyState(ki_to_vk[ki])>>15));
}

void a3dSetTitle(const wchar_t* name)
{
	HWND hWnd = WindowFromDC(wglGetCurrentDC());
	SetWindowText(hWnd, name);
}

int a3dGetTitle(wchar_t* name, int size)
{
	HWND hWnd = WindowFromDC(wglGetCurrentDC());
	if (name)
		GetWindowText(hWnd, name, size);
	return GetWindowTextLength(hWnd)+1;
}

void a3dSetVisible(bool visible)
{
	HWND hWnd = WindowFromDC(wglGetCurrentDC());
	ShowWindow(hWnd, visible ? SW_SHOW : SW_HIDE);
}

bool a3dGetVisible()
{
	HWND hWnd = WindowFromDC(wglGetCurrentDC());
	if (GetWindowLong(hWnd, GWL_STYLE) & WS_VISIBLE)
		return true;
	return false;
}

// resize
bool a3dGetRect(int* xywh)
{
	HWND hWnd = WindowFromDC(wglGetCurrentDC());
	RECT r;
	GetWindowRect(hWnd, &r);
	if (xywh)
	{
		xywh[0] = r.left;
		xywh[1] = r.top;
		xywh[2] = r.right - r.left;
		xywh[3] = r.bottom - r.top;
	}

	if (GetWindowLong(hWnd, GWL_STYLE) & WS_CAPTION)
		return true;
	return false;
}

void a3dSetRect(const int* xywh, bool wnd_mode)
{
	HWND hWnd = WindowFromDC(wglGetCurrentDC());
	DWORD s = GetWindowLong(hWnd, GWL_STYLE);
	DWORD ns = s;

	RECT r;
	RECT nr;
	GetWindowRect(hWnd, &r);

	nr.left = xywh[0];
	nr.top = xywh[1];
	nr.right = xywh[2] + xywh[0];
	nr.bottom = xywh[3] + xywh[1];

	if (wnd_mode)
		ns |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
	else
		ns &= ~(WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME);

	if (r.left != nr.left || r.top != nr.top ||
		r.right != nr.right || r.bottom != nr.bottom)
	{
		if (ns != s)
		{
			SetWindowLong(hWnd, GWL_STYLE, ns);
			SetWindowPos(hWnd, 0, nr.left, nr.top, nr.right - nr.left, nr.bottom - nr.top,
				SWP_NOZORDER | SWP_FRAMECHANGED | SWP_DRAWFRAME);
		}
		else
			SetWindowPos(hWnd, 0, nr.left, nr.top, nr.right - nr.left, nr.bottom - nr.top, SWP_NOZORDER);
	}
	else
	if (ns != s)
		SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_DRAWFRAME);
}


// mouse
MouseInfo a3dGetMouse(int* x, int* y) // returns but flags, mouse wheel has no state
{
	HWND hWnd = WindowFromDC(wglGetCurrentDC());
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(hWnd, &p);
	if (x)
		*x = p.x;
	if (y)
		*y = p.y;

	int fl = 0;

	if (0x8000 & GetKeyState(VK_LBUTTON))
		fl |= MouseInfo::LEFT;
	if (0x8000 & GetKeyState(VK_RBUTTON))
		fl |= MouseInfo::RIGHT;
	if (0x8000 & GetKeyState(VK_MBUTTON))
		fl |= MouseInfo::MIDDLE;

	return (MouseInfo)fl;
}

// keyb_key
bool a3dGetKeybKey(KeyInfo ki) // return true if vk is down, keyb_char has no state
{
	if (ki < 0 || ki >= A3D_MAPEND)
		return false;
	if (GetKeyState(ki_to_vk[ki]) & 0x8000)
		return true;
	return false;
}

// keyb_focus
bool a3dGetFocus()
{
	HWND hWnd = WindowFromDC(wglGetCurrentDC());
	return GetFocus() == hWnd;
}