#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

// ─────────────────────────────────────────────
//  Menü-Zustände
// ─────────────────────────────────────────────
enum MenuState {
    MAIN_MENU,
    SYSTEM_INFO,
    FILE_BROWSER,
    POWER_MENU,
    LED_FUN,
    MINI_GAME,
    NETWORK_INFO,
    HARDWARE_TEST,
    CLOCK_SCREEN,
    STORAGE_INFO,
    BUTTON_TEST,
    ABOUT
};

// ─────────────────────────────────────────────
//  Hilfsfunktionen
// ─────────────────────────────────────────────
void resetCursor() { printf("\x1b[H"); }
void fullClear()   { consoleClear(); }

const char* getIP() {
    u32 ip = gethostid();
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
        (int)((ip >>  0) & 0xFF), (int)((ip >>  8) & 0xFF),
        (int)((ip >> 16) & 0xFF), (int)((ip >> 24) & 0xFF));
    return buf;
}

const char* formatSize(u64 bytes) {
    static char buf[24];
    if     (bytes >= 1024ULL*1024*1024) snprintf(buf, sizeof(buf), "%.2f GB", (double)bytes/(1024.0*1024*1024));
    else if(bytes >= 1024ULL*1024)      snprintf(buf, sizeof(buf), "%.2f MB", (double)bytes/(1024.0*1024));
    else if(bytes >= 1024ULL)           snprintf(buf, sizeof(buf), "%.2f KB", (double)bytes/1024.0);
    else                                snprintf(buf, sizeof(buf), "%d B",    (int)bytes);
    return buf;
}

