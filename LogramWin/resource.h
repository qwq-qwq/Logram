#pragma once

#define IDR_MAINMENU        101
#define IDR_ACCEL           102
#define IDI_APPICON         103

// File menu
#define ID_FILE_OPEN        1001
#define ID_FILE_EXIT        1002

// View menu
#define ID_VIEW_DURATION    2001
#define ID_VIEW_STATS       2002
#define ID_VIEW_TIMING      2003
#define ID_THEME_TOKYONIGHT 2010
#define ID_THEME_TTY        2011

// Navigate menu
#define ID_NAV_FIND         3001
#define ID_NAV_FINDNEXT     3002
#define ID_NAV_FINDPREV     3003
#define ID_NAV_JUMPPAIR     3004
#define ID_NAV_NEXTERROR    3005
#define ID_NAV_PREVERROR    3006

// Help
#define ID_HELP_ABOUT       4001

// Custom messages
#define WM_APP_DOC_CHANGED  (WM_APP + 1)
#define WM_APP_DOC_LOADED   (WM_APP + 2)

// Level icons (RCDATA = raw PNG; decoded via WIC at runtime).
// NOTE: rc.exe does not evaluate parenthesized expressions in resource IDs,
// so these must be plain integer literals.
#define IDI_LEVEL_BASE      200
#define IDI_LEVEL_0         200
#define IDI_LEVEL_1         201
#define IDI_LEVEL_2         202
#define IDI_LEVEL_3         203
#define IDI_LEVEL_4         204
#define IDI_LEVEL_5         205
#define IDI_LEVEL_6         206
#define IDI_LEVEL_7         207
#define IDI_LEVEL_8         208
#define IDI_LEVEL_9         209
#define IDI_LEVEL_10        210
#define IDI_LEVEL_11        211
#define IDI_LEVEL_12        212
#define IDI_LEVEL_13        213
#define IDI_LEVEL_14        214
#define IDI_LEVEL_15        215
#define IDI_LEVEL_16        216
#define IDI_LEVEL_17        217
#define IDI_LEVEL_18        218
#define IDI_LEVEL_19        219
#define IDI_LEVEL_20        220
#define IDI_LEVEL_21        221
#define IDI_LEVEL_22        222
#define IDI_LEVEL_23        223
#define IDI_LEVEL_24        224
#define IDI_LEVEL_25        225
#define IDI_LEVEL_26        226
#define IDI_LEVEL_27        227
#define IDI_LEVEL_28        228
#define IDI_LEVEL_29        229
#define IDI_LEVEL_30        230
#define IDI_LEVEL_31        231

// UI icons (toolbar + detail panel)
#define IDI_UI_BASE         300
#define IDI_UI_CHEVUP       300
#define IDI_UI_CHEVDOWN     301
#define IDI_UI_CHEVUPERR    302
#define IDI_UI_CHEVDNERR    303
#define IDI_UI_SEARCH       304
#define IDI_UI_COPY         305
#define IDI_UI_EYE          306
#define IDI_UI_EYEOFF       307
#define IDI_UI_TIMING       308
#define IDI_UI_STATS        309
#define IDI_UI_OPENFILE     310
