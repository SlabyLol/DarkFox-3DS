#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

enum MenuState { MAIN_MENU, SYSTEM_INFO, FILE_BROWSER, POWER_MENU, LED_FUN, MINI_GAME, ABOUT };

void resetCursor() { printf("\x1b[H"); }
void fullClear()   { consoleClear(); }

// inet_ntoa existiert nicht auf 3DS – manuell formatieren
const char* getIP() {
    u32 ip = gethostid();
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
        (int)((ip >>  0) & 0xFF),
        (int)((ip >>  8) & 0xFF),
        (int)((ip >> 16) & 0xFF),
        (int)((ip >> 24) & 0xFF));
    return buf;
}

// LED-Farbe via MCUHWC Register 0x2D setzen (3 Bytes: R G B)
// Funktioniert auf allen libctru-Versionen ohne SetInfoLedPattern
void setLED(u8 r, u8 g, u8 b) {
    u8 pattern[3] = {r, g, b};
    MCUHWC_WriteData(0x2D, pattern, 3);
}

void showSystemInfo() {
    resetCursor();
    printf("\x1b[32m--- Advanced System Info ---\x1b[0m\n\n");

    u8 model = 0;
    cfguInit();
    CFGU_GetSystemModel(&model);
    cfguExit();
    const char* models[] = {"Old 3DS", "Old 3DS XL", "New 3DS",
                             "Old 2DS", "New 3DS XL", "New 2DS XL"};

    u8 batPct = 0, batChg = 0;
    MCUHWC_GetBatteryLevel(&batPct);
    PTMU_GetBatteryChargeState(&batChg);

    u32 wifi = 0;
    ACU_GetWifiStatus(&wifi);

    printf(" Model:   %-22s\n", (model < 6 ? models[model] : "Unknown"));
    printf(" Battery: %d%% (%s)          \n",
           batPct, (batChg ? "Charging    " : "Not Charging"));
    printf(" IP:      %-22s\n", getIP());
    printf(" WiFi:    %-22s\n", (wifi ? "Connected   " : "Disconnected"));
    printf("\n");
    printf("\x1b[20;1HPress B to return.          ");
}

void showPowerMenu(int selected) {
    resetCursor();
    printf("\x1b[31m--- Power Options ---\x1b[0m\n\n");
    const char* opts[] = {
        "Return to Loader  ",
        "Reboot System     ",
        "Return to Home    "
    };
    for(int i = 0; i < 3; i++)
        printf(" %s %s\n", (selected == i ? ">" : " "), opts[i]);
    printf("\n\x1b[20;1HPress A to confirm, B to return.");
}

void showLEDFun(int selected) {
    resetCursor();
    printf("\x1b[36m--- LED Color Fun ---\x1b[0m\n\n");
    const char* colors[] = {
        "Blue  ", "Green ", "Red   ",
        "Yellow", "White ", "Off   "
    };
    for(int i = 0; i < 6; i++)
        printf(" %s %s\n", (selected == i ? ">" : " "), colors[i]);
    printf("\n\x1b[20;1HPress A to set LED, B to return.");
}

void playMiniGame(int guess, int attempts, bool won) {
    resetCursor();
    printf("\x1b[35m--- Fox Guess (1-100) ---\x1b[0m\n\n");
    if(won) {
        printf(" CONGRATS! Found in %d tries!   \n", attempts);
        printf(" Press A to play again.          \n");
        printf("                                 \n");
    } else {
        printf(" Current Guess: %d   \n", guess);
        printf(" Attempts:      %d   \n", attempts);
        printf("\n D-Pad Up/Down to change, A to guess.\n");
    }
    printf("\n\x1b[20;1HPress B to return.");
}

