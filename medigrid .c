/*******************************************************************************
* MEDIGRID QUANTUM - Premium Hospital Information System v7.0
* Redesigned UI: sidebar, dashboard cards, animated charts, patient cards,
* graphical bed layout, modern modals, hover effects, vector icons.
*
* Build (Windows / MinGW):
* gcc medigrid_premium.c -o medigrid.exe -lraylib -lopengl32 -lgdi32 -lwinmm
* Build (Linux):
* gcc medigrid_premium.c -o medigrid -lraylib -lm -lGL -lpthread -ldl -lrt -lX11
*
* Optional: put Inter-Regular.ttf / Inter-Bold.ttf in a "fonts" folder next to
* the exe for an even more premium look (system fonts are auto-detected too).
*******************************************************************************/
#include "raylib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define SCREEN_W 1280
#define SCREEN_H 800
#define SIDEBAR_W 240
#define CX 264 /* content left edge */
#define CW 996 /* content width */

#define MAX_BEDS 8
#define MAX_QUEUE 10
#define MAX_LOGS 14
#define MAX_BLOOD 4
#define MAX_DOCTORS 4
#define MAX_MEDS 4
#define MAX_BILLS 20

#define SET(dst, src) snprintf((dst), sizeof(dst), "%s", (src))
#define COL(r, g, b) ((Color){ r, g, b, 255 })

/* ------------------------------ PALETTE -------------------------------- */
#define C_BG COL(9, 13, 28)
#define C_SIDE COL(13, 19, 38)
#define C_CARD COL(19, 27, 52)
#define C_CARD_HI COL(27, 38, 72)
#define C_BORDER COL(34, 46, 84)
#define C_TEXT COL(232, 236, 247)
#define C_MUTED COL(124, 137, 171)
#define C_DIM COL(80, 93, 130)
#define C_ACCENT COL(99, 102, 241)
#define C_ACCENTL COL(129, 140, 248)
#define C_CYAN COL(34, 211, 238)
#define C_GREEN COL(52, 211, 153)
#define C_RED COL(251, 113, 133)
#define C_AMBER COL(251, 191, 36)
#define C_VIOLET COL(167, 139, 250)
#define C_TRACK COL(11, 16, 34)

/* ------------------------------- TYPES --------------------------------- */
typedef struct { int id; bool occupied; char patient[30]; char ward[20]; char doctor[30]; char vitals[32]; } Bed;
typedef struct { char name[30]; int priority; int wardPref; char doc[30]; } Patient;
typedef struct { char group[8]; int units; int cap; int price; } BloodStock;
typedef struct { char name[30]; char specialty[30]; bool onDuty; } Doctor;
typedef struct { char name[32]; int stock; int cap; int price; } Medicine;
typedef struct { char patient[30]; char item[35]; int units; int total; } BillRecord;
typedef struct { char text[90]; float t; Color col; } LogEntry;

enum { M_NONE = 0, M_REG, M_BLOOD, M_MED };

/* ------------------------------- STATE --------------------------------- */
static Bed beds[MAX_BEDS];
static Patient queue[MAX_QUEUE];
static int queueCount = 0;
static BloodStock blood[MAX_BLOOD];
static Doctor doctors[MAX_DOCTORS];
static Medicine pharmacy[MAX_MEDS];
static BillRecord bills[MAX_BILLS];
static int billCount = 0;
static LogEntry logs[MAX_LOGS];
static int logCount = 0;

static int registeredToday = 0, admittedToday = 0;
static int weeklyReg[7] = { 0, 0, 0, 0, 0, 0, 0 };
static int weeklyAdm[7] = { 0, 0, 0, 0, 0, 0, 0 };

static int tab = 0;
static float tabT = 0.0f;

static int modal = M_NONE;
static float modalAnim = 0.0f;
static char regName[30] = ""; static int regLen = 0; static int regPriority = 1, regWard = 0, regDoc = 0;
static char bName[30] = ""; static int bLen = 0; static int bGroup = 0, bUnits = 1;
static char mName[30] = ""; static int mLen = 0; static int mItem = 0, mUnits = 1;
static char searchQ[30] = ""; static int searchLen = 0;

static float disp[4] = { 0, 0, 0, 0 };

/* fonts / frame globals */
static Font gReg, gBold;
static bool gCustom = false;
static Vector2 gMouse;
static float gDt = 0.016f, gTime = 0.0f;
static bool uiBlocked = false, ignoreClick = false;
static float hovTab[2048];

static char toastText[96] = "";
static float toastT = 0.0f;
static Color toastCol;

/* ============================== UTILITIES ============================== */
static float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static float Lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float EaseOut(float t) { t = Clamp01(t); return 1.0f - powf(1.0f - t, 3.0f); }
static Rectangle R(float x, float y, float w, float h) { Rectangle r = { x, y, w, h }; return r; }
static Color Mix(Color a, Color b, float t) {
Color c;
c.r = (unsigned char)Lerpf(a.r, b.r, t); c.g = (unsigned char)Lerpf(a.g, b.g, t);
c.b = (unsigned char)Lerpf(a.b, b.b, t); c.a = (unsigned char)Lerpf(a.a, b.a, t);
return c;
}

/* ------------------------------- TEXT ---------------------------------- */
static float TSp(float size) { return gCustom ? size * 0.01f : size / 10.0f; }
static void Txt(const char *s, float x, float y, float size, Color c) { DrawTextEx(gReg, s, (Vector2){ x, y }, size, TSp(size), c); }
static void TxtB(const char *s, float x, float y, float size, Color c) { DrawTextEx(gBold, s, (Vector2){ x, y }, size, TSp(size), c); }
static float TxtW(const char *s, float size, bool bold) { return MeasureTextEx(bold ? gBold : gReg, s, size, TSp(size)).x; }
static void TxtAt(const char *s, float x, float y, float size, Color c, bool bold) { if (bold) TxtB(s, x, y, size, c); else Txt(s, x, y, size, c); }
static void TxtC(const char *s, float cx, float y, float size, Color c, bool bold) { TxtAt(s, cx - TxtW(s, size, bold) / 2, y, size, c, bold); }
static void TxtR(const char *s, float rx, float y, float size, Color c, bool bold) { TxtAt(s, rx - TxtW(s, size, bold), y, size, c, bold); }

static const char *Fit(const char *s, float maxW, float size, bool bold) {
static char buf[4][128]; static int n = 0;
char *b = buf[n = (n + 1) & 3];
snprintf(b, 128, "%s", s);
int len = (int)strlen(b);
while (len > 2 && TxtW(b, size, bold) > maxW) b[--len] = '\0';
if ((size_t)len < strlen(s) && len >= 2) { b[len - 1] = '.'; b[len - 2] = '.'; }
return b;
}

static bool TryLoadFont(const char **paths, int n, Font *out) {
for (int i = 0; i < n; i++) {
if (FileExists(paths[i])) {
Font f = LoadFontEx(paths[i], 40, NULL, 0);
if (f.texture.id > 0) { SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR); *out = f; return true; }
}
}
return false;
}

static void LoadFonts(void) {
const char *reg[] = { "fonts/Inter-Regular.ttf", "fonts/Poppins-Regular.ttf", "C:/Windows/Fonts/segoeui.ttf",
"/System/Library/Fonts/Supplemental/Arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf" };
const char *bold[] = { "fonts/Inter-Bold.ttf", "fonts/Poppins-Bold.ttf", "C:/Windows/Fonts/segoeuib.ttf",
"/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
"/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf" };
gReg = GetFontDefault(); gBold = gReg;
if (TryLoadFont(reg, 6, &gReg)) {
gCustom = true;
if (!TryLoadFont(bold, 6, &gBold)) gBold = gReg;
} else {
SetTextureFilter(gReg.texture, TEXTURE_FILTER_BILINEAR);
gBold = gReg;
}
}

/* ------------------------------ SHAPES --------------------------------- */
static void RRect(Rectangle r, float radius, Color c) {
float m = r.width < r.height ? r.width : r.height;
float rd = (m > 0) ? (radius * 2.0f / m) : 0;
if (rd > 1) rd = 1;
DrawRectangleRounded(r, rd, 12, c);
}
static void RRectB(Rectangle r, float radius, Color fill, Color border, float bw) {
RRect(r, radius, border);
RRect(R(r.x + bw, r.y + bw, r.width - 2 * bw, r.height - 2 * bw), fmaxf(radius - bw, 1), fill);
}
static void Bar(Rectangle r, float v, Color c) {
RRect(r, r.height / 2, C_TRACK);
if (v > 0.001f) { Rectangle f = r; f.width = fmaxf(r.height, r.width * Clamp01(v)); RRect(f, r.height / 2, c); }
}

