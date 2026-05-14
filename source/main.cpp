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

enum MenuState {
    MAIN_MENU, SYSTEM_INFO, FILE_BROWSER, POWER_MENU,
    LED_FUN, MINI_GAME, NETWORK_INFO, HARDWARE_TEST,
    CLOCK_SCREEN, STORAGE_INFO, BUTTON_TEST, ABOUT
};

// ─── Hilfsfunktionen ───────────────────────────────────────────────────────
void resetCursor() { printf("\x1b[H"); }
void fullClear()   { consoleClear(); }

const char* getIP() {
    static char buf[16];
    u32 ip = gethostid();
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
        (int)((ip>> 0)&0xFF),(int)((ip>> 8)&0xFF),
        (int)((ip>>16)&0xFF),(int)((ip>>24)&0xFF));
    return buf;
}

const char* fmtSize(u64 b) {
    static char buf[24];
    if     (b>=1024ULL*1024*1024) snprintf(buf,sizeof(buf),"%.2f GB",(double)b/(1024.0*1024*1024));
    else if(b>=1024ULL*1024)      snprintf(buf,sizeof(buf),"%.2f MB",(double)b/(1024.0*1024));
    else if(b>=1024ULL)           snprintf(buf,sizeof(buf),"%.2f KB",(double)b/1024.0);
    else                          snprintf(buf,sizeof(buf),"%d B",(int)b);
    return buf;
}

// ─── System Info ───────────────────────────────────────────────────────────
void showSystemInfo() {
    resetCursor();
    printf("\x1b[32m=== System Info ===\x1b[0m\n\n");
    u8 model=0; cfguInit(); CFGU_GetSystemModel(&model); cfguExit();
    const char* models[]={"Old 3DS","Old 3DS XL","New 3DS","Old 2DS","New 3DS XL","New 2DS XL"};
    u8 batPct=0,batChg=0;
    MCUHWC_GetBatteryLevel(&batPct);
    PTMU_GetBatteryChargeState(&batChg);
    u8 slider3d=0,sound=0,fwH=0,fwL=0;
    MCUHWC_Get3dSliderLevel(&slider3d);
    MCUHWC_GetSoundSliderLevel(&sound);
    MCUHWC_GetFwVerHigh(&fwH); MCUHWC_GetFwVerLow(&fwL);
    u32 appUsed=(u32)osGetMemRegionUsed(MEMREGION_APPLICATION);
    u32 appTotal=(u32)osGetMemRegionSize(MEMREGION_APPLICATION);
    printf(" Model:    %-20s\n", model<6?models[model]:"Unknown");
    printf(" Battery:  %d%% (%s)    \n", batPct, batChg?"Charging":"Not charging");
    printf(" 3D Sld:   %d/255        \n", slider3d);
    printf(" Sound:    %d/255        \n", sound);
    printf(" MCU FW:   %d.%d         \n", fwH, fwL);
    printf(" RAM used: %s / %s  \n", fmtSize(appUsed), fmtSize(appTotal));
    printf("\n\x1b[20;1HPress B to return.");
}

// ─── Network Info ──────────────────────────────────────────────────────────
void showNetworkInfo() {
    resetCursor();
    printf("\x1b[36m=== Network Info ===\x1b[0m\n\n");
    u32 wifi=0; ACU_GetWifiStatus(&wifi);
    printf(" Status:   %-20s\n", wifi?"Connected   ":"Disconnected");
    printf(" IP Addr:  %-20s\n", getIP());
    // SSID via ACU_GetSSID (korrekte Funktion in libctru 2.3)
    char ssid[33]={0};
    ACU_GetSSID(ssid);
    printf(" SSID:     %-20s\n", ssid[0]?ssid:"(none)");
    printf("\n\x1b[20;1HPress B to return.");
}

