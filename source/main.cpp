#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/statvfs.h>

// Menu states
enum MenuState {
    MAIN_MENU,
    SYSTEM_INFO,
    FILE_BROWSER,
    TOOLS,
    ABOUT
};

void printMainMenu(int selected) {
    consoleClear();
    printf("\x1b[1;1H\x1b[31mDarkFox-3DS Utility\x1b[0m");
    printf("\x1b[3;1HUse D-Pad to navigate, A to select, START to exit.");
    
    printf("\x1b[5;1H%s 1. System Information", (selected == 0 ? ">" : " "));
    printf("\x1b[6;1H%s 2. File Browser", (selected == 1 ? ">" : " "));
    printf("\x1b[7;1H%s 3. Tools", (selected == 2 ? ">" : " "));
    printf("\x1b[8;1H%s 4. About", (selected == 3 ? ">" : " "));
}

void showSystemInfo() {
    consoleClear();
    printf("\x1b[1;1H\x1b[32m--- System Information ---\x1b[0m");
    
    u8 model;
    cfguInit();
    CFGU_GetSystemModel(&model);
    cfguExit();
    
    const char* modelName = "Unknown";
    switch(model) {
        case 0: modelName = "Old 3DS"; break;
        case 1: modelName = "Old 3DS XL"; break;
        case 2: modelName = "New 3DS"; break;
        case 3: modelName = "Old 2DS"; break;
        case 4: modelName = "New 3DS XL"; break;
        case 5: modelName = "New 2DS XL"; break;
    }
    
    u8 batteryPercent;
    u8 batteryCharging;
    MCUHWC_GetBatteryLevel(&batteryPercent);
    PTMU_GetBatteryChargeState(&batteryCharging);
    
    struct statvfs stats;
    statvfs("sdmc:/", &stats);
    double freeSpace = (double)stats.f_bavail * stats.f_frsize / (1024 * 1024 * 1024);

    printf("\x1b[3;1HModel: %s", modelName);
    printf("\x1b[4;1HBattery: %d%% (%s)", batteryPercent, (batteryCharging ? "Charging" : "Not Charging"));
    printf("\x1b[5;1HSD Free Space: %.2f GB", freeSpace);
    printf("\x1b[10;1HPress B to return.");
}

void showAbout() {
    consoleClear();
    printf("\x1b[1;1H\x1b[35m--- About DarkFox-3DS ---\x1b[0m");
    printf("\x1b[3;1HVersion: 1.0.0");
    printf("\x1b[4;1HDeveloper: DarkFox Team");
    printf("\x1b[6;1HA multi-purpose utility for the 3DS.");
    printf("\x1b[7;1HCreated for the Homebrew Community.");
    printf("\x1b[10;1HPress B to return.");
}

int main(int argc, char* argv[]) {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    int selected = 0;
    MenuState state = MAIN_MENU;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if (state == MAIN_MENU) {
            printMainMenu(selected);
            if (kDown & KEY_DUP) selected = (selected - 1 + 4) % 4;
            if (kDown & KEY_DDOWN) selected = (selected + 1) % 4;
            if (kDown & KEY_A) {
                if (selected == 0) state = SYSTEM_INFO;
                else if (selected == 1) state = FILE_BROWSER;
                else if (selected == 2) state = TOOLS;
                else if (selected == 3) state = ABOUT;
            }
        } else {
            if (state == SYSTEM_INFO) showSystemInfo();
            else if (state == ABOUT) showAbout();
            else {
                consoleClear();
                printf("\x1b[1;1HFeature coming soon!");
                printf("\x1b[3;1HPress B to return.");
            }
            
            if (kDown & KEY_B) state = MAIN_MENU;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