/* ------------------------------- ICONS --------------------------------- */
/* 0 grid, 1 user, 2 bed, 3 drop, 4 pill, 5 bill, 6 search, 7 logs, 8 plus, 9 check, 11 arrow */
static void Icon(int t, float cx, float cy, float s, Color c) {
switch (t) {
case 0: { float q = s * 0.82f;
RRect(R(cx - s, cy - s, q, q), 2, c); RRect(R(cx + s - q, cy - s, q, q), 2, c);
RRect(R(cx - s, cy + s - q, q, q), 2, c); RRect(R(cx + s - q, cy + s - q, q, q), 2, Fade(c, 0.5f)); } break;
case 1:
DrawCircle((int)cx, (int)(cy - s * 0.5f), s * 0.42f, c);
DrawCircleSector((Vector2){ cx, cy + s }, s * 0.95f, 180, 360, 24, c); break;
case 2:
RRect(R(cx - s, cy, s * 2, s * 0.5f), 2, c); RRect(R(cx - s, cy - s * 0.7f, s * 0.25f, s * 1.2f), 1, c);
RRect(R(cx - s * 0.6f, cy - s * 0.35f, s * 0.6f, s * 0.3f), 2, Fade(c, 0.6f)); break;
case 3:
DrawCircle((int)cx, (int)(cy + s * 0.3f), s * 0.62f, c);
DrawTriangle((Vector2){ cx, cy - s }, (Vector2){ cx - s * 0.58f, cy + s * 0.05f }, (Vector2){ cx + s * 0.58f, cy + s * 0.05f }, c); break;
case 4:
RRect(R(cx - s, cy - s * 0.45f, s * 2, s * 0.9f), s * 0.45f, Fade(c, 0.45f));
RRect(R(cx - s, cy - s * 0.45f, s * 1.1f, s * 0.9f), s * 0.45f, c); break;
case 5:
RRect(R(cx - s * 0.7f, cy - s, s * 1.4f, s * 2), 3, c);
for (int i = 0; i < 3; i++) DrawRectangle((int)(cx - s * 0.4f), (int)(cy - s * 0.45f + i * s * 0.45f), (int)(s * 0.8f), 2, Fade(C_BG, 0.85f));
break;
case 6:
DrawRing((Vector2){ cx - s * 0.15f, cy - s * 0.15f }, s * 0.5f, s * 0.78f, 0, 360, 24, c);
DrawLineEx((Vector2){ cx + s * 0.4f, cy + s * 0.4f }, (Vector2){ cx + s * 0.95f, cy + s * 0.95f }, s * 0.28f, c); break;
case 7:
for (int i = 0; i < 3; i++) {
float yy = cy + (i - 1) * s * 0.65f;
DrawCircle((int)(cx - s * 0.75f), (int)yy, s * 0.14f, c);
RRect(R(cx - s * 0.45f, yy - s * 0.09f, (i == 2 ? 1.0f : 1.4f) * s, s * 0.18f), 1, c);
} break;
case 8:
RRect(R(cx - s * 0.9f, cy - s * 0.22f, s * 1.8f, s * 0.44f), 1, c);
RRect(R(cx - s * 0.22f, cy - s * 0.9f, s * 0.44f, s * 1.8f), 1, c); break;
case 9:
DrawLineEx((Vector2){ cx - s * 0.7f, cy + s * 0.05f }, (Vector2){ cx - s * 0.2f, cy + s * 0.6f }, s * 0.3f, c);
DrawLineEx((Vector2){ cx - s * 0.2f, cy + s * 0.6f }, (Vector2){ cx + s * 0.8f, cy - s * 0.6f }, s * 0.3f, c); break;
case 11:
DrawLineEx((Vector2){ cx - s * 0.9f, cy }, (Vector2){ cx + s * 0.8f, cy }, s * 0.3f, c);
DrawLineEx((Vector2){ cx + s * 0.8f, cy }, (Vector2){ cx + s * 0.2f, cy - s * 0.6f }, s * 0.3f, c);
DrawLineEx((Vector2){ cx + s * 0.8f, cy }, (Vector2){ cx + s * 0.2f, cy + s * 0.6f }, s * 0.3f, c); break;
default: break;
}
}