// ─── Storage Info ──────────────────────────────────────────────────────────
void showStorageInfo() {
    resetCursor();
    printf("\x1b[33m=== Storage Info ===\x1b[0m\n\n");

    // FSUSER_GetArchiveResource ist die korrekte Funktion
    FS_ArchiveResource sdRes={0}, nandRes={0};
    FSUSER_GetArchiveResource(&sdRes,   SYSTEM_MEDIATYPE_SD);
    FSUSER_GetArchiveResource(&nandRes, SYSTEM_MEDIATYPE_NAND);

    u64 sdTotal  = (u64)sdRes.totalClusters   * sdRes.clusterSize;
    u64 sdFree   = (u64)sdRes.freeClusters    * sdRes.clusterSize;
    u64 nandTotal= (u64)nandRes.totalClusters * nandRes.clusterSize;
    u64 nandFree = (u64)nandRes.freeClusters  * nandRes.clusterSize;

    int sdPct = sdTotal>0?(int)(100-(sdFree*100/sdTotal)):0;

    printf(" SD Card:\n");
    printf("  Total: %-18s\n", fmtSize(sdTotal));
    printf("  Free:  %-18s\n", fmtSize(sdFree));
    printf("  Used:  %-18s\n", fmtSize(sdTotal-sdFree));
    printf("  [");
    for(int i=0;i<20;i++) printf(i<sdPct/5?"#":"-");
    printf("] %d%%\n\n", sdPct);

    printf(" NAND:\n");
    printf("  Total: %-18s\n", fmtSize(nandTotal));
    printf("  Free:  %-18s\n", fmtSize(nandFree));
    printf("\n\x1b[20;1HPress B to return.");
}

// ─── Clock ─────────────────────────────────────────────────────────────────
void showClock() {
    resetCursor();
    printf("\x1b[35m=== Clock & Date ===\x1b[0m\n\n");
    u64 ms=osGetTime(); time_t sec=(time_t)(ms/1000);
    struct tm* t=gmtime(&sec);
    const char* days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    printf("  Date:   %04d-%02d-%02d (%s)\n\n",
           t->tm_year+1900,t->tm_mon+1,t->tm_mday,days[t->tm_wday]);
    printf("  \x1b[32m%02d:%02d:%02d\x1b[0m UTC\n\n",
           t->tm_hour,t->tm_min,t->tm_sec);
    printf("  Uptime: %llu s\n", (unsigned long long)(ms/1000));
    printf("\n\x1b[20;1HPress B to return.");
}

// ─── Hardware Test ─────────────────────────────────────────────────────────
void showHardwareTest() {
    resetCursor();
    printf("\x1b[33m=== Hardware Test ===\x1b[0m\n\n");
    touchPosition touch; hidTouchRead(&touch);
    printf(" Touch:  X=%-4d Y=%-4d        \n", touch.px, touch.py);
    circlePosition circle; hidCircleRead(&circle);
    printf(" Circle: X=%-5d Y=%-5d      \n", circle.dx, circle.dy);
    angularRate gyro; hidGyroRead(&gyro);
    printf(" Gyro:   X=%-6d Y=%-6d    \n", gyro.x, gyro.y);
    printf("         Z=%-6d            \n", gyro.z);
    accelVector accel; hidAccelRead(&accel);
    printf(" Accel:  X=%-6d Y=%-6d    \n", accel.x, accel.y);
    printf("         Z=%-6d            \n", accel.z);
    u8 sl=0,so=0; MCUHWC_Get3dSliderLevel(&sl); MCUHWC_GetSoundSliderLevel(&so);
    printf(" 3D:%d  Sound:%d           \n", sl, so);
    printf("\n\x1b[20;1HPress B to return.");
}

// ─── Button Test ───────────────────────────────────────────────────────────
void showButtonTest(u32 kHeld) {
    resetCursor();
    printf("\x1b[36m=== Button Test ===\x1b[0m\n\n");
    struct{u32 k;const char* n;}b[]={
        {KEY_A,"A"},{KEY_B,"B"},{KEY_X,"X"},{KEY_Y,"Y"},
        {KEY_L,"L"},{KEY_R,"R"},{KEY_ZL,"ZL"},{KEY_ZR,"ZR"},
        {KEY_DUP,"DUp"},{KEY_DDOWN,"DDn"},{KEY_DLEFT,"DLt"},{KEY_DRIGHT,"DRt"},
        {KEY_START,"Sta"},{KEY_SELECT,"Sel"},
        {KEY_CPAD_UP,"CUp"},{KEY_CPAD_DOWN,"CDn"},
    };
    int n=sizeof(b)/sizeof(b[0]);
    for(int i=0;i<n;i++){
        bool h=(kHeld&b[i].k)!=0;
        printf(" %-3s %s  ",b[i].n,h?"\x1b[32m[ON]\x1b[0m":"\x1b[31m[  ]\x1b[0m");
        if(i%2==1) printf("\n");
    }
    printf("\n\x1b[20;1HPress B to return.");
}

