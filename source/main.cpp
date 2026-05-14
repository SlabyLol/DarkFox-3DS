#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

// ─────────────────────────────────────────────
//  Enum & Konstanten
// ─────────────────────────────────────────────
enum MenuState {
    MAIN_MENU, SYSTEM_INFO, FILE_BROWSER,
    POWER_MENU, LED_FUN, MINI_GAME, ABOUT
};

#define FB_MAX_ENTRIES 64
#define FB_NAME_LEN    48
#define FB_VISIBLE     16   // Zeilen auf dem Bildschirm

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

// ─────────────────────────────────────────────
//  System Info
// ─────────────────────────────────────────────
void showSystemInfo() {
    resetCursor();
    printf("\x1b[32m--- System Info ---\x1b[0m\n\n");

    u8 model = 0;
    cfguInit(); CFGU_GetSystemModel(&model); cfguExit();
    const char* models[] = {
        "Old 3DS","Old 3DS XL","New 3DS",
        "Old 2DS","New 3DS XL","New 2DS XL"
    };

    u8 batPct = 0, batChg = 0;
    MCUHWC_GetBatteryLevel(&batPct);
    PTMU_GetBatteryChargeState(&batChg);

    u32 wifi = 0;
    ACU_GetWifiStatus(&wifi);

    u8 slider3d = 0;
    MCUHWC_Get3dSliderLevel(&slider3d);

    printf(" Model:   %-22s\n", model < 6 ? models[model] : "Unknown");
    printf(" Battery: %d%% (%s)         \n",
           batPct, batChg ? "Charging    " : "Not Charging");
    printf(" IP:      %-22s\n", getIP());
    printf(" WiFi:    %-22s\n", wifi ? "Connected   " : "Disconnected");
    printf(" 3D Sld:  %d / 255            \n", slider3d);
    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  Power Menu
// ─────────────────────────────────────────────
void showPowerMenu(int sel) {
    resetCursor();
    printf("\x1b[31m--- Power Options ---\x1b[0m\n\n");
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
//  LED Fun  (nur was libctru 2.3 hat)
//  - Wifi-LED an/aus
//  - Power-LED Zustände
// ─────────────────────────────────────────────
static const char* ledOpts[] = {
    "Wifi LED: ON     ",
    "Wifi LED: OFF    ",
    "Power LED: Normal",
    "Power LED: Slow  ",
    "Power LED: Fast  ",
    "Power LED: Off   "
};
#define LED_OPT_COUNT 6

void showLEDFun(int sel) {
    resetCursor();
    printf("\x1b[36m--- LED Fun ---\x1b[0m\n\n");
    for(int i = 0; i < LED_OPT_COUNT; i++)
        printf(" %s %s\n", sel == i ? ">" : " ", ledOpts[i]);
    printf("\n\x1b[20;1HPress A to activate, B to return.");
}

void activateLED(int sel) {
    switch(sel) {
        case 0: MCUHWC_SetWifiLedState(true);  break;
        case 1: MCUHWC_SetWifiLedState(false); break;
        case 2: MCUHWC_SetPowerLedState(LED_NORMAL);     break;
        case 3: MCUHWC_SetPowerLedState(LED_SLEEP_MODE); break;
        case 4: MCUHWC_SetPowerLedState(LED_OFF);        break;
        case 5: MCUHWC_SetPowerLedState(LED_OFF);        break;
    }
}

// ─────────────────────────────────────────────
//  Mini-Game
// ─────────────────────────────────────────────
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
        printf("\n D-Pad Up/Down to change\n A to guess.\n");
    }
    printf("\n\x1b[20;1HPress B to return.");
}

// ─────────────────────────────────────────────
//  File Browser
// ─────────────────────────────────────────────
struct FBState {
    char path[256];           // aktuelles Verzeichnis
    char entries[FB_MAX_ENTRIES][FB_NAME_LEN];
    bool isDir[FB_MAX_ENTRIES];
    int  count;
    int  selected;
    int  scrollTop;
    bool confirmDelete;       // Lösch-Dialog offen?
    char message[64];         // Status-Meldung
    int  msgTimer;
};