/* --------------------------- INTERACTION ------------------------------- */
static bool IsOver(Rectangle r) { return !uiBlocked && CheckCollisionPointRec(gMouse, r); }
static float HoverAnim(Rectangle r) {
unsigned int id = (((unsigned int)(int)r.x) * 73856093u ^ ((unsigned int)(int)r.y) * 19349663u) & 2047u;
float target = IsOver(r) ? 1.0f : 0.0f;
hovTab[id] += (target - hovTab[id]) * fminf(1.0f, gDt * 14.0f);
return hovTab[id];
}
static bool Clicked(Rectangle r) { return IsOver(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ignoreClick; }

/* card with hover glow + optional lift (lift=0 for cards containing buttons) */
static Rectangle CardBegin(Rectangle r, Color accent, float lift, float *hOut) {
float h = HoverAnim(r);
Rectangle d = r; d.y -= h * lift;
RRect(R(d.x + 2, d.y + 8, d.width - 4, d.height), 16, Fade(BLACK, 0.25f + 0.12f * h));
RRectB(d, 16, Mix(C_CARD, C_CARD_HI, h), Mix(C_BORDER, accent, h * 0.85f), 1);
if (hOut) *hOut = h;
return d;
}
static Rectangle CardPlain(Rectangle r) {
RRect(R(r.x + 2, r.y + 8, r.width - 4, r.height), 16, Fade(BLACK, 0.25f));
RRectB(r, 16, C_CARD, C_BORDER, 1);
return r;
}

static bool Btn(Rectangle r, const char *label, int icon, Color col, bool ghost) {
float h = HoverAnim(r);
bool down = IsOver(r) && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
Rectangle d = r; d.y -= h * 2.0f; if (down) d.y += 2.0f;
Color txt;
if (ghost) {
RRectB(d, 11, Mix(C_CARD, C_CARD_HI, h), Mix(C_BORDER, col, h), 1);
txt = Mix(C_MUTED, C_TEXT, h);
} else {
for (int k = 3; k >= 1; k--) RRect(R(d.x - k * 3, d.y - k * 3, d.width + k * 6, d.height + k * 6), 11 + k * 3, Fade(col, 0.07f * h));
RRect(d, 11, Mix(col, WHITE, h * 0.18f));
txt = WHITE;
}
float size = 13.0f, tw = TxtW(label, size, true), iw = (icon >= 0) ? 22.0f : 0.0f;
float sx = d.x + (d.width - tw - iw) / 2.0f;
if (icon >= 0) Icon(icon, sx + 8, d.y + d.height / 2.0f, 7, txt);
TxtB(label, sx + iw, d.y + (d.height - size) / 2.0f - 1, size, txt);
return Clicked(r);
}

static bool Chip(Rectangle r, const char *label, bool sel, Color col) {
float h = HoverAnim(r);
Color fill = sel ? Mix(C_CARD, col, 0.22f) : Mix(COL(24, 33, 62), C_CARD_HI, h);
Color bord = sel ? col : Mix(C_BORDER, C_MUTED, h);
RRectB(r, 10, fill, bord, sel ? 2 : 1);
TxtC(label, r.x + r.width / 2, r.y + (r.height - 13) / 2 - 1, 13, sel ? C_TEXT : Mix(C_MUTED, C_TEXT, h), sel);
return Clicked(r);
}

static void InputBox(Rectangle r, const char *text, const char *ph, Color accent) {
RRectB(r, 11, COL(14, 21, 43), accent, 2);
float ty = r.y + (r.height - 15) / 2 - 1, tx = r.x + 16;
float caretX = tx;
if (text[0]) { Txt(text, tx, ty, 15, C_TEXT); caretX = tx + TxtW(text, 15, false) + 2; }
else Txt(ph, tx, ty, 15, C_DIM);
if (((int)(gTime * 2.0f)) % 2 == 0) DrawRectangle((int)caretX, (int)r.y + 11, 2, (int)r.height - 22, accent);
}

static void TypeInto(char *buf, int *len, int maxLen) {
int k = GetCharPressed();
while (k > 0) {
if (k >= 32 && k <= 125 && *len < maxLen) { buf[*len] = (char)k; buf[*len + 1] = '\0'; (*len)++; }
k = GetCharPressed();
}
static float bsTimer = 0.0f;
if (IsKeyPressed(KEY_BACKSPACE)) { bsTimer = 0.4f; if (*len > 0) { (*len)--; buf[*len] = '\0'; } }
else if (IsKeyDown(KEY_BACKSPACE)) {
bsTimer -= gDt;
if (bsTimer <= 0) { bsTimer = 0.04f; if (*len > 0) { (*len)--; buf[*len] = '\0'; } }
}
}

static void Avatar(float cx, float cy, float rad, const char *name, Color c) {
DrawCircle((int)cx, (int)cy, rad + 3, Fade(c, 0.22f));
DrawCircle((int)cx, (int)cy, rad, Mix(c, C_BG, 0.55f));
if (strncmp(name, "Dr. ", 4) == 0) name += 4;
char ini[3] = { 0, 0, 0 };
ini[0] = (char)toupper((unsigned char)name[0]);
const char *sp = strchr(name, ' ');
if (sp && sp[1]) ini[1] = (char)toupper((unsigned char)sp[1]);
float sz = rad * 0.82f;
TxtC(ini, cx, cy - sz / 2 - 1, sz, C_TEXT, true);
}

static float Badge(const char *s, float x, float y, Color c) {
float w = TxtW(s, 11, true) + 20;
RRect(R(x, y, w, 21), 10.5f, Fade(c, 0.18f));
TxtB(s, x + 10, y + 4, 11, c);
return w;
}
static void Label(const char *s, float x, float y) { TxtB(s, x, y, 11, C_MUTED); }

static void DrawECG(float x, float y, float w, float h, Color c) {
Vector2 prev = { 0, 0 };
for (int i = 0; i <= (int)w; i += 2) {
float p = fmodf((float)i / w * 1.6f - gTime * 0.6f, 1.0f);
if (p < 0) p += 1.0f;
float v = 0;
if (p < 0.10f) v = 0;
else if (p < 0.16f) v = 0.15f * sinf((p - 0.10f) / 0.06f * PI);
else if (p < 0.30f) v = 0;
else if (p < 0.33f) v = -0.18f * (p - 0.30f) / 0.03f;
else if (p < 0.37f) v = -0.18f + (p - 0.33f) / 0.04f * 1.38f;
else if (p < 0.41f) v = 1.2f - (p - 0.37f) / 0.04f * 1.6f;
else if (p < 0.44f) v = -0.4f + (p - 0.41f) / 0.03f * 0.4f;
else if (p < 0.58f) v = 0;
else if (p < 0.70f) v = 0.25f * sinf((p - 0.58f) / 0.12f * PI);
Vector2 cur = { x + i, y + h * 0.6f - v * h * 0.5f };
if (i > 0) DrawLineEx(prev, cur, 1.6f, c);
prev = cur;
}
}

/* ---------------------------- LOGIC / DATA ------------------------------ */
static void SaveDatabase(void) {
FILE *f = fopen("medigrid_database.txt", "w");
if (!f) return;
fprintf(f, "=== MEDIGRID ENTERPRISE DATABASE BACKUP ===\n--- BILLING LEDGER ---\n");
for (int i = 0; i < billCount; i++)
fprintf(f, "%s | %s | %d Units | Rs.%d\n", bills[i].patient, bills[i].item, bills[i].units, bills[i].total);
fprintf(f, "--- ACTIVE BEDS STATUS ---\n");
for (int i = 0; i < MAX_BEDS; i++)
fprintf(f, "Bed #%d: %s [%s] - Doc: %s\n", beds[i].id, beds[i].occupied ? beds[i].patient : "Empty", beds[i].ward, beds[i].doctor);
fclose(f);
}

static void AddLog(const char *msg, Color c) {
if (logCount >= MAX_LOGS) { for (int i = 0; i < MAX_LOGS - 1; i++) logs[i] = logs[i + 1]; logCount = MAX_LOGS - 1; }
SET(logs[logCount].text, msg);
logs[logCount].t = (float)GetTime();
logs[logCount].col = c;
logCount++;
}
static void Toast(const char *msg, Color c) { SET(toastText, msg); toastT = 3.0f; toastCol = c; }

static void AddBill(const char *patient, const char *item, int units, int total) {
if (billCount >= MAX_BILLS) { for (int i = 0; i < MAX_BILLS - 1; i++) bills[i] = bills[i + 1]; billCount = MAX_BILLS - 1; }
SET(bills[billCount].patient, patient); SET(bills[billCount].item, item);
bills[billCount].units = units; bills[billCount].total = total; billCount++;
}

static void SetTab(int t) { tab = t; tabT = 0.0f; }
static void OpenModal(int m) { modal = m; modalAnim = 0.0f; ignoreClick = true; while (GetCharPressed() > 0) {} }

static void DoRegister(void) {
if (regLen == 0) { Toast("Enter the patient name first", C_AMBER); return; }
if (queueCount >= MAX_QUEUE) { Toast("Queue is full", C_RED); return; }
Patient *p = &queue[queueCount++];
SET(p->name, regName); p->priority = regPriority; p->wardPref = regWard; SET(p->doc, doctors[regDoc].name);
registeredToday++;
AddLog(TextFormat("Registered %s | %s", regName, doctors[regDoc].name), C_CYAN);
Toast(TextFormat("%s added to the queue", regName), C_GREEN);
regName[0] = '\0'; regLen = 0; modal = M_NONE;
}

static void DoAdmit(void) {
if (queueCount == 0) { Toast("Queue is empty", C_AMBER); return; }
int qi = 0;
for (int i = 0; i < queueCount; i++) if (queue[i].priority == 1) { qi = i; break; }
const char *want = (queue[qi].wardPref == 0) ? "ICU" : "General Ward";
int bed = -1;
for (int i = 0; i < MAX_BEDS; i++) if (!beds[i].occupied && strcmp(beds[i].ward, want) == 0) { bed = i; break; }
if (bed < 0) for (int i = 0; i < MAX_BEDS; i++) if (!beds[i].occupied) { bed = i; break; }
if (bed < 0) { Toast("No free beds available!", C_RED); return; }
Bed *b = &beds[bed];
b->occupied = true; SET(b->patient, queue[qi].name); SET(b->doctor, queue[qi].doc);
snprintf(b->vitals, sizeof(b->vitals), "BP %d/%d HR %d", GetRandomValue(110, 130), GetRandomValue(70, 85), GetRandomValue(66, 92));
char nm[30]; SET(nm, queue[qi].name);
for (int i = qi; i < queueCount - 1; i++) queue[i] = queue[i + 1];
queueCount--; admittedToday++;
SaveDatabase();
AddLog(TextFormat("Admitted %s to Bed #%d", nm, bed + 1), C_GREEN);
Toast(TextFormat("%s admitted to Bed #%d", nm, bed + 1), C_GREEN);
}

static void DoDischarge(int i) {
if (!beds[i].occupied) return;
char nm[30]; SET(nm, beds[i].patient);
beds[i].occupied = false; SET(beds[i].patient, "None"); SET(beds[i].doctor, "None"); SET(beds[i].vitals, "N/A");
SaveDatabase();
AddLog(TextFormat("Discharged %s from Bed #%d", nm, i + 1), C_AMBER);
Toast(TextFormat("%s discharged", nm), C_AMBER);
}

static void DoBlood(void) {
if (bLen == 0) { Toast("Enter the patient name first", C_AMBER); return; }
BloodStock *b = &blood[bGroup];
if (b->units < bUnits) { Toast(TextFormat("Only %d unit(s) of %s in stock", b->units, b->group), C_RED); return; }
b->units -= bUnits;
int total = bUnits * b->price;
AddBill(bName, TextFormat("Blood (%s)", b->group), bUnits, total);
SaveDatabase();
AddLog(TextFormat("Blood issued to %s | Rs.%d", bName, total), C_RED);
Toast(TextFormat("Blood issued - Bill Rs.%d", total), C_GREEN);
bName[0] = '\0'; bLen = 0; bUnits = 1; modal = M_NONE;
}

static void DoMed(void) {
if (mLen == 0) { Toast("Enter the patient name first", C_AMBER); return; }
Medicine *m = &pharmacy[mItem];
if (m->stock < mUnits) { Toast(TextFormat("Only %d unit(s) left in stock", m->stock), C_RED); return; }
m->stock -= mUnits;
int total = mUnits * m->price;
AddBill(mName, m->name, mUnits, total);
SaveDatabase();
AddLog(TextFormat("Dispensed %s to %s | Rs.%d", m->name, mName, total), C_AMBER);
Toast(TextFormat("Dispensed - Bill Rs.%d", total), C_GREEN);
mName[0] = '\0'; mLen = 0; mUnits = 1; modal = M_NONE;
}

static bool ContainsCI(const char *h, const char *n) {
size_t hl = strlen(h), nl = strlen(n);
if (nl == 0) return true;
for (size_t i = 0; i + nl <= hl; i++) {
size_t j;
for (j = 0; j < nl; j++) if (tolower((unsigned char)h[i + j]) != tolower((unsigned char)n[j])) break;
if (j == nl) return true;
}
return false;
}

static void InitData(void) {
for (int i = 0; i < MAX_BEDS; i++) {
beds[i].id = i + 1; beds[i].occupied = false;
SET(beds[i].patient, "None"); SET(beds[i].ward, i < 2 ? "ICU" : "General Ward");
SET(beds[i].doctor, "None"); SET(beds[i].vitals, "N/A");
}
struct { const char *g; int u, p; } bl[MAX_BLOOD] = { { "A+", 6, 1200 }, { "B+", 4, 1200 }, { "O-", 2, 1500 }, { "AB+", 5, 1400 } };
for (int i = 0; i < MAX_BLOOD; i++) { SET(blood[i].group, bl[i].g); blood[i].units = bl[i].u; blood[i].cap = 10; blood[i].price = bl[i].p; }
const char *dn[MAX_DOCTORS] = { "Dr. Rajesh Sharma", "Dr. Priya Verma", "Dr. Amit Roy", "Dr. Neha Gupta" };
const char *ds[MAX_DOCTORS] = { "ICU Specialist", "Emergency Trauma", "General Medicine", "Pediatrics" };
for (int i = 0; i < MAX_DOCTORS; i++) { SET(doctors[i].name, dn[i]); SET(doctors[i].specialty, ds[i]); doctors[i].onDuty = true; }
struct { const char *n; int s, c, p; } md[MAX_MEDS] = {
{ "Oxygen Cylinders", 25, 40, 2000 }, { "Paracetamol IV", 120, 200, 150 }, { "Insulin Vials", 40, 80, 450 }, { "Emergency Saline", 85, 120, 300 } };
for (int i = 0; i < MAX_MEDS; i++) { SET(pharmacy[i].name, md[i].n); pharmacy[i].stock = md[i].s; pharmacy[i].cap = md[i].c; pharmacy[i].price = md[i].p; }

AddLog("MediGrid system started. All records empty.", C_CYAN);
AddLog("File persistence & patient lookup engines ready.", C_GREEN);
AddLog("System ready for clinical operations.", C_MUTED);
}

/* ================================ PAGES ================================ */
static void DrawSeries(float ax, float ay, float aw, float ah, const int *data, float maxV, float e, Color col, bool fill, int hoverIdx) {
Vector2 p[7];
for (int i = 0; i < 7; i++) { p[i].x = ax + aw * i / 6.0f; p[i].y = ay + ah - (data[i] / maxV) * ah * e; }
if (fill) {
for (int i = 0; i < 6; i++)
for (float xx = p[i].x; xx < p[i + 1].x; xx += 3.0f) {
float t = (xx - p[i].x) / (p[i + 1].x - p[i].x);
float yy = Lerpf(p[i].y, p[i + 1].y, t);
DrawRectangleGradientV((int)xx, (int)yy, 3, (int)(ay + ah - yy) + 1, Fade(col, 0.30f), Fade(col, 0.0f));
}
}
for (int i = 0; i < 6; i++) DrawLineEx(p[i], p[i + 1], 3, col);
for (int i = 0; i < 7; i++) {
DrawCircleV(p[i], hoverIdx == i ? 7.0f : 5.0f, C_CARD);
DrawCircleV(p[i], hoverIdx == i ? 4.5f : 3.0f, col);
}
}

static void PageDashboard(float e) {
int occupied = 0, icuOcc = 0, wardOcc = 0, totalBlood = 0, capBlood = 0, onDuty = 0;
for (int i = 0; i < MAX_BEDS; i++) if (beds[i].occupied) { occupied++; if (i < 2) icuOcc++; else wardOcc++; }
for (int i = 0; i < MAX_BLOOD; i++) { totalBlood += blood[i].units; capBlood += blood[i].cap; }
for (int i = 0; i < MAX_DOCTORS; i++) if (doctors[i].onDuty) onDuty++;

/* ---- stat cards ---- */
const char *labels[4] = { "AVAILABLE BEDS", "PATIENTS", "DOCTORS ON DUTY", "BLOOD UNITS" };
Color cols[4] = { C_CYAN, C_VIOLET, C_GREEN, C_RED };
int icons[4] = { 2, 1, 8, 3 };
float prog[4] = { occupied / (float)MAX_BEDS, queueCount / (float)MAX_QUEUE, 1.0f, totalBlood / (float)capBlood };
for (int i = 0; i < 4; i++) {
float cw = (CW - 48) / 4.0f;
float h;
Rectangle d = CardBegin(R(CX + i * (cw + 16), 100, cw, 112), cols[i], 4, &h);
RRect(R(d.x + 18, d.y + 18, 42, 42), 13, Fade(cols[i], 0.16f));
Icon(icons[i], d.x + 39, d.y + 39, 9, cols[i]);
TxtB(labels[i], d.x + 72, d.y + 20, 10, C_MUTED);
TxtB(TextFormat("%d", (int)roundf(disp[i])), d.x + 72, d.y + 36, 30, C_TEXT);
const char *sub = "";
if (i == 0) sub = TextFormat("of %d beds | %d occupied", MAX_BEDS, occupied);
if (i == 1) sub = TextFormat("%d admitted | %d waiting", occupied, queueCount);
if (i == 2) sub = "all specialists active";
if (i == 3) sub = TextFormat("across %d blood groups", MAX_BLOOD);
Txt(sub, d.x + 18, d.y + 76, 12, C_MUTED);
Bar(R(d.x + 18, d.y + 98, d.width - 36, 6), prog[i] * e, cols[i]);
}

/* ---- patient statistics line chart ---- */
Rectangle lc = CardPlain(R(CX, 228, 620, 290));
TxtB("Patient Statistics", lc.x + 24, lc.y + 20, 17, C_TEXT);
Txt("Registrations vs admissions - last 7 days", lc.x + 24, lc.y + 44, 12, C_MUTED);
DrawCircle((int)(lc.x + lc.width - 190), (int)(lc.y + 30), 4, C_ACCENTL); Txt("Registered", lc.x + lc.width - 180, lc.y + 23, 12, C_MUTED);
DrawCircle((int)(lc.x + lc.width - 96), (int)(lc.y + 30), 4, C_CYAN); Txt("Admitted", lc.x + lc.width - 86, lc.y + 23, 12, C_MUTED);

weeklyReg[6] = registeredToday; weeklyAdm[6] = admittedToday;
float maxV = 40;
for (int i = 0; i < 7; i++) {
if (weeklyReg[i] > maxV) maxV = ceilf(weeklyReg[i] / 10.0f) * 10.0f;
if (weeklyAdm[i] > maxV) maxV = ceilf(weeklyAdm[i] / 10.0f) * 10.0f;
}
float ax = lc.x + 56, ay = lc.y + 84, aw = lc.width - 56 - 34, ah = 150;
for (int i = 0; i <= 4; i++) {
float gy = ay + ah * i / 4.0f;
DrawRectangle((int)ax, (int)gy, (int)aw, 1, Fade(C_BORDER, 0.8f));
TxtR(TextFormat("%d", (int)(maxV * (4 - i) / 4)), ax - 12, gy - 6, 10, C_DIM, false);
}
const char *days[7] = { "Wed", "Thu", "Fri", "Sat", "Sun", "Mon", "Today" };
for (int i = 0; i < 7; i++) TxtC(days[i], ax + aw * i / 6.0f, ay + ah + 14, 11, i == 6 ? C_TEXT : C_MUTED, i == 6);
int hi = -1;
if (!uiBlocked && gMouse.x > ax - 16 && gMouse.x < ax + aw + 16 && gMouse.y > ay - 30 && gMouse.y < ay + ah + 30) {
hi = (int)roundf((gMouse.x - ax) / (aw / 6.0f));
if (hi < 0) hi = 0; if (hi > 6) hi = 6;
DrawRectangle((int)(ax + aw * hi / 6.0f), (int)ay, 1, (int)ah, Fade(C_TEXT, 0.18f));
}
DrawSeries(ax, ay, aw, ah, weeklyReg, maxV, e, C_ACCENTL, true, hi);
DrawSeries(ax, ay, aw, ah, weeklyAdm, maxV, e, C_CYAN, false, hi);
if (hi >= 0) {
float bx = ax + aw * hi / 6.0f - 56; if (bx < lc.x + 8) bx = lc.x + 8; if (bx > lc.x + lc.width - 120) bx = lc.x + lc.width - 120;
RRectB(R(bx, ay - 10, 112, 46), 10, COL(10, 15, 34), C_BORDER, 1);
Txt(TextFormat("Registered %d", weeklyReg[hi]), bx + 12, ay - 2, 11, C_ACCENTL);
Txt(TextFormat("Admitted %d", weeklyAdm[hi]), bx + 12, ay + 16, 11, C_CYAN);
}

/* ---- bed occupancy donut ---- */
Rectangle dc = CardPlain(R(CX + 636, 228, 360, 290));
TxtB("Bed Occupancy", dc.x + 24, dc.y + 20, 17, C_TEXT);
Txt("Live facility utilisation", dc.x + 24, dc.y + 44, 12, C_MUTED);
Vector2 cen = { dc.x + dc.width / 2, dc.y + 148 };
float frac = occupied / (float)MAX_BEDS;
Color arc = frac > 0.85f ? C_RED : (frac > 0.6f ? C_AMBER : C_CYAN);
DrawRing(cen, 60, 78, 0, 360, 64, C_TRACK);
if (frac * e > 0.002f) DrawRing(cen, 60, 78, -90, -90 + 360.0f * frac * e, 64, arc);
TxtC(TextFormat("%d%%", (int)roundf(frac * 100 * e)), cen.x, cen.y - 20, 32, C_TEXT, true);
TxtC("OCCUPIED", cen.x, cen.y + 16, 10, C_MUTED, true);
DrawCircle((int)(dc.x + 40), (int)(dc.y + 252), 5, C_VIOLET);
Txt(TextFormat("ICU %d/2", icuOcc), dc.x + 54, dc.y + 245, 13, C_MUTED);
DrawCircle((int)(dc.x + 200), (int)(dc.y + 252), 5, C_CYAN);
Txt(TextFormat("Ward %d/%d", wardOcc, MAX_BEDS - 2), dc.x + 214, dc.y + 245, 13, C_MUTED);

/* ---- blood bars ---- */
Rectangle bc = CardPlain(R(CX, 534, 490, 246));
TxtB("Blood Bank Stock", bc.x + 24, bc.y + 20, 17, C_TEXT);
Txt("Units available per group", bc.x + 24, bc.y + 44, 12, C_MUTED);
for (int i = 0; i < MAX_BLOOD; i++) {
float ry = bc.y + 78 + i * 40;
Color c = blood[i].units <= 2 ? C_RED : (blood[i].units <= 4 ? C_AMBER : C_GREEN);
Icon(3, bc.x + 34, ry + 9, 8, c);
TxtB(blood[i].group, bc.x + 56, ry, 15, C_TEXT);
Bar(R(bc.x + 120, ry + 4, 270, 12), blood[i].units / (float)blood[i].cap * e, c);
TxtR(TextFormat("%d U", blood[i].units), bc.x + bc.width - 24, ry + 1, 14, C_TEXT, true);
}

/* ---- recent activity ---- */
Rectangle ac = CardPlain(R(CX + 506, 534, 490, 246));
TxtB("Recent Activity", ac.x + 24, ac.y + 20, 17, C_TEXT);
Txt("Latest system events", ac.x + 24, ac.y + 44, 12, C_MUTED);
for (int k = 0; k < 5 && k < logCount; k++) {
LogEntry *l = &logs[logCount - 1 - k];
float ry = ac.y + 76 + k * 33;
DrawCircle((int)(ac.x + 32), (int)(ry + 8), 4, l->col);
Txt(Fit(l->text, 330, 12, false), ac.x + 48, ry, 12, C_TEXT);
TxtR(TextFormat("%02d:%02d", (int)(l->t / 60), (int)l->t % 60), ac.x + ac.width - 24, ry + 1, 11, C_DIM, false);
}
}

static void PagePatients(float e) {
(void)e;
TxtB("WAITING QUEUE", CX, 102, 12, C_MUTED);
Badge(TextFormat("%d", queueCount), CX + 112, 99, C_AMBER);
if (queueCount == 0) {
Rectangle d = CardPlain(R(CX, 128, CW, 84));
TxtC("Queue is empty - click Register to add a patient", d.x + d.width / 2, d.y + 34, 14, C_DIM, false);
}
for (int i = 0; i < queueCount; i++) {
int col = i % 4, row = i / 4;
Color pc = queue[i].priority == 1 ? C_RED : C_AMBER;
float h;
Rectangle d = CardBegin(R(CX + col * 253, 128 + row * 96, 237, 84), pc, 4, &h);
Avatar(d.x + 36, d.y + 42, 20, queue[i].name, pc);
TxtB(Fit(queue[i].name, 130, 14, true), d.x + 66, d.y + 14, 14, C_TEXT);
Txt(Fit(queue[i].doc, 150, 11, false), d.x + 66, d.y + 34, 11, C_MUTED);
Badge(queue[i].priority == 1 ? "CRITICAL" : "OPD", d.x + 66, d.y + 54, pc);
TxtR(TextFormat("#%d", i + 1), d.x + d.width - 14, d.y + 12, 12, C_DIM, true);
}

int occ = 0; for (int i = 0; i < MAX_BEDS; i++) if (beds[i].occupied) occ++;
TxtB("ADMITTED PATIENTS", CX, 424, 12, C_MUTED);
Badge(TextFormat("%d", occ), CX + 142, 421, C_GREEN);
if (occ == 0) {
Rectangle d = CardPlain(R(CX, 452, CW, 100));
TxtC("No admitted patients yet - use Admit Next", d.x + d.width / 2, d.y + 40, 14, C_DIM, false);
}
int n = 0;
for (int i = 0; i < MAX_BEDS; i++) {
if (!beds[i].occupied) continue;
int col = n % 4, row = n / 4; n++;
float h;
Rectangle d = CardBegin(R(CX + col * 253, 452 + row * 162, 237, 150), C_GREEN, 4, &h);
Avatar(d.x + 36, d.y + 38, 22, beds[i].patient, C_GREEN);
TxtB(Fit(beds[i].patient, 150, 15, true), d.x + 68, d.y + 16, 15, C_TEXT);
Txt(TextFormat("Bed #%02d | %s", beds[i].id, beds[i].ward), d.x + 68, d.y + 40, 12, C_MUTED);
Icon(8, d.x + 24, d.y + 76, 5, C_CYAN);
Txt(Fit(beds[i].doctor, 180, 12, false), d.x + 38, d.y + 69, 12, C_CYAN);
Txt(beds[i].vitals, d.x + 18, d.y + 88, 12, C_AMBER);
DrawECG(d.x + 18, d.y + 108, d.width - 36, 30, C_GREEN);
}
}

static void DrawBedGfx(float cx, float cy, bool occ, Color col) {
Color fr = COL(52, 64, 110);
RRect(R(cx - 76, cy - 4, 152, 40), 8, fr);
DrawRectangle((int)cx - 70, (int)cy + 34, 8, 14, fr); DrawRectangle((int)cx + 62, (int)cy + 34, 8, 14, fr);
RRect(R(cx - 82, cy - 46, 12, 84), 5, COL(78, 92, 150));
RRect(R(cx - 68, cy - 16, 138, 32), 8, COL(36, 48, 86));
RRect(R(cx - 62, cy - 24, 40, 18), 8, COL(210, 218, 240));
Color sheet = occ ? Mix(col, C_BG, 0.25f) : COL(58, 74, 128);
RRect(R(cx - 26, cy - 20, 94, 34), 9, sheet);
RRect(R(cx - 26, cy - 20, 94, 8), 4, Mix(sheet, WHITE, 0.18f));
if (occ) DrawCircle((int)(cx - 42), (int)(cy - 18), 10, COL(250, 214, 178));
}

static void PageBeds(float e) {
(void)e;
for (int i = 0; i < MAX_BEDS; i++) {
int col = i % 4, row = i / 4;
bool occ = beds[i].occupied;
Color sc = occ ? C_RED : C_GREEN;
float h;
Rectangle d = CardBegin(R(CX + col * 253, 100 + row * 316, 237, 300), sc, 0, &h);
RRect(R(d.x + 14, d.y + 14, d.width - 28, 130), 12, COL(12, 18, 38));
DrawBedGfx(d.x + d.width / 2, d.y + 88, occ, C_RED);
Rectangle mon = R(d.x + d.width - 76, d.y + 24, 52, 32);
RRectB(mon, 5, COL(6, 10, 22), C_BORDER, 1);
if (occ) DrawECG(mon.x + 4, mon.y + 4, mon.width - 8, mon.height - 8, C_GREEN);
else DrawRectangle((int)mon.x + 4, (int)(mon.y + mon.height / 2), (int)mon.width - 8, 1, C_DIM);

TxtB(TextFormat("BED #%02d", beds[i].id), d.x + 18, d.y + 158, 17, C_TEXT);
float bw = TxtW(occ ? "OCCUPIED" : "AVAILABLE", 11, true) + 20;
Badge(occ ? "OCCUPIED" : "AVAILABLE", d.x + d.width - 18 - bw, d.y + 160, sc);
Txt(i < 2 ? "ICU - Critical Care" : "General Ward", d.x + 18, d.y + 184, 12, i < 2 ? C_VIOLET : C_MUTED);
if (occ) {
TxtB(Fit(beds[i].patient, 200, 14, true), d.x + 18, d.y + 208, 14, C_TEXT);
Txt(Fit(beds[i].doctor, 200, 12, false), d.x + 18, d.y + 230, 12, C_CYAN);
Txt(beds[i].vitals, d.x + 18, d.y + 248, 12, C_AMBER);
if (Btn(R(d.x + 18, d.y + 268, d.width - 36, 24), "Discharge", -1, COL(190, 48, 80), false)) DoDischarge(i);
} else {
Txt("Ready for allocation", d.x + 18, d.y + 208, 13, C_GREEN);
Txt("Use Admit Next to assign a patient", d.x + 18, d.y + 232, 11, C_DIM);
}
}
DrawCircle(CX + 6, 740, 5, C_GREEN); Txt("Available", CX + 20, 733, 12, C_MUTED);
DrawCircle(CX + 106, 740, 5, C_RED); Txt("Occupied", CX + 120, 733, 12, C_MUTED);
Txt("Hover a bed card for details | [D] discharges the first occupied bed", CX + 220, 733, 12, C_DIM);
}

static void PageBloodDoc(float e) {
for (int i = 0; i < MAX_BLOOD; i++) {
int col = i % 2, row = i / 2;
Color c = blood[i].units <= 2 ? C_RED : (blood[i].units <= 4 ? C_AMBER : C_GREEN);
float h;
Rectangle d = CardBegin(R(CX + col * 253, 100 + row * 342, 237, 326), c, 4, &h);
RRect(R(d.x + 18, d.y + 22, 44, 44), 13, Fade(C_RED, 0.16f));
Icon(3, d.x + 40, d.y + 44, 10, C_RED);
TxtB(blood[i].group, d.x + 18, d.y + 84, 36, C_TEXT);
Txt(TextFormat("Rs.%d per unit", blood[i].price), d.x + 18, d.y + 130, 12, C_MUTED);
Rectangle tube = R(d.x + d.width - 62, d.y + 24, 30, 200);
RRect(tube, 15, C_TRACK);
float lv = blood[i].units / (float)blood[i].cap * e;
float lh = fmaxf(30, tube.height * Clamp01(lv));
RRect(R(tube.x, tube.y + tube.height - lh, tube.width, lh), 15, c);
TxtB("UNITS AVAILABLE", d.x + 18, d.y + 244, 10, C_MUTED);
TxtB(TextFormat("%d", blood[i].units), d.x + 18, d.y + 260, 34, c);
TxtB("U", d.x + 18 + TxtW(TextFormat("%d", blood[i].units), 34, true) + 6, d.y + 276, 14, C_MUTED);
Badge(blood[i].units <= 2 ? "LOW STOCK" : (blood[i].units <= 4 ? "MODERATE" : "HEALTHY"), d.x + 18, d.y + 298, c);
}
float pulse = 0.5f + 0.5f * sinf(gTime * 3.0f);
for (int i = 0; i < MAX_DOCTORS; i++) {
int active = 0;
for (int b = 0; b < MAX_BEDS; b++) if (beds[b].occupied && strcmp(beds[b].doctor, doctors[i].name) == 0) active++;
for (int q = 0; q < queueCount; q++) if (strcmp(queue[q].doc, doctors[i].name) == 0) active++;
float h;
Rectangle d = CardBegin(R(CX + 506, 100 + i * 171, 490, 155), C_CYAN, 4, &h);
Avatar(d.x + 56, d.y + 62, 32, doctors[i].name, C_CYAN);
TxtB(doctors[i].name, d.x + 108, d.y + 32, 19, C_TEXT);
Txt(doctors[i].specialty, d.x + 108, d.y + 60, 13, C_CYAN);
DrawCircle((int)(d.x + 118), (int)(d.y + 106), 4 + pulse * 2.5f, Fade(C_GREEN, 0.25f));
DrawCircle((int)(d.x + 118), (int)(d.y + 106), 3.5f, C_GREEN);
Badge("ON DUTY", d.x + 132, d.y + 96, C_GREEN);
TxtR(TextFormat("%d", active), d.x + d.width - 28, d.y + 34, 34, C_TEXT, true);
TxtR("ACTIVE PATIENTS", d.x + d.width - 28, d.y + 76, 10, C_MUTED, true);
}
}

static void PagePharmacy(float e) {
for (int i = 0; i < MAX_MEDS; i++) {
int col = i % 2, row = i / 2;
Medicine *m = &pharmacy[i];
float frac = m->stock / (float)m->cap;
Color c = frac < 0.25f ? C_RED : (frac < 0.5f ? C_AMBER : C_GREEN);
float h;
Rectangle d = CardBegin(R(CX + col * 506, 100 + row * 346, 490, 330), C_AMBER, 0, &h);
RRect(R(d.x + 24, d.y + 24, 58, 58), 16, Fade(C_AMBER, 0.16f));
if (i == 0) { DrawRing((Vector2){ d.x + 53, d.y + 53 }, 15, 20, 0, 360, 32, C_AMBER); TxtC("O2", d.x + 53, d.y + 46, 14, C_AMBER, true); }
else Icon(4, d.x + 53, d.y + 53, 14, C_AMBER);
TxtB(m->name, d.x + 100, d.y + 30, 21, C_TEXT);
Txt(TextFormat("Rs.%d per unit", m->price), d.x + 100, d.y + 60, 13, C_AMBER);
TxtB(TextFormat("%d", (int)roundf(m->stock * e)), d.x + 24, d.y + 110, 58, c);
Txt("units in stock", d.x + 24, d.y + 178, 13, C_MUTED);
Bar(R(d.x + 24, d.y + 208, d.width - 48, 12), frac * e, c);
Txt(TextFormat("Capacity %d", m->cap), d.x + 24, d.y + 228, 11, C_DIM);
TxtR(TextFormat("Stock value Rs.%d", m->stock * m->price), d.x + d.width - 24, d.y + 228, 11, C_MUTED, false);
Badge(frac < 0.25f ? "LOW" : (frac < 0.5f ? "MEDIUM" : "WELL STOCKED"), d.x + d.width - 24 - (TxtW(frac < 0.25f ? "LOW" : (frac < 0.5f ? "MEDIUM" : "WELL STOCKED"), 11, true) + 20), d.y + 34, c);
if (Btn(R(d.x + 24, d.y + 266, d.width - 48, 42), TextFormat("Dispense %s", m->name), 4, COL(180, 100, 10), false)) { mItem = i; OpenModal(M_MED); }
}
}

static void PageBilling(float e) {
int revenue = 0;
for (int i = 0; i < billCount; i++) revenue += bills[i].total;
const char *lab[3] = { "TOTAL REVENUE", "ITEMS BILLED", "AVERAGE BILL" };
Color cc[3] = { C_GREEN, C_VIOLET, C_AMBER };
for (int i = 0; i < 3; i++) {
float h;
Rectangle d = CardBegin(R(CX + i * 337, 100, 322, 84), cc[i], 4, &h);
RRect(R(d.x + 18, d.y + 20, 44, 44), 13, Fade(cc[i], 0.16f));
Icon(5, d.x + 40, d.y + 42, 10, cc[i]);
TxtB(lab[i], d.x + 78, d.y + 18, 10, C_MUTED);
if (i == 0) TxtB(TextFormat("Rs. %d", (int)(revenue * e)), d.x + 78, d.y + 36, 26, C_TEXT);
if (i == 1) TxtB(TextFormat("%d", (int)(billCount * e)), d.x + 78, d.y + 36, 26, C_TEXT);
if (i == 2) TxtB(TextFormat("Rs. %d", billCount ? (int)(revenue / billCount * e) : 0), d.x + 78, d.y + 36, 26, C_TEXT);
}
Rectangle tc = CardPlain(R(CX, 200, CW, 580));
RRect(R(tc.x + 16, tc.y + 16, tc.width - 32, 34), 9, COL(14, 21, 43));
TxtB("PATIENT", tc.x + 32, tc.y + 27, 11, C_ACCENTL);
TxtB("ITEM / BLOOD", tc.x + 310, tc.y + 27, 11, C_ACCENTL);
TxtB("UNITS", tc.x + 640, tc.y + 27, 11, C_ACCENTL);
TxtR("TOTAL (Rs.)", tc.x + tc.width - 32, tc.y + 27, 11, C_ACCENTL, true);
if (billCount == 0) TxtC("No bills yet - issue blood or dispense medicines", tc.x + tc.width / 2, tc.y + 120, 14, C_DIM, false);
for (int i = 0; i < billCount; i++) {
float ry = tc.y + 58 + i * 25;
if (i % 2 == 0) RRect(R(tc.x + 16, ry - 2, tc.width - 32, 24), 6, Fade(C_CARD_HI, 0.55f));
Txt(bills[i].patient, tc.x + 32, ry + 3, 13, C_TEXT);
Txt(bills[i].item, tc.x + 310, ry + 3, 13, C_AMBER);
Txt(TextFormat("%d U", bills[i].units), tc.x + 640, ry + 3, 13, C_CYAN);
TxtR(TextFormat("%d", bills[i].total), tc.x + tc.width - 32, ry + 3, 13, C_GREEN, true);
}
}

static void ResultRow(int idx, const char *tag, Color c, const char *title, const char *detail) {
float h;
Rectangle d = CardBegin(R(CX, 222 + idx * 72, CW, 62), c, 3, &h);
Avatar(d.x + 38, d.y + 31, 18, title, c);
Badge(tag, d.x + 76, d.y + 10, c);
TxtB(Fit(title, 280, 14, true), d.x + 76 + TxtW(tag, 11, true) + 34, d.y + 10, 14, C_TEXT);
Txt(Fit(detail, CW - 120, 12, false), d.x + 76, d.y + 36, 12, C_MUTED);
}

static void PageSearch(float e) {
(void)e;
Rectangle sc = CardPlain(R(CX, 100, CW, 106));
Label("SEARCH PATIENT BY NAME", sc.x + 24, sc.y + 16);
Icon(6, sc.x + sc.width - 44, sc.y + 62, 11, C_DIM);
InputBox(R(sc.x + 24, sc.y + 38, sc.width - 48, 50), searchQ, "Start typing a patient name...", C_ACCENT);
if (searchLen == 0) {
Rectangle d = CardPlain(R(CX, 222, CW, 200));
Icon(6, d.x + d.width / 2, d.y + 70, 22, C_DIM);
TxtC("Search across beds, queue and billing ledger", d.x + d.width / 2, d.y + 118, 15, C_MUTED, true);
TxtC("Results appear instantly as you type", d.x + d.width / 2, d.y + 144, 12, C_DIM, false);
return;
}
int shown = 0, total = 0;
for (int i = 0; i < MAX_BEDS; i++) if (beds[i].occupied && ContainsCI(beds[i].patient, searchQ)) {
total++;
if (shown < 7) { ResultRow(shown, "BED", C_GREEN, beds[i].patient, TextFormat("Bed #%d (%s) | %s | %s", beds[i].id, beds[i].ward, beds[i].doctor, beds[i].vitals)); shown++; }
}
for (int i = 0; i < queueCount; i++) if (ContainsCI(queue[i].name, searchQ)) {
total++;
if (shown < 7) { ResultRow(shown, "QUEUE", C_AMBER, queue[i].name, TextFormat("Waiting #%d | %s | %s", i + 1, queue[i].priority == 1 ? "Critical" : "OPD", queue[i].doc)); shown++; }
}
for (int i = 0; i < billCount; i++) if (ContainsCI(bills[i].patient, searchQ)) {
total++;
if (shown < 7) { ResultRow(shown, "BILL", C_VIOLET, bills[i].patient, TextFormat("%s | %d unit(s) | Rs.%d", bills[i].item, bills[i].units, bills[i].total)); shown++; }
}
if (total == 0) {
Rectangle d = CardPlain(R(CX, 222, CW, 120));
TxtC("No records match this name", d.x + d.width / 2, d.y + 46, 15, C_RED, true);
} else if (total > shown) {
Txt(TextFormat("+ %d more result(s) - refine your search", total - shown), CX + 6, 222 + shown * 72 + 6, 12, C_MUTED);
}
}

static void PageLogs(float e) {
Rectangle lc = CardPlain(R(CX, 100, CW, 680));
DrawRectangle((int)lc.x + 39, (int)lc.y + 34, 2, (int)(lc.height - 68), C_BORDER);
if (logCount == 0) TxtC("No events yet", lc.x + lc.width / 2, lc.y + 60, 14, C_DIM, false);
for (int k = 0; k < logCount; k++) {
LogEntry *l = &logs[logCount - 1 - k];
float ry = lc.y + 30 + k * 46;
float vis = EaseOut(e * 1.6f - k * 0.06f);
if (vis <= 0.01f) continue;
DrawCircle((int)(lc.x + 40), (int)(ry + 9), 9, Fade(l->col, 0.2f * vis));
DrawCircle((int)(lc.x + 40), (int)(ry + 9), 5, l->col);
TxtB(TextFormat("LOG-%02d", logCount - k), lc.x + 70, ry + 1, 12, l->col);
Txt(Fit(l->text, 640, 14, false), lc.x + 150, ry, 14, C_TEXT);
TxtR(TextFormat("T+%02d:%02d", (int)(l->t / 60), (int)l->t % 60), lc.x + lc.width - 28, ry + 2, 12, C_DIM, false);
}
}

/* ================================ MODALS =============================== */
static void DrawModals(void) {
if (modal == M_NONE) return;
uiBlocked = false;
float t = EaseOut(modalAnim);
DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(COL(3, 6, 16), 0.78f * t));
Rectangle box = R((SCREEN_W - 520) / 2.0f, (SCREEN_H - 510) / 2.0f + (1 - t) * 28, 520, 510);
Color acc = modal == M_REG ? C_ACCENT : (modal == M_BLOOD ? C_RED : C_AMBER);
for (int k = 4; k >= 1; k--) RRect(R(box.x - k * 5, box.y - k * 5 + 10, box.width + k * 10, box.height + k * 10), 22 + k * 5, Fade(acc, 0.035f));
RRectB(box, 22, COL(16, 23, 46), Mix(C_BORDER, acc, 0.6f), 1);

const char *title = modal == M_REG ? "Patient Registration" : (modal == M_BLOOD ? "Blood Issuance" : "Pharmacy Dispense");
const char *sub = modal == M_REG ? "Add a patient to the triage queue" : (modal == M_BLOOD ? "Issue blood units and generate a bill" : "Dispense medicine / oxygen and bill the patient");
int ic = modal == M_REG ? 1 : (modal == M_BLOOD ? 3 : 4);
RRect(R(box.x + 28, box.y + 26, 46, 46), 14, Fade(acc, 0.18f));
Icon(ic, box.x + 51, box.y + 49, 10, acc);
TxtB(title, box.x + 88, box.y + 28, 21, C_TEXT);
Txt(sub, box.x + 88, box.y + 56, 12, C_MUTED);
if (Btn(R(box.x + box.width - 60, box.y + 28, 32, 32), "x", -1, C_RED, true)) modal = M_NONE;

float bx = box.x + 28, by = box.y, bw = 464;
Label("PATIENT NAME", bx, by + 92);

if (modal == M_REG) {
InputBox(R(bx, by + 110, bw, 44), regName, "e.g. Rahul Sharma", acc);
Label("TRIAGE PRIORITY", bx, by + 170);
if (Chip(R(bx, by + 188, 226, 38), "Critical Emergency", regPriority == 1, C_RED)) regPriority = 1;
if (Chip(R(bx + 238, by + 188, 226, 38), "General OPD", regPriority == 2, C_AMBER)) regPriority = 2;
Label("WARD PREFERENCE", bx, by + 240);
if (Chip(R(bx, by + 258, 226, 38), "ICU / Critical Care", regWard == 0, C_VIOLET)) regWard = 0;
if (Chip(R(bx + 238, by + 258, 226, 38), "General Ward", regWard == 1, C_CYAN)) regWard = 1;
Label("ASSIGN SPECIALIST", bx, by + 310);
for (int d = 0; d < MAX_DOCTORS; d++)
if (Chip(R(bx + (d % 2) * 238, by + 328 + (d / 2) * 44, 226, 38), doctors[d].name, regDoc == d, C_GREEN)) regDoc = d;
if (Btn(R(bx, by + 442, 150, 44), "Cancel", -1, C_MUTED, true)) modal = M_NONE;
if (Btn(R(bx + 164, by + 442, 300, 44), "Confirm Registration", 9, C_ACCENT, false)) DoRegister();
}
else if (modal == M_BLOOD) {
InputBox(R(bx, by + 110, bw, 44), bName, "Patient receiving blood", acc);
Label("BLOOD GROUP", bx, by + 170);
for (int g = 0; g < MAX_BLOOD; g++)
if (Chip(R(bx + g * 118, by + 188, 110, 44), TextFormat("%s (%d)", blood[g].group, blood[g].units), bGroup == g, C_RED)) bGroup = g;
Txt(TextFormat("Rs.%d per unit | %d unit(s) available", blood[bGroup].price, blood[bGroup].units), bx, by + 246, 13, C_MUTED);
Label("UNITS", bx, by + 278);
if (Btn(R(bx, by + 296, 44, 40), "-", -1, C_MUTED, true) && bUnits > 1) bUnits--;
RRectB(R(bx + 54, by + 296, 100, 40), 11, COL(14, 21, 43), C_BORDER, 1);
TxtC(TextFormat("%d", bUnits), bx + 104, by + 306, 18, C_TEXT, true);
if (Btn(R(bx + 164, by + 296, 44, 40), "+", -1, C_MUTED, true) && bUnits < 5) bUnits++;
Txt("max 5 per issue", bx + 224, by + 308, 12, C_DIM);
RRectB(R(bx, by + 356, bw, 60), 14, COL(12, 18, 38), C_BORDER, 1);
TxtB("TOTAL BILL", bx + 20, by + 379, 11, C_MUTED);
TxtR(TextFormat("Rs. %d", bUnits * blood[bGroup].price), bx + bw - 20, by + 372, 24, C_GREEN, true);
if (Btn(R(bx, by + 442, 150, 44), "Cancel", -1, C_MUTED, true)) modal = M_NONE;
if (Btn(R(bx + 164, by + 442, 300, 44), "Issue & Generate Bill", 9, COL(225, 29, 72), false)) DoBlood();
}
else {
InputBox(R(bx, by + 110, bw, 44), mName, "Patient receiving medicine", acc);
Label("SELECT ITEM", bx, by + 170);
for (int m = 0; m < MAX_MEDS; m++)
if (Chip(R(bx + (m % 2) * 238, by + 188 + (m / 2) * 48, 226, 40), pharmacy[m].name, mItem == m, C_AMBER)) mItem = m;
Txt(TextFormat("Rs.%d per unit | %d in stock", pharmacy[mItem].price, pharmacy[mItem].stock), bx, by + 286, 13, C_MUTED);
Label("QUANTITY", bx, by + 312);
if (Btn(R(bx, by + 330, 44, 38), "-", -1, C_MUTED, true) && mUnits > 1) mUnits--;
RRectB(R(bx + 54, by + 330, 100, 38), 11, COL(14, 21, 43), C_BORDER, 1);
TxtC(TextFormat("%d", mUnits), bx + 104, by + 339, 18, C_TEXT, true);
if (Btn(R(bx + 164, by + 330, 44, 38), "+", -1, C_MUTED, true) && mUnits < 10) mUnits++;
Txt("max 10 per dispense", bx + 224, by + 341, 12, C_DIM);
RRectB(R(bx, by + 378, bw, 52), 14, COL(12, 18, 38), C_BORDER, 1);
TxtB("TOTAL BILL", bx + 20, by + 398, 11, C_MUTED);
TxtR(TextFormat("Rs. %d", mUnits * pharmacy[mItem].price), bx + bw - 20, by + 392, 22, C_GREEN, true);
if (Btn(R(bx, by + 442, 150, 44), "Cancel", -1, C_MUTED, true)) modal = M_NONE;
if (Btn(R(bx + 164, by + 442, 300, 44), "Dispense & Bill", 9, COL(180, 100, 10), false)) DoMed();
}
}