// ─── Power Menu ────────────────────────────────────────────────────────────
void showPowerMenu(int sel) {
    resetCursor();
    printf("\x1b[31m=== Power Options ===\x1b[0m\n\n");
    const char* opts[]={"Return to Loader","Reboot System","Return to Home"};
    for(int i=0;i<3;i++)
        printf(" %s %s\n",sel==i?">":"  ",opts[i]);
    printf("\n\x1b[20;1HPress A to confirm, B to return.");
}

// ─── LED Fun ───────────────────────────────────────────────────────────────
#define LED_N 5
const char* ledOpts[LED_N]={
    "Wifi LED: ON          ",
    "Wifi LED: OFF         ",
    "Power LED: Normal     ",
    "Power LED: Sleep Blink",
    "Power LED: OFF        ",
};
void showLEDFun(int sel){
    resetCursor();
    printf("\x1b[36m=== LED Control ===\x1b[0m\n\n");
    for(int i=0;i<LED_N;i++)
        printf(" %s %s\n",sel==i?">":"  ",ledOpts[i]);
    printf("\n\x1b[20;1HPress A to activate, B to return.");
}
void activateLED(int sel){
    switch(sel){
        case 0: MCUHWC_SetWifiLedState(true);            break;
        case 1: MCUHWC_SetWifiLedState(false);           break;
        case 2: MCUHWC_SetPowerLedState(LED_NORMAL);     break;
        case 3: MCUHWC_SetPowerLedState(LED_SLEEP_MODE); break;
        case 4: MCUHWC_SetPowerLedState(LED_OFF);        break;
    }
}

// ─── Mini-Game ─────────────────────────────────────────────────────────────
void playMiniGame(int guess,int attempts,bool won,int target){
    resetCursor();
    printf("\x1b[35m=== Fox Guess (1-100) ===\x1b[0m\n\n");
    if(won){
        printf(" \x1b[32mCONGRATS! Found %d in %d tries!\x1b[0m\n",target,attempts);
        printf(" Press A to play again.        \n\n\n");
    } else {
        printf(" Guess:    %d    \n",guess);
        printf(" Attempts: %d    \n",attempts);
        if(attempts>0){
            if(guess<target) printf(" Hint: \x1b[33mGo HIGHER!\x1b[0m    \n");
            else             printf(" Hint: \x1b[33mGo LOWER! \x1b[0m    \n");
        } else printf("                           \n");
        printf("\n DPad Up/Down: change  A: guess\n");
    }
    printf("\n\x1b[20;1HPress B to return.");
}

// ─── File Browser ──────────────────────────────────────────────────────────
#define FB_MAX 128
#define FB_NL   48
#define FB_VIS  16

struct FBState {
    char path[256];
    char entries[FB_MAX][FB_NL];
    bool isDir[FB_MAX];
    int  count,sel,top;
    bool confirmDel;
    char msg[64];
    int  msgT;
};

void fbLoad(FBState& fb){
    fb.count=fb.sel=fb.top=0;
    fb.confirmDel=false; fb.msg[0]=0; fb.msgT=0;
    DIR* d=opendir(fb.path); if(!d) return;
    if(strcmp(fb.path,"sdmc:/")!=0){
        strncpy(fb.entries[fb.count],"..",FB_NL-1);
        fb.isDir[fb.count++]=true;
    }
    struct dirent* e;
    while((e=readdir(d))&&fb.count<FB_MAX){
        if(e->d_name[0]=='.') continue;
        strncpy(fb.entries[fb.count],e->d_name,FB_NL-1);
        fb.entries[fb.count][FB_NL-1]=0;
        char fp[320]; snprintf(fp,sizeof(fp),"%s%s",fb.path,e->d_name);
        struct stat st; fb.isDir[fb.count]=(stat(fp,&st)==0&&S_ISDIR(st.st_mode));
        fb.count++;
    }
    closedir(d);
}

void fbNav(FBState& fb){
    if(!fb.count) return;
    if(strcmp(fb.entries[fb.sel],"..")==0){
        size_t len=strlen(fb.path);
        if(len>7){
            char tmp[256]; strncpy(tmp,fb.path,sizeof(tmp));
            tmp[len-1]=0; char* s=strrchr(tmp,'/');
            if(s){*(s+1)=0; strncpy(fb.path,tmp,sizeof(fb.path));}
        }
    } else if(fb.isDir[fb.sel]){
        size_t l=strlen(fb.path);
        strncat(fb.path,fb.entries[fb.sel],sizeof(fb.path)-l-2);
        strncat(fb.path,"/",sizeof(fb.path)-strlen(fb.path)-1);
    }
    fbLoad(fb);
}