void showAbout() {
    resetCursor();
    printf("\x1b[35m--- DarkFox-3DS ---\x1b[0m\n\n");
    printf(" Version:   1.2.2\n");
    printf(" Developer: DarkFox Team\n\n");
    printf(" Running on real hardware!\n");
    printf("\n\x1b[20;1HPress B to return.");
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    PrintConsole topScreen, bottomScreen;
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    // Services initialisieren
    mcuHwcInit();
    ptmuInit();
    cfguInit();
    acInit();

    int selected    = 0;
    int subSelected = 0;
    MenuState state     = MAIN_MENU;
    MenuState prevState = (MenuState)-1;

    int  guess    = 50;
    int  target   = 42;
    int  attempts = 0;
    bool won      = false;
    srand((unsigned int)osGetTime());
    target = rand() % 100 + 1;

    while(aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if(kDown & KEY_START) break;

        // State-Wechsel -> einmal sauber clearen
        bool stateChanged = (state != prevState);
        if(stateChanged) {
            consoleSelect(&topScreen);    fullClear();
            consoleSelect(&bottomScreen); fullClear();
            prevState = state;
        }

        // Bottom Screen – Statusleiste
        consoleSelect(&bottomScreen);
        printf("\x1b[H");
        printf("\x1b[31mDarkFox-3DS Status\x1b[0m\n");
        printf("--------------------------\n");
        u8 bat = 0;
        MCUHWC_GetBatteryLevel(&bat);
        printf("Battery: %d%%  |  %-12s\n", bat,
               (state == MAIN_MENU ? "Main Menu" : "Sub-Menu "));

        // Top Screen
        consoleSelect(&topScreen);

        if(state == MAIN_MENU) {
            resetCursor();
            printf("\x1b[31mDarkFox-3DS Utility v1.2.2\x1b[0m\n\n");
            const char* opts[] = {
                "System Info  ", "File Browser ", "Power Options",
                "LED Fun      ", "Mini-Game    ", "About        "
            };
            for(int i = 0; i < 6; i++)
                printf(" %s %d. %s\n", (selected == i ? ">" : " "), i+1, opts[i]);

            if(kDown & KEY_DUP)   selected = (selected - 1 + 6) % 6;
            if(kDown & KEY_DDOWN) selected = (selected + 1) % 6;
            if(kDown & KEY_A) {
                subSelected = 0;
                switch(selected) {
                    case 0: state = SYSTEM_INFO;  break;
                    case 1: state = FILE_BROWSER; break;
                    case 2: state = POWER_MENU;   break;
                    case 3: state = LED_FUN;      break;
                    case 4: state = MINI_GAME;    break;
                    case 5: state = ABOUT;        break;
                }
            }
        } else {
            if(state == SYSTEM_INFO) {
                showSystemInfo();
            }
            else if(state == POWER_MENU) {
                showPowerMenu(subSelected);
                if(kDown & KEY_DUP)   subSelected = (subSelected - 1 + 3) % 3;
                if(kDown & KEY_DDOWN) subSelected = (subSelected + 1) % 3;
                if(kDown & KEY_A) {
                    if(subSelected == 0) break;            // Return to Loader
                    if(subSelected == 1) NS_RebootSystem();// Reboot
                    if(subSelected == 2) break;            // Return to Home
                }
            }
            else if(state == LED_FUN) {
                showLEDFun(subSelected);
                if(kDown & KEY_DUP)   subSelected = (subSelected - 1 + 6) % 6;
                if(kDown & KEY_DDOWN) subSelected = (subSelected + 1) % 6;
                if(kDown & KEY_A) {
                    u8 r=0, g=0, b=0;
                    switch(subSelected) {
                        case 0: b=255; break;               // Blue
                        case 1: g=255; break;               // Green
                        case 2: r=255; break;               // Red
                        case 3: r=255; g=255; break;        // Yellow
                        case 4: r=255; g=255; b=255; break; // White
                        case 5: break;                      // Off
                    }
                    setLED(r, g, b);
                }
            }
            else if(state == MINI_GAME) {
                playMiniGame(guess, attempts, won);
                if(!won) {
                    if(kDown & KEY_DUP)   { guess++; if(guess > 100) guess = 100; }
                    if(kDown & KEY_DDOWN) { guess--; if(guess <   1) guess =   1; }
                    if(kDown & KEY_A) {
                        attempts++;
                        if(guess == target) won = true;
                    }
                } else if(kDown & KEY_A) {
                    won = false; attempts = 0; guess = 50;
                    target = rand() % 100 + 1;
                }
            }
            else if(state == ABOUT) {
                showAbout();
            }

            if(kDown & KEY_B) state = MAIN_MENU;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    // Services freigeben
    acExit();
    cfguExit();
    ptmuExit();
    mcuHwcExit();
    gfxExit();
    return 0;
}