static void DrawToast(void) {
if (toastT <= 0) return;
float enter = EaseOut((3.0f - toastT) / 0.3f), exitT = Clamp01(toastT / 0.35f);
float w = TxtW(toastText, 14, true) + 78, hgt = 52;
float x = SCREEN_W - 24 - w + (1.0f - enter * exitT) * (w + 40);
float y = SCREEN_H - 24 - hgt;
RRect(R(x + 2, y + 8, w - 4, hgt), 14, Fade(BLACK, 0.4f));
RRectB(R(x, y, w, hgt), 14, COL(16, 23, 46), toastCol, 1);
DrawCircle((int)(x + 28), (int)(y + hgt / 2), 12, Fade(toastCol, 0.2f));
Icon(9, x + 28, y + hgt / 2, 6, toastCol);
TxtB(toastText, x + 54, y + (hgt - 14) / 2 - 1, 14, C_TEXT);
}

/* ================================= MAIN ================================ */
int main(void) {
SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
InitWindow(SCREEN_W, SCREEN_H, "MediGrid Quantum - Premium HIS Suite v7.0");
SetExitKey(KEY_NULL); /* ESC closes popups, not the app */
SetTargetFPS(60);
LoadFonts();
InitData();

const char *navNames[8] = { "Dashboard", "Patients", "Beds", "Blood & Doctors", "Pharmacy", "Billing", "Search", "Audit Logs" };
int navIcons[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
const char *titles[8] = { "Command Center", "Patient Management", "Bed Visualization", "Blood Bank & Specialists", "Pharmacy & Oxygen", "Billing Ledger", "Patient Search", "System Audit Logs" };
const char *subs[8] = {
"Live overview of beds, patients, doctors and blood stock", "Triage queue and admitted patient profiles",
"Graphical ward layout with live bed status", "Stock levels and on-duty roster",
"Medicine and oxygen cylinder inventory", "Itemised billing history", "Find any patient across all records", "Real-time system event timeline" };
float indicatorY = 124;

while (!WindowShouldClose()) {
gDt = GetFrameTime(); gTime = (float)GetTime(); gMouse = GetMousePosition();
ignoreClick = false;

/* ------------- input ------------- */
if (modal == M_REG) {
TypeInto(regName, &regLen, 28);
if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) regPriority = (regPriority == 1) ? 2 : 1;
if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) regWard = 1 - regWard;
if (IsKeyPressed(KEY_TAB)) regDoc = (regDoc + 1) % MAX_DOCTORS;
if (IsKeyPressed(KEY_ENTER)) DoRegister();
if (IsKeyPressed(KEY_ESCAPE)) modal = M_NONE;
} else if (modal == M_BLOOD) {
TypeInto(bName, &bLen, 28);
if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) bGroup = (bGroup + 1) % MAX_BLOOD;
if (IsKeyPressed(KEY_RIGHT) && bUnits < 5) bUnits++;
if (IsKeyPressed(KEY_LEFT) && bUnits > 1) bUnits--;
if (IsKeyPressed(KEY_ENTER)) DoBlood();
if (IsKeyPressed(KEY_ESCAPE)) modal = M_NONE;
} else if (modal == M_MED) {
TypeInto(mName, &mLen, 28);
if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) mItem = (mItem + 1) % MAX_MEDS;
if (IsKeyPressed(KEY_RIGHT) && mUnits < 10) mUnits++;
if (IsKeyPressed(KEY_LEFT) && mUnits > 1) mUnits--;
if (IsKeyPressed(KEY_ENTER)) DoMed();
if (IsKeyPressed(KEY_ESCAPE)) modal = M_NONE;
} else if (tab == 6) {
TypeInto(searchQ, &searchLen, 28);
if (IsKeyPressed(KEY_ESCAPE)) { searchQ[0] = '\0'; searchLen = 0; }
} else {
if (IsKeyPressed(KEY_ONE)) SetTab(0); if (IsKeyPressed(KEY_TWO)) SetTab(1);
if (IsKeyPressed(KEY_THREE)) SetTab(2); if (IsKeyPressed(KEY_FOUR)) SetTab(3);
if (IsKeyPressed(KEY_FIVE)) SetTab(4); if (IsKeyPressed(KEY_SIX)) SetTab(5);
if (IsKeyPressed(KEY_SEVEN)) SetTab(6); if (IsKeyPressed(KEY_EIGHT)) SetTab(7);
if (IsKeyPressed(KEY_R)) OpenModal(M_REG);
if (IsKeyPressed(KEY_B)) OpenModal(M_BLOOD);
if (IsKeyPressed(KEY_M)) OpenModal(M_MED);
if (IsKeyPressed(KEY_A)) DoAdmit();
if (IsKeyPressed(KEY_D)) for (int i = 0; i < MAX_BEDS; i++) if (beds[i].occupied) { DoDischarge(i); break; }
}