void fbDel(FBState& fb){
    if(!fb.count||strcmp(fb.entries[fb.sel],"..")==0) return;
    char fp[320]; snprintf(fp,sizeof(fp),"%s%s",fb.path,fb.entries[fb.sel]);
    int r=fb.isDir[fb.sel]?rmdir(fp):remove(fp);
    snprintf(fb.msg,sizeof(fb.msg),r==0?"Deleted!":"Delete failed!");
    fb.msgT=90; fbLoad(fb);
}

void showFB(FBState& fb){
    resetCursor();
    char sp[38]; strncpy(sp,fb.path,37); sp[37]=0;
    printf("\x1b[33m%-38s\x1b[0m\n",sp);
    printf("--------------------------------------\n");
    if(!fb.count){ printf(" (empty)\n"); for(int i=1;i<FB_VIS;i++) printf("\n"); }
    else {
        int end=fb.top+FB_VIS; if(end>fb.count) end=fb.count;
        for(int i=fb.top;i<end;i++){
            bool s=(i==fb.sel);
            char nm[28]; strncpy(nm,fb.entries[i],27); nm[27]=0;
            printf(" %s %s %-27s\n",s?">":"  ",fb.isDir[i]?"\x1b[34m[D]\x1b[0m":"[F]",nm);
        }
        for(int i=end-fb.top;i<FB_VIS;i++) printf("\n");
    }
    printf("-- %d/%d ",fb.sel+1,fb.count);
    if(fb.msgT>0){printf("%-20s\n",fb.msg);fb.msgT--;}else printf("\n");
    printf("\x1b[20;1H");
    if(fb.confirmDel) printf("\x1b[31mDelete? A=Yes B=No\x1b[0m          ");
    else              printf("A=Enter  X=Delete  B=Back       ");
}

void fbInput(FBState& fb,u32 kDown){
    if(fb.confirmDel){
        if(kDown&KEY_A) fbDel(fb);
        if(kDown&(KEY_A|KEY_B)) fb.confirmDel=false;
        return;
    }
    if(kDown&KEY_DUP&&fb.sel>0){fb.sel--;if(fb.sel<fb.top)fb.top=fb.sel;}
    if(kDown&KEY_DDOWN&&fb.sel<fb.count-1){
        fb.sel++;if(fb.sel>=fb.top+FB_VIS)fb.top=fb.sel-FB_VIS+1;
    }
    if(kDown&KEY_A) fbNav(fb);
    if(kDown&KEY_X&&fb.count&&strcmp(fb.entries[fb.sel],"..")!=0)
        fb.confirmDel=true;
}

// ─── About ─────────────────────────────────────────────────────────────────
void showAbout(){
    resetCursor();
    printf("\x1b[35m=== DarkFox-3DS v2.0 ===\x1b[0m\n\n");
    printf(" Developer: DarkFox Team\n\n");
    printf("  1. System Info\n");
    printf("  2. File Browser (A/X/B)\n");
    printf("  3. Power Options\n");
    printf("  4. LED Control\n");
    printf("  5. Mini-Game w/ Hints\n");
    printf("  6. Network Info + SSID\n");
    printf("  7. Hardware Test\n");
    printf("  8. Clock & Date\n");
    printf("  9. Storage Info\n");
    printf("  0. Button Tester\n");
    printf("\n\x1b[20;1HPress B to return.");
}