void fbLoad(FBState& fb) {
    fb.count = 0;
    fb.selected = 0;
    fb.scrollTop = 0;
    fb.confirmDelete = false;
    fb.message[0] = '\0';
    fb.msgTimer = 0;

    DIR* dir = opendir(fb.path);
    if(!dir) return;

    // ".." immer als ersten Eintrag
    if(strcmp(fb.path, "sdmc:/") != 0) {
        strncpy(fb.entries[fb.count], "..", FB_NAME_LEN-1);
        fb.isDir[fb.count] = true;
        fb.count++;
    }

    struct dirent* ent;
    while((ent = readdir(dir)) != NULL && fb.count < FB_MAX_ENTRIES) {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        strncpy(fb.entries[fb.count], ent->d_name, FB_NAME_LEN-1);
        fb.entries[fb.count][FB_NAME_LEN-1] = '\0';

        char fullPath[320];
        snprintf(fullPath, sizeof(fullPath), "%s%s", fb.path, ent->d_name);
        struct stat st;
        fb.isDir[fb.count] = (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode));
        fb.count++;
    }
    closedir(dir);
}

void fbNavigateInto(FBState& fb) {
    if(fb.count == 0) return;
    const char* name = fb.entries[fb.selected];

    if(strcmp(name, "..") == 0) {
        // Ein Level höher
        size_t len = strlen(fb.path);
        if(len > 7) { // mindestens "sdmc:/x"
            // Letzten '/' entfernen, dann bis zum vorherigen '/' kürzen
            char tmp[256];
            strncpy(tmp, fb.path, sizeof(tmp));
            tmp[len-1] = '\0'; // trailing '/' weg
            char* slash = strrchr(tmp, '/');
            if(slash) {
                *(slash+1) = '\0';
                strncpy(fb.path, tmp, sizeof(fb.path));
            }
        }
    } else if(fb.isDir[fb.selected]) {
        size_t len = strlen(fb.path);
        if(len + strlen(name) + 2 < sizeof(fb.path)) {
            strncat(fb.path, name, sizeof(fb.path) - len - 2);
            strncat(fb.path, "/", sizeof(fb.path) - strlen(fb.path) - 1);
        }
    }
    fbLoad(fb);
}

void fbDelete(FBState& fb) {
    if(fb.count == 0 || strcmp(fb.entries[fb.selected], "..") == 0) return;

    char fullPath[320];
    snprintf(fullPath, sizeof(fullPath), "%s%s", fb.path, fb.entries[fb.selected]);

    int result;
    if(fb.isDir[fb.selected])
        result = rmdir(fullPath);
    else
        result = remove(fullPath);

    if(result == 0)
        snprintf(fb.message, sizeof(fb.message), "Deleted!");
    else
        snprintf(fb.message, sizeof(fb.message), "Delete failed!");

    fb.msgTimer = 90; // ~3 Sekunden bei 30fps
    fbLoad(fb);
}

void showFileBrowser(FBState& fb) {
    resetCursor();
    // Pfad oben anzeigen (gekürzt wenn zu lang)
    char shortPath[38];
    strncpy(shortPath, fb.path, 37);
    shortPath[37] = '\0';
    printf("\x1b[33m%-38s\x1b[0m\n", shortPath);
    printf("------------------------------------ \n");

    if(fb.count == 0) {
        printf(" (empty)                             \n");
    } else {
        int end = fb.scrollTop + FB_VISIBLE;
        if(end > fb.count) end = fb.count;
        for(int i = fb.scrollTop; i < end; i++) {
            bool isSel = (i == fb.selected);
            // Typ-Icon
            const char* icon = fb.isDir[i] ? "[D]" : "[F]";
            // Name kürzen
            char name[28];
            strncpy(name, fb.entries[i], 27);
            name[27] = '\0';
            printf(" %s %s %-27s\n",
                   isSel ? ">" : " ", icon, name);
        }
        // restliche Zeilen leer halten
        for(int i = end - fb.scrollTop; i < FB_VISIBLE; i++)
            printf("                                     \n");
    }

    // Scrollbar-Info
    printf("----  %d/%d  ", fb.selected+1, fb.count);
    if(fb.msgTimer > 0) {
        printf("%-20s\n", fb.message);
        fb.msgTimer--;
    } else {
        printf("                    \n");
    }

    // Steuerung
    printf("\x1b[20;1H");
    if(fb.confirmDelete) {
        printf("\x1b[31mDelete? A=Yes  B=No         \x1b[0m");
    } else {
        printf("A=Open/Enter  X=Delete  B=Back");
    }
}