/* ------------- animation state ------------- */
tabT = fminf(1.0f, tabT + gDt / 0.9f);
float e = EaseOut(tabT);
if (modal != M_NONE) modalAnim = fminf(1.0f, modalAnim + gDt / 0.25f);
if (toastT > 0) toastT -= gDt;

int occupied = 0, totalBlood = 0, onDuty = 0;
for (int i = 0; i < MAX_BEDS; i++) if (beds[i].occupied) occupied++;
for (int i = 0; i < MAX_BLOOD; i++) totalBlood += blood[i].units;
for (int i = 0; i < MAX_DOCTORS; i++) if (doctors[i].onDuty) onDuty++;
float target[4] = { (float)(MAX_BEDS - occupied), (float)(occupied + queueCount), (float)onDuty, (float)totalBlood };
for (int i = 0; i < 4; i++) disp[i] += (target[i] - disp[i]) * fminf(1.0f, gDt * 6.0f);

/* ------------- draw ------------- */
BeginDrawing();
ClearBackground(C_BG);
uiBlocked = (modal != M_NONE);

/* ambient glow */
for (int k = 0; k < 7; k++) {
DrawCircle(1120, 40, 380 * (1.0f - k / 7.0f), Fade(C_ACCENT, 0.022f));
DrawCircle(560, 800, 420 * (1.0f - k / 7.0f), Fade(C_CYAN, 0.014f));
}