// ─────────────────────────────────────────────
//  System Info
// ─────────────────────────────────────────────
void showSystemInfo() {
    resetCursor();
    printf("\x1b[32m=== System Info ===\x1b[0m\n\n");

    u8 model = 0;
    cfguInit(); CFGU_GetSystemModel(&model); cfguExit();
    const char* models[] = {
        "Old 3DS","Old 3DS XL","New 3DS",
        "Old 2DS","New 3DS XL","New 2DS XL"
    };

    u8 batPct = 0, batChg = 0;
    MCUHWC_GetBatteryLevel(&batPct);
    PTMU_GetBatteryChargeState(&batChg);

    u8 slider3d = 0;
    MCUHWC_Get3dSliderLevel(&slider3d);

    u8 fwHigh = 0, fwLow = 0;
    MCUHWC_GetFwVerHigh(&fwHigh);
    MCUHWC_GetFwVerLow(&fwLow);

    u8 sound = 0;
    MCUHWC_GetSoundSliderLevel(&sound);

    u64 appUsed = 0, appTotal = 0;
    osGetMemRegionUsed(MEMREGION_APPLICATION);
    appUsed  = (u64)osGetMemRegionUsed(MEMREGION_APPLICATION);
    appTotal = (u64)osGetMemRegionSize(MEMREGION_APPLICATION);

    printf(" Model:    %-20s\n", model < 6 ? models[model] : "Unknown");
    printf(" Battery:  %d%% (%s)     \n",
           batPct, batChg ? "Charging" : "Not charging");
    printf(" 3D Sld:   %d/255           \n", slider3d);
    printf(" Sound:    %d/255           \n", sound);
    printf(" MCU FW:   %d.%d            \n", fwHigh, fwLow);
    printf(" RAM used: %s / %s     \n",
           formatSize(appUsed), formatSize(appTotal));
    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  Network Info
// ─────────────────────────────────────────────
void showNetworkInfo() {
    resetCursor();
    printf("\x1b[36m=== Network Info ===\x1b[0m\n\n");

    u32 wifi = 0;
    ACU_GetWifiStatus(&wifi);

    u32 wifiStrength = 0;
    ACU_GetNZoneApNum(&wifiStrength); // Anzahl gefundener APs als Proxy

    printf(" Status:   %-20s\n", wifi ? "Connected   " : "Disconnected");
    printf(" IP Addr:  %-20s\n", getIP());

    // SSID lesen
    char ssid[33] = {0};
    acGetSSID(ssid);
    printf(" SSID:     %-20s\n", ssid[0] ? ssid : "(none)");

    printf(" Nearby AP:%d              \n", wifiStrength);
    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  Storage Info
// ─────────────────────────────────────────────
void showStorageInfo() {
    resetCursor();
    printf("\x1b[33m=== Storage Info ===\x1b[0m\n\n");

    // SD-Karte
    u64 sdFree = 0, sdTotal = 0;
    FSUSER_GetSdmcFreeSpace(NULL, &sdFree);
    FSUSER_GetSdmcTotalSpace(NULL, &sdTotal);

    // NAND
    u64 nandFree = 0, nandTotal = 0;
    FSUSER_GetNandFreeSpace(NULL, &nandFree);
    FSUSER_GetNandTotalSpace(NULL, &nandTotal);

    printf(" SD Card:\n");
    printf("  Total: %-20s\n", formatSize(sdTotal));
    printf("  Free:  %-20s\n", formatSize(sdFree));
    printf("  Used:  %-20s\n", formatSize(sdTotal - sdFree));

    // Balken
    int pct = sdTotal > 0 ? (int)(100 - (sdFree * 100 / sdTotal)) : 0;
    printf("  [");
    for(int i = 0; i < 20; i++) printf(i < pct/5 ? "#" : "-");
    printf("] %d%%\n\n", pct);

    printf(" NAND:\n");
    printf("  Total: %-20s\n", formatSize(nandTotal));
    printf("  Free:  %-20s\n", formatSize(nandFree));

    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  Uhr / Datum
// ─────────────────────────────────────────────
void showClock() {
    resetCursor();
    printf("\x1b[35m=== Clock & Date ===\x1b[0m\n\n");

    u64 msec = osGetTime();
    time_t sec = (time_t)(msec / 1000);
    struct tm* t = gmtime(&sec);

    printf("  Date: %04d-%02d-%02d\n\n",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    printf("  \x1b[32m%02d:%02d:%02d\x1b[0m (UTC)\n\n",
           t->tm_hour, t->tm_min, t->tm_sec);

    // Wochentag
    const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    printf("  Day:  %s\n\n", days[t->tm_wday]);

    u64 upMs = osGetTime();
    printf("  Uptime: %llus\n", (unsigned long long)(upMs / 1000));

    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  Hardware Test – Slider, Gyro, Touch
// ─────────────────────────────────────────────
void showHardwareTest() {
    resetCursor();
    printf("\x1b[33m=== Hardware Test ===\x1b[0m\n\n");

    // Touchscreen
    touchPosition touch;
    hidTouchRead(&touch);
    printf(" Touch:  X=%-4d Y=%-4d        \n", touch.px, touch.py);

    // Circle Pad
    circlePosition circle;
    hidCircleRead(&circle);
    printf(" Circle: X=%-5d Y=%-5d      \n", circle.dx, circle.dy);

    // Gyro
    angularRate gyro;
    hidGyroRead(&gyro);
    printf(" Gyro:   X=%-6d Y=%-6d    \n", gyro.x, gyro.y);
    printf("         Z=%-6d            \n", gyro.z);

    // Accel
    accelVector accel;
    hidAccelRead(&accel);
    printf(" Accel:  X=%-6d Y=%-6d    \n", accel.x, accel.y);
    printf("         Z=%-6d            \n", accel.z);

    // Slider
    u8 slider3d = 0, sound = 0;
    MCUHWC_Get3dSliderLevel(&slider3d);
    MCUHWC_GetSoundSliderLevel(&sound);
    printf(" 3D Sld: %d    Sound: %d     \n", slider3d, sound);

    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  Button Test
// ─────────────────────────────────────────────
void showButtonTest(u32 kHeld) {
    resetCursor();
    printf("\x1b[36m=== Button Test ===\x1b[0m\n\n");

    struct { u32 key; const char* name; } btns[] = {
        {KEY_A,      "A      "}, {KEY_B,      "B      "},
        {KEY_X,      "X      "}, {KEY_Y,      "Y      "},
        {KEY_L,      "L      "}, {KEY_R,      "R      "},
        {KEY_ZL,     "ZL     "}, {KEY_ZR,     "ZR     "},
        {KEY_DUP,    "D-Up   "}, {KEY_DDOWN,  "D-Down "},
        {KEY_DLEFT,  "D-Left "}, {KEY_DRIGHT, "D-Right"},
        {KEY_START,  "Start  "}, {KEY_SELECT, "Select "},
        {KEY_CPAD_UP,"C-Up   "}, {KEY_CPAD_DOWN,"C-Down"},
    };
    int n = sizeof(btns)/sizeof(btns[0]);
    for(int i = 0; i < n; i++) {
        bool held = (kHeld & btns[i].key) != 0;
        if(i % 2 == 0) printf(" ");
        printf("%s %s  ", btns[i].name, held ? "\x1b[32m[ON]\x1b[0m " : "\x1b[31m[  ]\x1b[0m");
        if(i % 2 == 1) printf("\n");
    }
    printf("\n\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  Power Menu
// ─────────────────────────────────────────────
void showPowerMenu(int sel) {
    resetCursor();
    printf("\x1b[31m=== Power Options ===\x1b[0m\n\n");
    const char* opts[] = {
        "Return to Loader  ",
        "Reboot System     ",
        "Return to Home    "
    };
    for(int i = 0; i < 3; i++)
        printf(" %s %s\n", sel == i ? ">" : " ", opts[i]);
    printf("\n\x1b[20;1HPress A to confirm, B to return.");
}

// ─────────────────────────────────────────────
//  LED Fun
// ─────────────────────────────────────────────
static const char* ledOpts[] = {
    "Wifi LED: ON          ",
    "Wifi LED: OFF         ",
    "Power LED: Normal     ",
    "Power LED: Sleep Blink",
    "Power LED: OFF        ",
};
#define LED_COUNT 5

void showLEDFun(int sel) {
    resetCursor();
    printf("\x1b[36m=== LED Control ===\x1b[0m\n\n");
    for(int i = 0; i < LED_COUNT; i++)
        printf(" %s %s\n", sel == i ? ">" : " ", ledOpts[i]);
    printf("\n\x1b[20;1HPress A to activate, B to return.");
}

void activateLED(int sel) {
    switch(sel) {
        case 0: MCUHWC_SetWifiLedState(true);            break;
        case 1: MCUHWC_SetWifiLedState(false);           break;
        case 2: MCUHWC_SetPowerLedState(LED_NORMAL);     break;
        case 3: MCUHWC_SetPowerLedState(LED_SLEEP_MODE); break;
        case 4: MCUHWC_SetPowerLedState(LED_OFF);        break;
    }
}

// ─────────────────────────────────────────────
//  Mini-Game
// ─────────────────────────────────────────────
void playMiniGame(int guess, int attempts, bool won, int target) {
    resetCursor();
    printf("\x1b[35m=== Fox Guess (1-100) ===\x1b[0m\n\n");
    if(won) {
        printf(" \x1b[32mCONGRATS! Found %d in %d tries!\x1b[0m\n", target, attempts);
        printf(" Press A to play again.        \n");
        printf("                               \n");
    } else {
        printf(" Guess:    %d    \n", guess);
        printf(" Attempts: %d    \n", attempts);
        // Hint
        if(attempts > 0) {
            if(guess < target)       printf(" Hint: \x1b[33mGo HIGHER!\x1b[0m    \n");
            else if(guess > target)  printf(" Hint: \x1b[33mGo LOWER! \x1b[0m    \n");
        } else {
            printf("                           \n");
        }
        printf("\n DPad Up/Down: change\n A: guess\n");
    }
    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  File Browser
// ─────────────────────────────────────────────
#define FB_MAX  128
#define FB_NAME  48
#define FB_VIS   16

struct FBState {
    char path[256];
    char entries[FB_MAX][FB_NAME];
    bool isDir[FB_MAX];
    int  count, selected, scrollTop;
    bool confirmDelete;
    char message[64];
    int  msgTimer;
};

void fbLoad(FBState& fb) {
    fb.count = fb.selected = fb.scrollTop = 0;
    fb.confirmDelete = false;
    fb.message[0] = '\0'; fb.msgTimer = 0;

    DIR* dir = opendir(fb.path);
    if(!dir) return;

    if(strcmp(fb.path, "sdmc:/") != 0) {
        strncpy(fb.entries[fb.count], "..", FB_NAME-1);
        fb.isDir[fb.count++] = true;
    }

    struct dirent* ent;
    while((ent = readdir(dir)) && fb.count < FB_MAX) {
        if(ent->d_name[0] == '.') continue;
        strncpy(fb.entries[fb.count], ent->d_name, FB_NAME-1);
        fb.entries[fb.count][FB_NAME-1] = '\0';
        char fp[320]; snprintf(fp, sizeof(fp), "%s%s", fb.path, ent->d_name);
        struct stat st;
        fb.isDir[fb.count] = (stat(fp, &st) == 0 && S_ISDIR(st.st_mode));
        fb.count++;
    }
    closedir(dir);
}

void fbNavigate(FBState& fb) {
    if(!fb.count) return;
    if(strcmp(fb.entries[fb.selected], "..") == 0) {
        size_t len = strlen(fb.path);
        if(len > 7) {
            char tmp[256]; strncpy(tmp, fb.path, sizeof(tmp));
            tmp[len-1] = '\0';
            char* s = strrchr(tmp, '/');
            if(s) { *(s+1) = '\0'; strncpy(fb.path, tmp, sizeof(fb.path)); }
        }
    } else if(fb.isDir[fb.selected]) {
        size_t len = strlen(fb.path);
        strncat(fb.path, fb.entries[fb.selected], sizeof(fb.path)-len-2);
        strncat(fb.path, "/", sizeof(fb.path)-strlen(fb.path)-1);
    }
    fbLoad(fb);
}

void fbDelete(FBState& fb) {
    if(!fb.count || strcmp(fb.entries[fb.selected], "..") == 0) return;
    char fp[320]; snprintf(fp, sizeof(fp), "%s%s", fb.path, fb.entries[fb.selected]);
    int r = fb.isDir[fb.selected] ? rmdir(fp) : remove(fp);
    snprintf(fb.message, sizeof(fb.message), r == 0 ? "Deleted!" : "Delete failed!");
    fb.msgTimer = 90;
    fbLoad(fb);
}

void showFileBrowser(FBState& fb) {
    resetCursor();
    char sp[38]; strncpy(sp, fb.path, 37); sp[37] = '\0';
    printf("\x1b[33m%-38s\x1b[0m\n", sp);
    printf("--------------------------------------\n");

    if(!fb.count) {
        printf(" (empty)                             \n");
        for(int i = 1; i < FB_VIS; i++) printf("                                     \n");
    } else {
        int end = fb.scrollTop + FB_VIS;
        if(end > fb.count) end = fb.count;
        for(int i = fb.scrollTop; i < end; i++) {
            bool sel = (i == fb.selected);
            char nm[28]; strncpy(nm, fb.entries[i], 27); nm[27] = '\0';
            printf(" %s %s %-27s\n",
                   sel ? ">" : " ",
                   fb.isDir[i] ? "\x1b[34m[D]\x1b[0m" : "[F]", nm);
        }
        for(int i = end - fb.scrollTop; i < FB_VIS; i++)
            printf("                                     \n");
    }

    printf("-- %d/%d ", fb.selected+1, fb.count);
    if(fb.msgTimer > 0) { printf("%-20s\n", fb.message); fb.msgTimer--; }
    else printf("                    \n");

    printf("\x1b[20;1H");
    if(fb.confirmDelete)
        printf("\x1b[31mDelete? A=Yes B=No              \x1b[0m");
    else
        printf("A=Enter  X=Delete  B=Back       ");
}

void fbInput(FBState& fb, u32 kDown) {
    if(fb.confirmDelete) {
        if(kDown & KEY_A) fbDelete(fb);
        if(kDown & (KEY_A|KEY_B)) fb.confirmDelete = false;
        return;
    }
    if(kDown & KEY_DUP && fb.selected > 0) {
        fb.selected--;
        if(fb.selected < fb.scrollTop) fb.scrollTop = fb.selected;
    }
    if(kDown & KEY_DDOWN && fb.selected < fb.count-1) {
        fb.selected++;
        if(fb.selected >= fb.scrollTop + FB_VIS)
            fb.scrollTop = fb.selected - FB_VIS + 1;
    }
    if(kDown & KEY_A) fbNavigate(fb);
    if(kDown & KEY_X && fb.count && strcmp(fb.entries[fb.selected],"..") != 0)
        fb.confirmDelete = true;
}

// ─────────────────────────────────────────────
//  About
// ─────────────────────────────────────────────
void showAbout() {
    resetCursor();
    printf("\x1b[35m=== DarkFox-3DS v2.0 ===\x1b[0m\n\n");
    printf(" Developer: DarkFox Team\n\n");
    printf(" Features:\n");
    printf("  [1] System Info\n");
    printf("  [2] File Browser + Delete\n");
    printf("  [3] Power Options\n");
    printf("  [4] LED Control\n");
    printf("  [5] Mini-Game w/ Hints\n");
    printf("  [6] Network Info\n");
    printf("  [7] Hardware Test\n");
    printf("  [8] Clock & Date\n");
    printf("  [9] Storage Info\n");
    printf("  [0] Button Tester\n");
    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    gfxInitDefault();
    PrintConsole topScreen, bottomScreen;
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    mcuHwcInit();
    ptmuInit();
    acInit();
    hidInit();

    FBState fb;
    memset(&fb, 0, sizeof(fb));
    strncpy(fb.path, "sdmc:/", sizeof(fb.path)-1);
    fbLoad(fb);

    int selected    = 0;
    int subSelected = 0;
    MenuState state     = MAIN_MENU;
    MenuState prevState = (MenuState)-1;

    int  guess = 50, target = 1, attempts = 0;
    bool won = false;
    srand((unsigned int)osGetTime());
    target = rand() % 100 + 1;

    const char* mainOpts[] = {
        "System Info  ","File Browser ","Power Options",
        "LED Control  ","Mini-Game    ","Network Info ",
        "Hardware Test","Clock & Date ","Storage Info ","Button Test  ","About        "
    };
    const int mainCount = 11;

    while(aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        if(kDown & KEY_START) break;

        bool stateChanged = (state != prevState);
        if(stateChanged) {
            consoleSelect(&topScreen);    fullClear();
            consoleSelect(&bottomScreen); fullClear();
            prevState = state;
            if(state == FILE_BROWSER) fbLoad(fb);
        }

        // Bottom Screen
        consoleSelect(&bottomScreen);
        printf("\x1b[H\x1b[31mDarkFox-3DS v2.0\x1b[0m\n");
        printf("----------------------------\n");
        u8 bat = 0; MCUHWC_GetBatteryLevel(&bat);
        u8 batChg = 0; PTMU_GetBatteryChargeState(&batChg);
        printf("Bat: %d%% %s\n", bat, batChg ? "\x1b[32m[CHG]\x1b[0m" : "     ");

        u64 ms = osGetTime();
        time_t sec = (time_t)(ms / 1000);
        struct tm* t = gmtime(&sec);
        printf("UTC: %02d:%02d:%02d\n", t->tm_hour, t->tm_min, t->tm_sec);

        u32 wifi = 0; ACU_GetWifiStatus(&wifi);
        printf("Net: %s\n", wifi ? "\x1b[32mConnected\x1b[0m   " : "\x1b[31mNo WiFi\x1b[0m     ");
        printf("IP:  %s\n", getIP());

        // Top Screen
        consoleSelect(&topScreen);

        if(state == MAIN_MENU) {
            resetCursor();
            printf("\x1b[31mDarkFox-3DS v2.0\x1b[0m\n\n");
            for(int i = 0; i < mainCount; i++)
                printf(" %s %2d. %s\n", selected == i ? ">" : " ", i+1, mainOpts[i]);

            if(kDown & KEY_DUP)   selected = (selected - 1 + mainCount) % mainCount;
            if(kDown & KEY_DDOWN) selected = (selected + 1) % mainCount;
            if(kDown & KEY_A) {
                subSelected = 0;
                switch(selected) {
                    case 0:  state = SYSTEM_INFO;   break;
                    case 1:  state = FILE_BROWSER;  break;
                    case 2:  state = POWER_MENU;    break;
                    case 3:  state = LED_FUN;       break;
                    case 4:  state = MINI_GAME;     break;
                    case 5:  state = NETWORK_INFO;  break;
                    case 6:  state = HARDWARE_TEST; break;
                    case 7:  state = CLOCK_SCREEN;  break;
                    case 8:  state = STORAGE_INFO;  break;
                    case 9:  state = BUTTON_TEST;   break;
                    case 10: state = ABOUT;         break;
                }
            }
        } else {
            switch(state) {
                case SYSTEM_INFO:   showSystemInfo();   break;
                case NETWORK_INFO:  showNetworkInfo();  break;
                case STORAGE_INFO:  showStorageInfo();  break;
                case CLOCK_SCREEN:  showClock();        break;
                case HARDWARE_TEST: showHardwareTest(); break;
                case BUTTON_TEST:   showButtonTest(kHeld); break;
                case ABOUT:         showAbout();        break;

                case POWER_MENU:
                    showPowerMenu(subSelected);
                    if(kDown & KEY_DUP)   subSelected = (subSelected-1+3)%3;
                    if(kDown & KEY_DDOWN) subSelected = (subSelected+1)%3;
                    if(kDown & KEY_A) {
                        if(subSelected == 0) goto done;
                        if(subSelected == 1) NS_RebootSystem();
                        if(subSelected == 2) goto done;
                    }
                    break;

                case LED_FUN:
                    showLEDFun(subSelected);
                    if(kDown & KEY_DUP)   subSelected = (subSelected-1+LED_COUNT)%LED_COUNT;
                    if(kDown & KEY_DDOWN) subSelected = (subSelected+1)%LED_COUNT;
                    if(kDown & KEY_A)     activateLED(subSelected);
                    break;

                case MINI_GAME:
                    playMiniGame(guess, attempts, won, target);
                    if(!won) {
                        if(kDown & KEY_DUP)   { guess++; if(guess>100) guess=100; }
                        if(kDown & KEY_DDOWN) { guess--; if(guess<1)   guess=1;   }
                        if(kDown & KEY_A) { attempts++; if(guess==target) won=true; }
                    } else if(kDown & KEY_A) {
                        won=false; attempts=0; guess=50; target=rand()%100+1;
                    }
                    break;

                case FILE_BROWSER:
                    fbInput(fb, kDown);
                    showFileBrowser(fb);
                    break;

                default: break;
            }

            if((kDown & KEY_B) && state == FILE_BROWSER && !fb.confirmDelete)
                state = MAIN_MENU;
            else if((kDown & KEY_B) && state != FILE_BROWSER)
                state = MAIN_MENU;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

done:
    acExit();
    ptmuExit();
    mcuHwcExit();
    gfxExit();
    return 0;
}