// ─── Main ──────────────────────────────────────────────────────────────────
int main(int argc,char* argv[]){
    gfxInitDefault();
    PrintConsole top,bot;
    consoleInit(GFX_TOP,&top);
    consoleInit(GFX_BOTTOM,&bot);
    mcuHwcInit(); ptmuInit(); acInit(); hidInit();

    FBState fb; memset(&fb,0,sizeof(fb));
    strncpy(fb.path,"sdmc:/",sizeof(fb.path)-1); fbLoad(fb);

    int sel=0,subSel=0;
    MenuState state=MAIN_MENU,prev=(MenuState)-1;
    int guess=50,target=1,attempts=0; bool won=false;
    srand((unsigned int)osGetTime()); target=rand()%100+1;

    const char* opts[]={
        "System Info  ","File Browser ","Power Options","LED Control  ",
        "Mini-Game    ","Network Info ","Hardware Test","Clock & Date ",
        "Storage Info ","Button Test  ","About        "
    };
    const int N=11;

    while(aptMainLoop()){
        hidScanInput();
        u32 kDown=hidKeysDown(), kHeld=hidKeysHeld();
        if(kDown&KEY_START) break;

        bool changed=(state!=prev);
        if(changed){
            consoleSelect(&top); fullClear();
            consoleSelect(&bot); fullClear();
            prev=state;
            if(state==FILE_BROWSER) fbLoad(fb);
        }

        // Bottom – Live Status
        consoleSelect(&bot);
        printf("\x1b[H\x1b[31mDarkFox-3DS v2.0\x1b[0m\n");
        printf("----------------------------\n");
        u8 bat=0,batChg=0;
        MCUHWC_GetBatteryLevel(&bat); PTMU_GetBatteryChargeState(&batChg);
        printf("Bat: %d%% %s\n",bat,batChg?"\x1b[32m[CHG]\x1b[0m":"     ");
        u64 ms=osGetTime(); time_t sec=(time_t)(ms/1000);
        struct tm* t=gmtime(&sec);
        printf("UTC: %02d:%02d:%02d\n",t->tm_hour,t->tm_min,t->tm_sec);
        u32 wifi=0; ACU_GetWifiStatus(&wifi);
        printf("Net: %s\n",wifi?"\x1b[32mConnected\x1b[0m   ":"\x1b[31mNo WiFi\x1b[0m     ");
        printf("IP:  %s\n",getIP());

        // Top
        consoleSelect(&top);

        if(state==MAIN_MENU){
            resetCursor();
            printf("\x1b[31mDarkFox-3DS v2.0\x1b[0m\n\n");
            for(int i=0;i<N;i++)
                printf(" %s %2d. %s\n",sel==i?">":"  ",i+1,opts[i]);
            if(kDown&KEY_DUP)   sel=(sel-1+N)%N;
            if(kDown&KEY_DDOWN) sel=(sel+1)%N;
            if(kDown&KEY_A){
                subSel=0;
                switch(sel){
                    case 0: state=SYSTEM_INFO;   break;
                    case 1: state=FILE_BROWSER;  break;
                    case 2: state=POWER_MENU;    break;
                    case 3: state=LED_FUN;       break;
                    case 4: state=MINI_GAME;     break;
                    case 5: state=NETWORK_INFO;  break;
                    case 6: state=HARDWARE_TEST; break;
                    case 7: state=CLOCK_SCREEN;  break;
                    case 8: state=STORAGE_INFO;  break;
                    case 9: state=BUTTON_TEST;   break;
                    case 10:state=ABOUT;         break;
                }
            }
        } else {
            switch(state){
                case SYSTEM_INFO:   showSystemInfo();        break;
                case NETWORK_INFO:  showNetworkInfo();       break;
                case STORAGE_INFO:  showStorageInfo();       break;
                case CLOCK_SCREEN:  showClock();             break;
                case HARDWARE_TEST: showHardwareTest();      break;
                case BUTTON_TEST:   showButtonTest(kHeld);   break;
                case ABOUT:         showAbout();             break;
                case POWER_MENU:
                    showPowerMenu(subSel);
                    if(kDown&KEY_DUP)   subSel=(subSel-1+3)%3;
                    if(kDown&KEY_DDOWN) subSel=(subSel+1)%3;
                    if(kDown&KEY_A){
                        if(subSel==0) goto done;
                        if(subSel==1) NS_RebootSystem();
                        if(subSel==2) goto done;
                    }
                    break;
                case LED_FUN:
                    showLEDFun(subSel);
                    if(kDown&KEY_DUP)   subSel=(subSel-1+LED_N)%LED_N;
                    if(kDown&KEY_DDOWN) subSel=(subSel+1)%LED_N;
                    if(kDown&KEY_A) activateLED(subSel);
                    break;
                case MINI_GAME:
                    playMiniGame(guess,attempts,won,target);
                    if(!won){
                        if(kDown&KEY_DUP)   {guess++;if(guess>100)guess=100;}
                        if(kDown&KEY_DDOWN) {guess--;if(guess<1)  guess=1;  }
                        if(kDown&KEY_A){attempts++;if(guess==target)won=true;}
                    } else if(kDown&KEY_A){
                        won=false;attempts=0;guess=50;target=rand()%100+1;
                    }
                    break;
                case FILE_BROWSER:
                    fbInput(fb,kDown); showFB(fb);
                    break;
                default: break;
            }
            bool backOk=(state==FILE_BROWSER&&!fb.confirmDel)||state!=FILE_BROWSER;
            if((kDown&KEY_B)&&backOk) state=MAIN_MENU;
        }
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
    }
done:
    acExit(); ptmuExit(); mcuHwcExit(); gfxExit();
    return 0;
}