/* ---- sidebar ---- */
DrawRectangle(0, 0, SIDEBAR_W, SCREEN_H, C_SIDE);
DrawRectangle(SIDEBAR_W - 1, 0, 1, SCREEN_H, C_BORDER);
RRect(R(22, 22, 44, 44), 14, C_ACCENT);
RRect(R(22, 22, 44, 22), 14, Fade(WHITE, 0.10f));
Icon(8, 44, 44, 11, WHITE);
TxtB("MediGrid", 78, 24, 22, C_TEXT);
TxtB("QUANTUM HIS", 79, 50, 10, C_ACCENTL);
TxtB("MENU", 28, 98, 10, C_DIM);

float targetY = 124 + tab * 52;
indicatorY += (targetY - indicatorY) * fminf(1.0f, gDt * 12.0f);
RRect(R(14, indicatorY, 212, 44), 12, Mix(C_SIDE, C_ACCENT, 0.22f));
RRect(R(14, indicatorY + 11, 4, 22), 2, C_ACCENTL);
for (int i = 0; i < 8; i++) {
Rectangle nr = R(14, 124 + i * 52, 212, 44);
float h = HoverAnim(nr);
bool act = (i == tab);
if (!act && h > 0.01f) RRect(nr, 12, Mix(C_SIDE, C_CARD, h));
Color ic = act ? C_ACCENTL : Mix(C_MUTED, C_TEXT, h);
Icon(navIcons[i], nr.x + 34, nr.y + 22, 9, ic);
TxtAt(navNames[i], nr.x + 56 + h * 3, nr.y + 14, 14, act ? C_TEXT : Mix(C_MUTED, C_TEXT, h), act);
if (i == 1 && queueCount > 0) {
DrawCircle((int)(nr.x + nr.width - 22), (int)(nr.y + 22), 11, Fade(C_AMBER, 0.2f));
TxtC(TextFormat("%d", queueCount), nr.x + nr.width - 22, nr.y + 16, 12, C_AMBER, true);
} else TxtR(TextFormat("%d", i + 1), nr.x + nr.width - 16, nr.y + 16, 11, C_DIM, false);
if (Clicked(nr)) SetTab(i);
}
RRectB(R(14, 566, 212, 118), 14, COL(15, 22, 44), C_BORDER, 1);
TxtB("SHORTCUTS", 30, 580, 10, C_DIM);
Txt("[1-8] Switch tab", 30, 602, 12, C_MUTED);
Txt("[R] Register [A] Admit", 30, 622, 12, C_MUTED);
Txt("[B] Blood [M] Meds", 30, 642, 12, C_MUTED);
Txt("[D] Discharge [Esc] Close", 30, 662, 12, C_MUTED);
RRectB(R(14, 706, 212, 70), 14, COL(15, 22, 44), C_BORDER, 1);
Avatar(48, 741, 19, "Admin Console", C_ACCENT);
TxtB("Admin Console", 78, 725, 14, C_TEXT);
float pulse = 0.5f + 0.5f * sinf(gTime * 3.0f);
DrawCircle(84, 752, 4 + pulse * 2.5f, Fade(C_GREEN, 0.25f)); DrawCircle(84, 752, 3.5f, C_GREEN);
Txt("System online", 96, 745, 12, C_GREEN);

