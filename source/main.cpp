#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <malloc.h>
#include <unistd.h>

enum MenuState { MAIN_MENU, SYSTEM_INFO, FILE_BROWSER, POWER_MENU, LED_FUN, MINI_GAME, ABOUT };

void clearUI() {
    consoleClear();
    printf("\x1b[2J");
}

// Function to get IP Address
const char* getIP() {
    struct in_addr addr;
    addr.s_addr = gethostid();
    return inet_ntoa(addr);
}

void showSystemInfo() {
    clearUI();
    printf("\x1b[1;1H\x1b[32m--- Advanced System Info ---\x1b[0m\n\n");
    u8 model; cfguInit(); CFGU_GetSystemModel(&model); cfguExit();
    const char* models[] = {"Old 3DS", "Old 3DS XL", "New 3DS", "Old 2DS", "New 3DS XL", "New 2DS XL"};
    u8 batPct, batChg; MCUHWC_GetBatteryLevel(&batPct); PTMU_GetBatteryChargeState(&batChg);
    
    printf(" Model: %s\n", (model < 6 ? models[model] : "Unknown"));
    printf(" Battery: %d%% (%s)\n", batPct, (batChg ? "Charging" : "Not Charging"));
    printf(" IP Address: %s\n", getIP());
    
    u32 wifi; AC_GetWifiStatus(&wifi);
    printf(" WiFi Status: %s\n", (wifi ? "Connected" : "Disconnected"));
    printf("\n\x1b[20;1HPress B to return.");
}

void showPowerMenu(int selected) {
    clearUI();
    printf("\x1b[1;1H\x1b[31m--- Power Options ---\x1b[0m\n\n");
    const char* opts[] = {"Reboot System", "Power Off", "Return to Home Menu"};
    for(int i=0; i<3; i++) printf(" %s %s\n", (selected == i ? ">" : " "), opts[i]);
    printf("\n\x1b[20;1HPress A to confirm, B to return.");
}

void showLEDFun(int selected) {
    clearUI();
    printf("\x1b[1;1H\x1b[36m--- LED Color Fun ---\x1b[0m\n\n");
    const char* colors[] = {"Blue", "Green", "Red", "Yellow", "White", "Off"};
    for(int i=0; i<6; i++) printf(" %s %s\n", (selected == i ? ">" : " "), colors[i]);
    printf("\n\x1b[20;1HPress A to set LED, B to return.");
}

void playMiniGame(int& guess, int& target, int& attempts, bool& won) {
    clearUI();
    printf("\x1b[1;1H\x1b[35m--- Fox Guess (1-100) ---\x1b[0m\n\n");
    if(won) {
        printf(" CONGRATS! You found it in %d tries!\n", attempts);
        printf(" Press A to play again.");
    } else {
        printf(" Current Guess: %d\n", guess);
        printf(" Attempts: %d\n", attempts);
        printf("\n Use D-Pad Up/Down to change, A to guess.");
    }
    printf("\n\x1b[20;1HPress B to return.");
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    PrintConsole topScreen, bottomScreen;
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    int selected = 0;
    int subSelected = 0;
    int brightness = 50;
    MenuState state = MAIN_MENU;
    
    // Game variables
    int guess = 50, target = rand() % 100 + 1, attempts = 0;
    bool won = false;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        // Bottom Screen Status
        consoleSelect(&bottomScreen);
        printf("\x1b[1;1H\x1b[31mDarkFox-3DS Status\x1b[0m\n");
        printf("--------------------------\n");
        u8 bat; MCUHWC_GetBatteryLevel(&bat);
        printf("Battery: %d%%  |  State: %s\n", bat, (state == MAIN_MENU ? "Main Menu" : "Sub-Menu"));

        consoleSelect(&topScreen);
        if (state == MAIN_MENU) {
            clearUI();
            printf("\x1b[1;1H\x1b[31mDarkFox-3DS Utility v1.2.0\x1b[0m\n\n");
            const char* mainOpts[] = {"System Info", "File Browser", "Power Options", "LED Fun", "Mini-Game", "About"};
            for(int i=0; i<6; i++) printf(" %s %d. %s\n", (selected == i ? ">" : " "), i+1, mainOpts[i]);
            
            if (kDown & KEY_DUP) selected = (selected - 1 + 6) % 6;
            if (kDown & KEY_DDOWN) selected = (selected + 1) % 6;
            if (kDown & KEY_A) {
                subSelected = 0;
                if (selected == 0) state = SYSTEM_INFO;
                else if (selected == 1) state = FILE_BROWSER;
                else if (selected == 2) state = POWER_MENU;
                else if (selected == 3) state = LED_FUN;
                else if (selected == 4) state = MINI_GAME;
                else if (selected == 5) state = ABOUT;
            }
        } else {
            if (state == SYSTEM_INFO) showSystemInfo();
            else if (state == POWER_MENU) {
                showPowerMenu(subSelected);
                if (kDown & KEY_DUP) subSelected = (subSelected - 1 + 3) % 3;
                if (kDown & KEY_DDOWN) subSelected = (subSelected + 1) % 3;
                if (kDown & KEY_A) {
                    if(subSelected == 0) PTMU_RebootSystem();
                    if(subSelected == 1) PTMU_PowerOffSystem();
                    if(subSelected == 2) break; // Exit to Home
                }
            }
            else if (state == LED_FUN) {
                showLEDFun(subSelected);
                if (kDown & KEY_DUP) subSelected = (subSelected - 1 + 6) % 6;
                if (kDown & KEY_DDOWN) subSelected = (subSelected + 1) % 6;
                if (kDown & KEY_A) {
                    // Simple LED logic (requires light service)
                }
            }
            else if (state == MINI_GAME) {
                playMiniGame(guess, target, attempts, won);
                if(!won) {
                    if(kDown & KEY_DUP) guess++;
                    if(kDown & KEY_DDOWN) guess--;
                    if(kDown & KEY_A) {
                        attempts++;
                        if(guess == target) won = true;
                    }
                } else if(kDown & KEY_A) {
                    won = false; attempts = 0; target = rand() % 100 + 1;
                }
            }
            else if (state == ABOUT) {
                clearUI();
                printf("\x1b[1;1H\x1b[35m--- DarkFox-3DS ---\x1b[0m\n\n");
                printf(" Version: 1.2.0 (Mega Update)\n");
                printf(" Developer: DarkFox Team\n\n");
                printf(" Features: Power, LED, Games, Info.\n");
                printf("\n\x1b[20;1HPress B to return.");
            }
            if (kDown & KEY_B) state = MAIN_MENU;
        }
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