void fbHandleInput(FBState& fb, u32 kDown) {
    if(fb.confirmDelete) {
        if(kDown & KEY_A) fbDelete(fb);
        if(kDown & KEY_A || kDown & KEY_B) fb.confirmDelete = false;
        return;
    }

    if(kDown & KEY_DUP) {
        if(fb.selected > 0) {
            fb.selected--;
            if(fb.selected < fb.scrollTop) fb.scrollTop = fb.selected;
        }
    }
    if(kDown & KEY_DDOWN) {
        if(fb.selected < fb.count - 1) {
            fb.selected++;
            if(fb.selected >= fb.scrollTop + FB_VISIBLE)
                fb.scrollTop = fb.selected - FB_VISIBLE + 1;
        }
    }
    if(kDown & KEY_A) {
        if(fb.isDir[fb.selected] || strcmp(fb.entries[fb.selected], "..") == 0)
            fbNavigateInto(fb);
    }
    if(kDown & KEY_X) {
        // Kein Löschen von ".."
        if(strcmp(fb.entries[fb.selected], "..") != 0)
            fb.confirmDelete = true;
    }
}

// ─────────────────────────────────────────────
//  About
// ─────────────────────────────────────────────
void showAbout() {
    resetCursor();
    printf("\x1b[35m--- DarkFox-3DS ---\x1b[0m\n\n");
    printf(" Version:   1.3.0\n");
    printf(" Developer: DarkFox Team\n\n");
    printf(" Features:\n");
    printf("  - System Info\n");
    printf("  - File Browser (A/X/B)\n");
    printf("  - LED Control\n");
    printf("  - Mini-Game\n");
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

    // File Browser initialisieren
    FBState fb;
    memset(&fb, 0, sizeof(fb));
    strncpy(fb.path, "sdmc:/", sizeof(fb.path)-1);
    fbLoad(fb);

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

        bool stateChanged = (state != prevState);
        if(stateChanged) {
            consoleSelect(&topScreen);    fullClear();
            consoleSelect(&bottomScreen); fullClear();
            prevState = state;
            // File Browser neu laden wenn wir rein gehen
            if(state == FILE_BROWSER) fbLoad(fb);
        }

        // Bottom Screen – Statusbar
        consoleSelect(&bottomScreen);
        printf("\x1b[H\x1b[31mDarkFox-3DS\x1b[0m\n");
        printf("----------------------------\n");
        u8 bat = 0; MCUHWC_GetBatteryLevel(&bat);
        printf("Battery: %d%%  State: %-10s\n", bat,
               state == MAIN_MENU ? "Main Menu" : "Sub-Menu");

        // Top Screen
        consoleSelect(&topScreen);

        if(state == MAIN_MENU) {
            resetCursor();
            printf("\x1b[31mDarkFox-3DS v1.3.0\x1b[0m\n\n");
            const char* opts[] = {
                "System Info  ","File Browser ","Power Options",
                "LED Fun      ","Mini-Game    ","About        "
            };
            for(int i = 0; i < 6; i++)
                printf(" %s %d. %s\n", selected == i ? ">" : " ", i+1, opts[i]);

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
                    if(subSelected == 0) break;
                    if(subSelected == 1) NS_RebootSystem();
                    if(subSelected == 2) break;
                }
            }
            else if(state == LED_FUN) {
                showLEDFun(subSelected);
                if(kDown & KEY_DUP)   subSelected = (subSelected - 1 + LED_OPT_COUNT) % LED_OPT_COUNT;
                if(kDown & KEY_DDOWN) subSelected = (subSelected + 1) % LED_OPT_COUNT;
                if(kDown & KEY_A)     activateLED(subSelected);
            }
            else if(state == MINI_GAME) {
                playMiniGame(guess, attempts, won);
                if(!won) {
                    if(kDown & KEY_DUP)   { guess++; if(guess > 100) guess = 100; }
                    if(kDown & KEY_DDOWN) { guess--; if(guess <   1) guess =   1; }
                    if(kDown & KEY_A) { attempts++; if(guess == target) won = true; }
                } else if(kDown & KEY_A) {
                    won = false; attempts = 0; guess = 50;
                    target = rand() % 100 + 1;
                }
            }
            else if(state == FILE_BROWSER) {
                fbHandleInput(fb, kDown);
                showFileBrowser(fb);
            }
            else if(state == ABOUT) {
                showAbout();
            }

            // B = zurück ins Hauptmenü (ausser im Lösch-Dialog)
            if((kDown & KEY_B) && state != FILE_BROWSER)
                state = MAIN_MENU;
            if((kDown & KEY_B) && state == FILE_BROWSER && !fb.confirmDelete)
                state = MAIN_MENU;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    acExit();
    ptmuExit();
    mcuHwcExit();
    gfxExit();
    return 0;
}