/* ---- top bar ---- */
TxtB(titles[tab], CX, 16, 27, C_TEXT);
Txt(subs[tab], CX, 52, 13, C_MUTED);
float bx0 = CX + CW - 538;
if (Btn(R(bx0, 20, 140, 42), "Register", 8, C_ACCENT, false)) OpenModal(M_REG);
if (Btn(R(bx0 + 150, 20, 120, 42), "Admit Next", 11, COL(5, 150, 105), false)) DoAdmit();
if (Btn(R(bx0 + 280, 20, 118, 42), "Issue Blood", 3, COL(225, 29, 72), false)) OpenModal(M_BLOOD);
if (Btn(R(bx0 + 408, 20, 130, 42), "Dispense", 4, COL(180, 100, 10), false)) OpenModal(M_MED);

/* ---- pages ---- */
switch (tab) {
case 0: PageDashboard(e); break;
case 1: PagePatients(e); break;
case 2: PageBeds(e); break;
case 3: PageBloodDoc(e); break;
case 4: PagePharmacy(e); break;
case 5: PageBilling(e); break;
case 6: PageSearch(e); break;
case 7: PageLogs(e); break;
}

DrawModals();
DrawToast();
EndDrawing();
}
if (gCustom) {
if (gBold.texture.id != gReg.texture.id) UnloadFont(gBold);
UnloadFont(gReg);
}
CloseWindow();
return 0;
}

