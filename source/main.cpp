#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

enum MenuState { MAIN_MENU, SYSTEM_INFO, FILE_BROWSER, POWER_MENU, LED_FUN, MINI_GAME, ABOUT };

// Nur einmal clearen, dann Cursor an den Anfang setzen
void resetCursor() {
    printf("\x1b[H"); // Cursor zu 0,0 – kein flackerndes clear jeden Frame
}

void fullClear() {
    consoleClear();
}

const char* getIP() {
    u32 ip = gethostid();
    static char buf[16];
    snprintf(buf, sizeof(buf), "%lu.%lu.%lu.%lu",
        (ip >> 0)  & 0xFF,
        (ip >> 8)  & 0xFF,
        (ip >> 16) & 0xFF,
        (ip >> 24) & 0xFF);
    return buf;
}

void showSystemInfo() {
    resetCursor();
    printf("\x1b[32m--- Advanced System Info ---\x1b[0m\n\n");
    u8 model; cfguInit(); CFGU_GetSystemModel(&model); cfguExit();
    const char* models[] = {"Old 3DS", "Old 3DS XL", "New 3DS", "Old 2DS", "New 3DS XL", "New 2DS XL"};
    u8 batPct, batChg; MCUHWC_GetBatteryLevel(&batPct); PTMU_GetBatteryChargeState(&batChg);

    printf(" Model:   %-20s\n", (model < 6 ? models[model] : "Unknown"));
    printf(" Battery: %d%% (%s)          \n", batPct, (batChg ? "Charging    " : "Not Charging"));
    printf(" IP:      %-20s\n", getIP());

    u32 wifi; ACU_GetWifiStatus(&wifi);
    printf(" WiFi:    %-20s\n", (wifi ? "Connected   " : "Disconnected"));
    printf("\n\x1b[20;1HPress B to return.          ");
}

void showPowerMenu(int selected) {
    resetCursor();
    printf("\x1b[31m--- Power Options ---\x1b[0m\n\n");
    const char* opts[] = {"Return to Loader", "Reboot System", "Return to Home Menu"};
    for(int i=0; i<3; i++)
        printf(" %s %s          \n", (selected == i ? ">" : " "), opts[i]);
    printf("\n\x1b[20;1HPress A to confirm, B to return.");
}

void showLEDFun(int selected) {
    resetCursor();
    printf("\x1b[36m--- LED Color Fun ---\x1b[0m\n\n");
    const char* colors[] = {"Blue", "Green", "Red", "Yellow", "White", "Off"};
    for(int i=0; i<6; i++)
        printf(" %s %s          \n", (selected == i ? ">" : " "), colors[i]);
    printf("\n\x1b[20;1HPress A to set LED, B to return.");
}

void playMiniGame(int guess, int target, int attempts, bool won) {
    resetCursor();
    printf("\x1b[35m--- Fox Guess (1-100) ---\x1b[0m\n\n");
    if(won) {
        printf(" CONGRATS! Found in %d tries!   \n", attempts);
        printf(" Press A to play again.          ");
        printf("                                 \n");
        printf("                                 \n");
    } else {
        printf(" Current Guess: %d   \n", guess);
        printf(" Attempts:      %d   \n", attempts);
        printf("\n Use D-Pad Up/Down to change, A to guess.");
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

    mcuHwcInit();
    ptmuInit();
    acuInit();

    int selected = 0;
    int subSelected = 0;
    MenuState state = MAIN_MENU;
    MenuState prevState = (MenuState)-1; // Damit beim ersten Frame ein clear passiert

    int guess = 50, target = 42, attempts = 0;
    bool won = false;
    srand(osGetTime());
    target = rand() % 100 + 1;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        // State gewechselt -> einmal clearen
        bool stateChanged = (state != prevState);
        if (stateChanged) {
            consoleSelect(&topScreen);
            fullClear();
            consoleSelect(&bottomScreen);
            fullClear();
            prevState = state;
        }

        // Bottom Screen – Status
        consoleSelect(&bottomScreen);
        printf("\x1b[H"); // Cursor reset, kein flackerndes clear
        printf("\x1b[31mDarkFox-3DS Status\x1b[0m\n");
        printf("--------------------------\n");
        u8 bat; MCUHWC_GetBatteryLevel(&bat);
        printf("Battery: %d%%  |  %-12s\n", bat,
            (state == MAIN_MENU ? "Main Menu" : "Sub-Menu "));

        // Top Screen
        consoleSelect(&topScreen);

        if (state == MAIN_MENU) {
            resetCursor();
            printf("\x1b[31mDarkFox-3DS Utility v1.2.2\x1b[0m\n\n");
            const char* mainOpts[] = {
                "System Info", "File Browser", "Power Options",
                "LED Fun", "Mini-Game", "About"
            };
            for(int i=0; i<6; i++)
                printf(" %s %d. %-20s\n", (selected == i ? ">" : " "), i+1, mainOpts[i]);

            if (kDown & KEY_DUP)   selected = (selected - 1 + 6) % 6;
            if (kDown & KEY_DDOWN) selected = (selected + 1) % 6;
            if (kDown & KEY_A) {
                subSelected = 0;
                if      (selected == 0) state = SYSTEM_INFO;
                else if (selected == 1) state = FILE_BROWSER;
                else if (selected == 2) state = POWER_MENU;
                else if (selected == 3) state = LED_FUN;
                else if (selected == 4) state = MINI_GAME;
                else if (selected == 5) state = ABOUT;
            }
        } else {
            if (state == SYSTEM_INFO) {
                showSystemInfo();
            }
            else if (state == POWER_MENU) {
                showPowerMenu(subSelected);
                if (kDown & KEY_DUP)   subSelected = (subSelected - 1 + 3) % 3;
                if (kDown & KEY_DDOWN) subSelected = (subSelected + 1) % 3;
                if (kDown & KEY_A) {
                    if (subSelected == 0) break;
                    if (subSelected == 1) NS_RebootSystem();
                    if (subSelected == 2) break;
                }
            }
            else if (state == LED_FUN) {
                showLEDFun(subSelected);
                if (kDown & KEY_DUP)   subSelected = (subSelected - 1 + 6) % 6;
                if (kDown & KEY_DDOWN) subSelected = (subSelected + 1) % 6;
                if (kDown & KEY_A) {
                    u8 r=0, g=0, b=0;
                    switch(subSelected) {
                        case 0: b=255; break;
                        case 1: g=255; break;
                        case 2: r=255; break;
                        case 3: r=255; g=255; break;
                        case 4: r=255; g=255; b=255; break;
                        case 5: break;
                    }
                    MCUHWC_SetInfoLEDPattern(r, g, b);
                }
            }
            else if (state == MINI_GAME) {
                playMiniGame(guess, target, attempts, won);
                if(!won) {
                    if(kDown & KEY_DUP)   guess++;
                    if(kDown & KEY_DDOWN) guess--;
                    if(guess < 1)   guess = 1;
                    if(guess > 100) guess = 100;
                    if(kDown & KEY_A) {
                        attempts++;
                        if(guess == target) won = true;
                    }
                } else if(kDown & KEY_A) {
                    won = false; attempts = 0; guess = 50;
                    target = rand() % 100 + 1;
                }
            }
            else if (state == ABOUT) {
                showAbout();
            }

            if (kDown & KEY_B) state = MAIN_MENU;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    acuExit();
    ptmuExit();
    mcuHwcExit();
    gfxExit();
    return 0;
}
