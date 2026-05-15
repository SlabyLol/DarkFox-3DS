// DarkFox-3DS v4.0 - libctru 2.3 verified
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

enum MenuState {
    MAIN_MENU=0, SYSTEM_INFO, FILE_BROWSER, POWER_MENU,
    LED_FUN, MINI_GAME, NETWORK_INFO, HW_TEST,
    CLOCK_VIEW, STORAGE_INFO, BTN_TEST, SNAKE_GAME,
    CALC, REACTION_GAME, COLOR_TEST, ABOUT
};

#define FB_MAX 128
#define FB_NL   48
#define FB_VIS  15
#define LED_N    5

// ═══════════════════════════════════════════════
//  UTILITIES
// ═══════════════════════════════════════════════
void cur() { printf("\x1b[H"); }
void cls() { consoleClear(); }
void gotoxy(int x,int y){printf("\x1b[%d;%dH",y,x);}

const char* ip4str() {
    static char b[16];
    u32 ip=gethostid();
    snprintf(b,16,"%d.%d.%d.%d",
        (int)(ip&0xFF),(int)((ip>>8)&0xFF),
        (int)((ip>>16)&0xFF),(int)((ip>>24)&0xFF));
    return b;
}
const char* szStr(u64 b){
    static char buf[24];
    if     (b>=1024ULL*1024*1024)snprintf(buf,24,"%.2fGB",(double)b/(1024.0*1024*1024));
    else if(b>=1024ULL*1024)     snprintf(buf,24,"%.2fMB",(double)b/(1024.0*1024));
    else if(b>=1024ULL)          snprintf(buf,24,"%.2fKB",(double)b/1024.0);
    else                         snprintf(buf,24,"%dB",(int)b);
    return buf;
}
void bar(int pct,int w){
    printf("[");
    for(int i=0;i<w;i++) printf(i<pct*w/100?"#":"-");
    printf("]%d%%",pct);
}

// ═══════════════════════════════════════════════
//  BOTTOM SCREEN - RICH STATUS PANEL
// ═══════════════════════════════════════════════
// Bottom screen is 40 cols x 30 rows (console mode)
// We use all of it for a dashboard

static MenuState g_state=MAIN_MENU;
static int       g_snakeScore=0;
static int       g_guessAttempts=0;

void drawBottom(){
    printf("\x1b[H"); // cursor to top-left of bottom screen

    // ── Row 1-2: Header ──
    printf("\x1b[31m╔══ DarkFox-3DS v4.0 ════╗\x1b[0m\n");

    // ── Row 3-5: Battery ──
    u8 bat=0,chg=0;
    MCUHWC_GetBatteryLevel(&bat);
    PTMU_GetBatteryChargeState(&chg);
    const char* batCol=(bat>50)?"\x1b[32m":(bat>20)?"\x1b[33m":"\x1b[31m";
    printf("%sBat:%3d%%%s %s\n",batCol,bat,"\x1b[0m",chg?"\x1b[33m⚡CHG\x1b[0m":"     ");
    // battery bar (20 wide)
    int bw=(int)(bat*20/100);
    printf("[");
    for(int i=0;i<20;i++) printf(i<bw?(bat>50?"#":(bat>20?"=":" ")):"_");
    printf("]\n");

    // ── Row 6-7: Clock ──
    u64 ms=osGetTime(); time_t s=(time_t)(ms/1000);
    struct tm* t=gmtime(&s);
    printf("\x1b[36mUTC \x1b[32m%02d:%02d:%02d\x1b[0m %02d/%02d/%04d\n",
           t->tm_hour,t->tm_min,t->tm_sec,
           t->tm_mday,t->tm_mon+1,t->tm_year+1900);

    // ── Row 8-9: Network ──
    u32 wifi=0; ACU_GetWifiStatus(&wifi);
    printf("Net:%s  IP:%-15s\n",
           wifi?"\x1b[32mONLINE \x1b[0m":"\x1b[31mOFFLINE\x1b[0m",
           ip4str());

    // ── Row 10: Sliders ──
    u8 sl3d=0,slSnd=0;
    MCUHWC_Get3dSliderLevel(&sl3d);
    MCUHWC_GetSoundSliderLevel(&slSnd);
    printf("3D:%-3d  Vol:%-3d  ",sl3d,slSnd);
    // wifi bars
    if(wifi){
        printf("\x1b[32m▂▄▆█\x1b[0m");
    } else {
        printf("\x1b[31m----\x1b[0m");
    }
    printf("\n");

    // ── Row 11: RAM ──
    u32 ramUsed=(u32)osGetMemRegionUsed(MEMREGION_APPLICATION);
    u32 ramTot =(u32)osGetMemRegionSize(MEMREGION_APPLICATION);
    int rp=ramTot>0?(int)(ramUsed*100/ramTot):0;
    printf("RAM[");
    for(int i=0;i<16;i++) printf(i<rp*16/100?"#":"-");
    printf("]%d%%\n",rp);

    // ── Row 12: Divider ──
    printf("\x1b[31m╠════════════════════════╣\x1b[0m\n");

    // ── Row 13-14: Touch + Circle ──
    touchPosition tp; hidTouchRead(&tp);
    circlePosition cp; hidCircleRead(&cp);
    printf("Touch:X=%-4d Y=%-4d      \n",tp.px,tp.py);
    printf("Stick:X=%-5d Y=%-5d    \n",cp.dx,cp.dy);

    // ── Row 15: Held buttons summary ──
    u32 held=hidKeysHeld();
    printf("Btn:");
    if(held&KEY_A)     printf("\x1b[32mA\x1b[0m");else printf(".");
    if(held&KEY_B)     printf("\x1b[32mB\x1b[0m");else printf(".");
    if(held&KEY_X)     printf("\x1b[32mX\x1b[0m");else printf(".");
    if(held&KEY_Y)     printf("\x1b[32mY\x1b[0m");else printf(".");
    if(held&KEY_L)     printf("\x1b[32mL\x1b[0m");else printf(".");
    if(held&KEY_R)     printf("\x1b[32mR\x1b[0m");else printf(".");
    if(held&KEY_DUP)   printf("\x1b[32m^\x1b[0m");else printf(".");
    if(held&KEY_DDOWN) printf("\x1b[32mv\x1b[0m");else printf(".");
    if(held&KEY_DLEFT) printf("\x1b[32m<\x1b[0m");else printf(".");
    if(held&KEY_DRIGHT)printf("\x1b[32m>\x1b[0m");else printf(".");
    if(held&KEY_START) printf("\x1b[33mST\x1b[0m");else printf("..");
    printf("\n");

    // ── Row 16: Divider ──
    printf("\x1b[31m╠════════════════════════╣\x1b[0m\n");

    // ── Row 17-19: Context info depending on state ──
    const char* stateName="";
    switch(g_state){
        case MAIN_MENU:    stateName="Main Menu";    break;
        case SYSTEM_INFO:  stateName="System Info";  break;
        case FILE_BROWSER: stateName="File Browser"; break;
        case POWER_MENU:   stateName="Power Menu";   break;
        case LED_FUN:      stateName="LED Control";  break;
        case MINI_GAME:    stateName="Mini-Game";    break;
        case NETWORK_INFO: stateName="Network";      break;
        case HW_TEST:      stateName="HW Test";      break;
        case CLOCK_VIEW:   stateName="Clock";        break;
        case STORAGE_INFO: stateName="Storage";      break;
        case BTN_TEST:     stateName="Btn Test";     break;
        case SNAKE_GAME:   stateName="Snake";        break;
        case CALC:         stateName="Calculator";   break;
        case REACTION_GAME:stateName="Reaction";     break;
        case COLOR_TEST:   stateName="Color Test";   break;
        case ABOUT:        stateName="About";        break;
        default:           stateName="???";          break;
    }
    printf("\x1b[33mMode: %-18s\x1b[0m\n",stateName);

    // Context-specific info
    if(g_state==SNAKE_GAME)
        printf("Score: \x1b[32m%d\x1b[0m                  \n",g_snakeScore);
    else if(g_state==MINI_GAME)
        printf("Attempts: \x1b[33m%d\x1b[0m               \n",g_guessAttempts);
    else {
        // uptime
        unsigned long up=(unsigned long)(ms/1000);
        printf("Up: %luh%02lum%02lus              \n",up/3600,(up%3600)/60,up%60);
    }

    // Gyro mini display
    angularRate gy; hidGyroRead(&gy);
    int gx=gy.x/200, gz=gy.z/200; // scale down
    if(gx>5)gx=5;if(gx<-5)gx=-5;
    if(gz>5)gz=5;if(gz<-5)gz=-5;
    printf("Gyro[");
    for(int i=-5;i<=5;i++) printf(i==gx?"\x1b[36mX\x1b[0m":(i==0?"|":" "));
    printf("]\n");

    // ── Row 20: Footer ──
    printf("\x1b[31m╚════════════════════════╝\x1b[0m");
}

// ═══════════════════════════════════════════════
//  1. SYSTEM INFO
// ═══════════════════════════════════════════════
void showSystemInfo(){
    cur();
    printf("\x1b[32m== System Info ==\x1b[0m\n\n");
    u8 model=0; cfguInit(); CFGU_GetSystemModel(&model); cfguExit();
    const char* mdl[]={"Old3DS","Old3DSXL","New3DS","Old2DS","New3DSXL","New2DSXL"};
    u8 bat=0,chg=0,sl3d=0,slSnd=0,fwH=0,fwL=0;
    MCUHWC_GetBatteryLevel(&bat); PTMU_GetBatteryChargeState(&chg);
    MCUHWC_Get3dSliderLevel(&sl3d); MCUHWC_GetSoundSliderLevel(&slSnd);
    MCUHWC_GetFwVerHigh(&fwH); MCUHWC_GetFwVerLow(&fwL);
    u32 used=(u32)osGetMemRegionUsed(MEMREGION_APPLICATION);
    u32 total=(u32)osGetMemRegionSize(MEMREGION_APPLICATION);
    char sysVer[0x60]={0};
    osGetSystemVersionDataString(NULL,NULL,sysVer,0x60);
    printf(" Model:  %-16s\n",model<6?mdl[model]:"Unknown");
    printf(" Bat:    %d%% %s\n",bat,chg?"[CHG]":"");
    printf(" 3DSld:  %d/255\n",sl3d);
    printf(" Sound:  %d/255\n",slSnd);
    printf(" MCU FW: %d.%d\n",fwH,fwL);
    printf(" RAM:    %s/%s\n",szStr(used),szStr(total));
    int rp=total>0?(int)(used*100/total):0;
    printf("  "); bar(rp,16); printf("\n");
    printf(" SysVer: %.24s\n",sysVer);
    u64 up=osGetTime()/1000;
    printf(" Uptime: %luh%lum\n",(unsigned long)(up/3600),(unsigned long)((up%3600)/60));
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  2. NETWORK INFO
// ═══════════════════════════════════════════════
void showNetworkInfo(){
    cur();
    printf("\x1b[36m== Network Info ==\x1b[0m\n\n");
    u32 wifi=0; ACU_GetWifiStatus(&wifi);
    printf(" Status:  %s\n",wifi?"\x1b[32mConnected\x1b[0m":"Disconnected");
    printf(" IP:      %s\n",ip4str());
    char ssid[33]={0}; ACU_GetSSID(ssid);
    printf(" SSID:    %.22s\n",ssid[0]?ssid:"(none)");
    acSecurityMode secMode=(acSecurityMode)0;
    ACU_GetSecurityMode(&secMode);
    const char* secN[]={"Open","WEP40","WEP104","WEP128","WPA-TKIP","WPA-AES","WPA2-TKIP","WPA2-AES"};
    printf(" Security:%s\n",(int)secMode<8?secN[(int)secMode]:"Unknown");
    bool proxyEn=false; ACU_GetProxyEnable(&proxyEn);
    printf(" Proxy:   %s\n",proxyEn?"Enabled":"Disabled");
    if(proxyEn){u32 port=0;ACU_GetProxyPort(&port);printf(" PxyPort: %lu\n",(unsigned long)port);}
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  3. STORAGE INFO
// ═══════════════════════════════════════════════
void showStorageInfo(){
    cur();
    printf("\x1b[33m== Storage Info ==\x1b[0m\n\n");
    FS_ArchiveResource sdR={0};
    FSUSER_GetArchiveResource(&sdR,SYSTEM_MEDIATYPE_SD);
    u64 sdTot=(u64)sdR.totalClusters*sdR.clusterSize;
    u64 sdFree=(u64)sdR.freeClusters*sdR.clusterSize;
    int sdPct=sdTot>0?(int)(100-(sdFree*100/sdTot)):0;
    printf(" \x1b[33mSD Card:\x1b[0m\n");
    printf("  Total: %s\n",szStr(sdTot));
    printf("  Free:  %s\n",szStr(sdFree));
    printf("  Used:  %s\n",szStr(sdTot-sdFree));
    printf("  "); bar(sdPct,16); printf("\n\n");
    u32 used=(u32)osGetMemRegionUsed(MEMREGION_APPLICATION);
    u32 tot=(u32)osGetMemRegionSize(MEMREGION_APPLICATION);
    int rp=tot>0?(int)(used*100/tot):0;
    printf(" \x1b[33mApp RAM:\x1b[0m\n");
    printf("  Total: %s\n",szStr(tot));
    printf("  Used:  %s\n",szStr(used));
    printf("  Free:  %s\n",szStr(tot-used));
    printf("  "); bar(rp,16); printf("\n");
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  4. HARDWARE TEST
// ═══════════════════════════════════════════════
void showHWTest(){
    cur();
    printf("\x1b[33m== Hardware Test ==\x1b[0m\n\n");
    touchPosition tp; hidTouchRead(&tp);
    printf(" Touch:  X=%-4d Y=%-4d     \n",tp.px,tp.py);
    // touch visual
    printf(" [");
    int tx=tp.px*18/320;
    for(int i=0;i<18;i++) printf(i==tx?"T":"-");
    printf("]\n");
    circlePosition cp; hidCircleRead(&cp);
    printf(" Circle: X=%-5d Y=%-5d   \n",cp.dx,cp.dy);
    angularRate gy; hidGyroRead(&gy);
    printf(" Gyro:   X=%-6d Y=%-6d \n",gy.x,gy.y);
    printf("         Z=%-6d          \n",gy.z);
    accelVector ac; hidAccelRead(&ac);
    printf(" Accel:  X=%-6d Y=%-6d \n",ac.x,ac.y);
    printf("         Z=%-6d          \n",ac.z);
    u8 sl=0,so=0;
    MCUHWC_Get3dSliderLevel(&sl); MCUHWC_GetSoundSliderLevel(&so);
    printf(" 3DSld:  %-3d  Sound:%-3d  \n",sl,so);
    printf("\n\x1b[21;1H\x1b[90mLive  B=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  5. CLOCK
// ═══════════════════════════════════════════════
void showClock(){
    cur();
    printf("\x1b[35m== Clock & Date ==\x1b[0m\n\n");
    u64 ms=osGetTime(); time_t s=(time_t)(ms/1000);
    struct tm* t=gmtime(&s);
    const char* days[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    const char* months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    printf("  \x1b[32m%02d:%02d:%02d\x1b[0m UTC\n\n",t->tm_hour,t->tm_min,t->tm_sec);
    printf("  %s %d %s %04d\n\n",days[t->tm_wday],t->tm_mday,months[t->tm_mon],t->tm_year+1900);
    printf("  Week:%d  Day:%d/365\n",(t->tm_yday+6)/7,t->tm_yday+1);
    unsigned long upSec=(unsigned long)(ms/1000);
    printf("\n  Uptime:%02luh%02lum%02lus\n",upSec/3600,(upSec%3600)/60,upSec%60);
    // simple clock face with hours
    printf("\n  ");
    for(int h=0;h<24;h++){
        if(h==t->tm_hour) printf("\x1b[32m*\x1b[0m");
        else printf(h%6==0?"|":".");
    }
    printf("\n  0  6  12  18  23\n");
    printf("\n\x1b[21;1H\x1b[90mLive  B=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  6. BUTTON TEST
// ═══════════════════════════════════════════════
void showBtnTest(u32 held){
    cur();
    printf("\x1b[36m== Button Test ==\x1b[0m\n\n");
    struct{u32 k;const char* n;}b[]={
        {KEY_A,"A    "},{KEY_B,"B    "},{KEY_X,"X    "},{KEY_Y,"Y    "},
        {KEY_L,"L    "},{KEY_R,"R    "},{KEY_ZL,"ZL   "},{KEY_ZR,"ZR   "},
        {KEY_DUP,"DUp  "},{KEY_DDOWN,"DDwn "},{KEY_DLEFT,"DLft "},{KEY_DRIGHT,"DRgt "},
        {KEY_START,"Start"},{KEY_SELECT,"Sel  "},
        {KEY_CPAD_UP,"CPUp "},{KEY_CPAD_DOWN,"CPDwn"},
        {KEY_CSTICK_UP,"CSUp "},{KEY_CSTICK_DOWN,"CSDwn"},
    };
    int n=sizeof(b)/sizeof(b[0]);
    for(int i=0;i<n;i++){
        bool h=(held&b[i].k)!=0;
        printf(" %s%s\x1b[0m",h?"\x1b[32m":"\x1b[31m",b[i].n);
        if(i%3==2) printf("\n");
    }
    printf("\n\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  7. POWER MENU
// ═══════════════════════════════════════════════
void showPowerMenu(int s){
    cur();
    printf("\x1b[31m== Power Options ==\x1b[0m\n\n");
    const char* o[]={"Return to Loader","Reboot System","Return to Home"};
    for(int i=0;i<3;i++) printf(" %s %s\n",s==i?"\x1b[32m>\x1b[0m":" ",o[i]);
    printf("\n\x1b[21;1H\x1b[90mA=Confirm  B=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  8. LED CONTROL
// ═══════════════════════════════════════════════
const char* ledOpts[LED_N]={"Wifi LED: ON","Wifi LED: OFF","Power: Normal","Power: Sleep","Power: OFF"};
void showLED(int s){
    cur();
    printf("\x1b[36m== LED Control ==\x1b[0m\n\n");
    for(int i=0;i<LED_N;i++) printf(" %s %s\n",s==i?"\x1b[36m>\x1b[0m":" ",ledOpts[i]);
    printf("\n\x1b[21;1H\x1b[90mA=Activate  B=Back\x1b[0m");
}
void doLED(int s){
    switch(s){
        case 0:MCUHWC_SetWifiLedState(true);           break;
        case 1:MCUHWC_SetWifiLedState(false);          break;
        case 2:MCUHWC_SetPowerLedState(LED_NORMAL);    break;
        case 3:MCUHWC_SetPowerLedState(LED_SLEEP_MODE);break;
        case 4:MCUHWC_SetPowerLedState(LED_OFF);       break;
    }
}

// ═══════════════════════════════════════════════
//  9. MINI-GAME: Number Guess
// ═══════════════════════════════════════════════
void showGuess(int g,int a,bool won,int tgt){
    cur();
    printf("\x1b[35m== Fox Guess (1-100) ==\x1b[0m\n\n");
    if(won){
        printf(" \x1b[32mFound %d in %d tries!\x1b[0m\n\n",tgt,a);
        printf(" Press A to play again.\n");
    } else {
        printf(" Guess:    \x1b[33m%d\x1b[0m      \n",g);
        printf(" Attempts: %d      \n",a);
        // progress bar towards answer
        printf(" [");
        for(int i=1;i<=10;i++){
            int zone=(tgt-1)/10;
            int gzone=(g-1)/10;
            printf(i-1==zone?"\x1b[31mT\x1b[0m":(i-1==gzone?"\x1b[33mG\x1b[0m":"-"));
        }
        printf("]\n");
        if(a>0){ if(g<tgt) printf(" \x1b[32mGo HIGHER!\x1b[0m   \n");
                 else      printf(" \x1b[31mGo LOWER! \x1b[0m   \n"); }
        else printf("              \n");
        printf("\n DPad Up/Down + A\n");
    }
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  10. SNAKE GAME
// ═══════════════════════════════════════════════
#define SN_W 38
#define SN_H 17
#define SN_MAX 200
struct Snake{int x[SN_MAX],y[SN_MAX],len,dir,fx,fy,score,tick,tickRate;bool dead;};
void snakeInit(Snake& sn){
    memset(&sn,0,sizeof(sn));
    sn.len=3;sn.dir=0;sn.tick=0;sn.tickRate=8;
    for(int i=0;i<sn.len;i++){sn.x[i]=10-i;sn.y[i]=8;}
    sn.fx=20;sn.fy=8;
}
void snakeDraw(Snake& sn){
    cur();
    printf("\x1b[35m== Snake  Score:%-4d ==\x1b[0m\n",sn.score);
    static char field[SN_H][SN_W+1];
    for(int r=0;r<SN_H;r++){
        for(int c=0;c<SN_W;c++) field[r][c]=(r==0||r==SN_H-1||c==0||c==SN_W-1)?'#':' ';
        field[r][SN_W]=0;
    }
    if(sn.fx>=1&&sn.fx<SN_W-1&&sn.fy>=1&&sn.fy<SN_H-1) field[sn.fy][sn.fx]='*';
    for(int i=0;i<sn.len;i++)
        if(sn.x[i]>=0&&sn.x[i]<SN_W&&sn.y[i]>=0&&sn.y[i]<SN_H)
            field[sn.y[i]][sn.x[i]]=(i==0?'O':'o');
    for(int r=0;r<SN_H;r++) printf("%s\n",field[r]);
    if(sn.dead) printf("\x1b[31mGAME OVER! A=Restart\x1b[0m ");
    else        printf(" DPad=Move  Len:%-4d ",sn.len);
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}
void snakeUpdate(Snake& sn,u32 kDown){
    if(sn.dead){if(kDown&KEY_A)snakeInit(sn);return;}
    if(kDown&KEY_DRIGHT&&sn.dir!=1)sn.dir=0;
    if(kDown&KEY_DLEFT &&sn.dir!=0)sn.dir=1;
    if(kDown&KEY_DUP   &&sn.dir!=3)sn.dir=2;
    if(kDown&KEY_DDOWN &&sn.dir!=2)sn.dir=3;
    if(++sn.tick<sn.tickRate)return;
    sn.tick=0;
    for(int i=sn.len-1;i>0;i--){sn.x[i]=sn.x[i-1];sn.y[i]=sn.y[i-1];}
    if(sn.dir==0)sn.x[0]++;if(sn.dir==1)sn.x[0]--;
    if(sn.dir==2)sn.y[0]--;if(sn.dir==3)sn.y[0]++;
    if(sn.x[0]<=0||sn.x[0]>=SN_W-1||sn.y[0]<=0||sn.y[0]>=SN_H-1){sn.dead=true;return;}
    for(int i=1;i<sn.len;i++) if(sn.x[0]==sn.x[i]&&sn.y[0]==sn.y[i]){sn.dead=true;return;}
    if(sn.x[0]==sn.fx&&sn.y[0]==sn.fy){
        sn.score+=10;if(sn.len<SN_MAX)sn.len++;if(sn.tickRate>2)sn.tickRate--;
        sn.fx=1+rand()%(SN_W-2);sn.fy=1+rand()%(SN_H-2);
    }
}

// ═══════════════════════════════════════════════
//  11. CALCULATOR
// ═══════════════════════════════════════════════
struct Calc{double val,prev;char op,disp[32],msg[32];bool newInput;};
void calcInit(Calc& c){c.val=0;c.prev=0;c.op=0;snprintf(c.disp,32,"0");c.newInput=true;c.msg[0]=0;}
void showCalc(Calc& c,int s){
    cur();
    printf("\x1b[33m== Calculator ==\x1b[0m\n\n");
    printf(" \x1b[32m%-20s\x1b[0m",c.disp);
    if(c.op) printf("  op:\x1b[33m%c\x1b[0m",c.op);
    printf("\n\n");
    if(c.msg[0]) printf(" \x1b[33m%s\x1b[0m\n\n",c.msg); else printf("\n\n");
    const char* btns[]={"7","8","9","/","4","5","6","*","1","2","3","-","0",".","=","+"};
    for(int i=0;i<16;i++){
        bool sel=(i==s);
        printf("%s[%s]%s",sel?"\x1b[32m":"\x1b[0m ",btns[i],sel?"\x1b[0m":"");
        if(i%4==3) printf("\n");
    }
    printf("\n A=Press  DPad=Move  X=Clear\n");
    printf("\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}
void calcPress(Calc& c,int s){
    const char* btns[]={"7","8","9","/","4","5","6","*","1","2","3","-","0",".","=","+"};
    const char* b=btns[s]; c.msg[0]=0;
    if(b[0]>='0'&&b[0]<='9'){
        if(c.newInput){snprintf(c.disp,32,"%c",b[0]);c.newInput=false;}
        else if(strlen(c.disp)<14){char tmp[32];snprintf(tmp,32,"%s%c",c.disp,b[0]);strcpy(c.disp,tmp);}
        c.val=atof(c.disp);
    } else if(b[0]=='.'){
        if(!strchr(c.disp,'.')){strncat(c.disp,".",31);c.val=atof(c.disp);}
    } else if(b[0]=='='){
        double res=c.prev;
        if(c.op=='+')res+=c.val;
        else if(c.op=='-')res-=c.val;
        else if(c.op=='*')res*=c.val;
        else if(c.op=='/'){if(c.val!=0)res/=c.val;else{snprintf(c.msg,32,"Div/0!");return;}}
        else res=c.val;
        snprintf(c.disp,32,"%.8g",res);
        c.val=res;c.prev=0;c.op=0;c.newInput=true;
    } else {
        c.prev=c.val;c.op=b[0];c.newInput=true;
        snprintf(c.msg,32,"%.8g %c ?",c.val,b[0]);
    }
}

// ═══════════════════════════════════════════════
//  12. REACTION TIME GAME
// ═══════════════════════════════════════════════
enum ReactionState{RX_WAIT,RX_READY,RX_RESULT,RX_TOOSOON};
struct Reaction{
    ReactionState rs;
    u64 startMs,reactionMs;
    int bestMs,count;
    u64 waitUntil;
};
void reactionInit(Reaction& rx){
    rx.rs=RX_WAIT;rx.reactionMs=0;rx.bestMs=99999;rx.count=0;
    rx.waitUntil=osGetTime()+(u64)(2000+rand()%3000);
}
void showReaction(Reaction& rx,u32 kDown){
    cur();
    printf("\x1b[35m== Reaction Test ==\x1b[0m\n\n");
    u64 now=osGetTime();

    if(rx.rs==RX_WAIT){
        if(now>=rx.waitUntil) rx.rs=RX_READY;
        printf(" \x1b[90mWaiting...          \x1b[0m\n\n");
        printf(" Get ready!\n\n");
        printf(" Press A when screen\n goes GREEN!\n");
        if(kDown&KEY_A){rx.rs=RX_TOOSOON;}
    } else if(rx.rs==RX_READY){
        printf(" \x1b[32m████ PRESS A! ████  \x1b[0m\n\n");
        printf(" \x1b[32mNOW! NOW! NOW!\x1b[0m\n");
        if(!rx.startMs) rx.startMs=now;
        if(kDown&KEY_A){
            rx.reactionMs=now-rx.startMs;
            if((int)rx.reactionMs<rx.bestMs) rx.bestMs=(int)rx.reactionMs;
            rx.count++;
            rx.rs=RX_RESULT;
        }
    } else if(rx.rs==RX_RESULT){
        printf(" Time: \x1b[32m%llums\x1b[0m            \n\n",(unsigned long long)rx.reactionMs);
        printf(" Best: \x1b[33m%dms\x1b[0m              \n",rx.bestMs);
        printf(" Tries:%d\n\n",rx.count);
        if(rx.reactionMs<200)      printf(" \x1b[32mWOW! Incredible!\x1b[0m\n");
        else if(rx.reactionMs<300) printf(" \x1b[32mGreat!\x1b[0m\n");
        else if(rx.reactionMs<500) printf(" \x1b[33mGood!\x1b[0m\n");
        else                       printf(" \x1b[31mSlow...\x1b[0m\n");
        printf("\n Press A to try again.\n");
        if(kDown&KEY_A){
            rx.rs=RX_WAIT;rx.startMs=0;
            rx.waitUntil=now+(u64)(2000+rand()%3000);
        }
    } else if(rx.rs==RX_TOOSOON){
        printf(" \x1b[31mTOO SOON!\x1b[0m\n\n");
        printf(" Wait for GREEN!\n\n");
        printf(" Press A to retry.\n");
        if(kDown&KEY_A){
            rx.rs=RX_WAIT;rx.startMs=0;
            rx.waitUntil=now+(u64)(2000+rand()%3000);
        }
    }
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  13. COLOR TEST
// ═══════════════════════════════════════════════
void showColorTest(int frame){
    cur();
    printf("\x1b[35m== Color & ANSI Test ==\x1b[0m\n\n");
    // 8 basic colors
    printf(" ");
    for(int i=30;i<=37;i++) printf("\x1b[%dm█\x1b[0m",i);
    printf("  Normal\n ");
    for(int i=30;i<=37;i++) printf("\x1b[1;%dm█\x1b[0m",i);
    printf("  Bold\n\n");
    // animated bar
    int pos=(frame/2)%36;
    printf(" [");
    for(int i=0;i<36;i++){
        int c=30+(i+frame/4)%8;
        printf(i==pos?"\x1b[%dm*\x1b[0m":"\x1b[%dm-\x1b[0m",c,c);
    }
    printf("]\n\n");
    // rainbow text
    const char* txt="DarkFox-3DS v4.0";
    printf(" ");
    for(int i=0;txt[i];i++) printf("\x1b[%dm%c\x1b[0m",30+(i+frame/8)%8,txt[i]);
    printf("\n\n");
    // gradient bar
    printf(" ");
    for(int i=0;i<20;i++){
        printf(i<7?"\x1b[31m":i<14?"\x1b[33m":"\x1b[32m");
        printf("█\x1b[0m");
    }
    printf("  Gradient\n");
    printf("\n\x1b[21;1H\x1b[90mLive  B=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  14. FILE BROWSER
// ═══════════════════════════════════════════════
struct FBState{
    char path[256],ent[FB_MAX][FB_NL],msg[64];
    bool isDir[FB_MAX],confirmDel;
    int cnt,sel,top,msgT;
};
void fbLoad(FBState& f){
    f.cnt=f.sel=f.top=0;f.confirmDel=false;f.msg[0]=0;f.msgT=0;
    DIR* d=opendir(f.path);if(!d)return;
    if(strcmp(f.path,"sdmc:/")!=0){strncpy(f.ent[f.cnt],"..",FB_NL-1);f.isDir[f.cnt++]=true;}
    struct dirent* e;
    while((e=readdir(d))&&f.cnt<FB_MAX){
        if(e->d_name[0]=='.') continue;
        strncpy(f.ent[f.cnt],e->d_name,FB_NL-1);f.ent[f.cnt][FB_NL-1]=0;
        char fp[320];snprintf(fp,320,"%s%s",f.path,e->d_name);
        struct stat st;f.isDir[f.cnt]=(stat(fp,&st)==0&&S_ISDIR(st.st_mode));
        f.cnt++;
    }
    closedir(d);
}
void fbNav(FBState& f){
    if(!f.cnt)return;
    if(strcmp(f.ent[f.sel],"..")==0){
        int l=(int)strlen(f.path);
        if(l>7){char tmp[256];strncpy(tmp,f.path,255);tmp[l-1]=0;
            char* s=strrchr(tmp,'/');if(s){*(s+1)=0;strncpy(f.path,tmp,255);}}
    } else if(f.isDir[f.sel]){
        if(strlen(f.path)+strlen(f.ent[f.sel])+2<256){strcat(f.path,f.ent[f.sel]);strcat(f.path,"/");}
    }
    fbLoad(f);
}
void fbDel(FBState& f){
    if(!f.cnt||strcmp(f.ent[f.sel],"..")==0)return;
    char fp[320];snprintf(fp,320,"%s%s",f.path,f.ent[f.sel]);
    int r=f.isDir[f.sel]?rmdir(fp):remove(fp);
    snprintf(f.msg,64,r==0?"Deleted!":"Failed!");f.msgT=90;fbLoad(f);
}
void showFB(FBState& f){
    cur();
    char sp[36];strncpy(sp,f.path,35);sp[35]=0;
    printf("\x1b[33m%-36s\x1b[0m\n",sp);
    printf("------------------------------------\n");
    if(!f.cnt){printf(" (empty)\n");for(int i=1;i<FB_VIS;i++)printf("\n");}
    else{
        int end=f.top+FB_VIS;if(end>f.cnt)end=f.cnt;
        for(int i=f.top;i<end;i++){
            bool s=(i==f.sel);
            char nm[26];strncpy(nm,f.ent[i],25);nm[25]=0;
            printf("%s %s %-25s\x1b[0m\n",s?"\x1b[32m":"\x1b[0m",f.isDir[i]?"[D]":"[F]",nm);
        }
        for(int i=end-f.top;i<FB_VIS;i++)printf("\n");
    }
    printf("-- %d/%d ",f.sel+1,f.cnt);
    if(f.msgT>0){printf("%-14s\n",f.msg);f.msgT--;}else printf("              \n");
    printf("\x1b[21;1H");
    if(f.confirmDel) printf("\x1b[31mDelete? A=Yes B=No\x1b[0m      ");
    else             printf("\x1b[90mA=Open  X=Del  B=Back\x1b[0m  ");
}
void fbInput(FBState& f,u32 kDown){
    if(f.confirmDel){if(kDown&KEY_A)fbDel(f);if(kDown&(KEY_A|KEY_B))f.confirmDel=false;return;}
    if(kDown&KEY_DUP&&f.sel>0){f.sel--;if(f.sel<f.top)f.top=f.sel;}
    if(kDown&KEY_DDOWN&&f.sel<f.cnt-1){f.sel++;if(f.sel>=f.top+FB_VIS)f.top=f.sel-FB_VIS+1;}
    if(kDown&KEY_A)fbNav(f);
    if(kDown&KEY_X&&f.cnt&&strcmp(f.ent[f.sel],"..")!=0)f.confirmDel=true;
}

// ═══════════════════════════════════════════════
//  15. ABOUT
// ═══════════════════════════════════════════════
void showAbout(){
    cur();
    printf("\x1b[35m== DarkFox-3DS v4.0 ==\x1b[0m\n\n");
    printf(" Developer: DarkFox Team\n\n");
    printf("  1  System Info\n");
    printf("  2  File Browser\n");
    printf("  3  Power Options\n");
    printf("  4  LED Control\n");
    printf("  5  Number Guess\n");
    printf("  6  Network Info\n");
    printf("  7  Hardware Test\n");
    printf("  8  Clock & Date\n");
    printf("  9  Storage Info\n");
    printf("  10 Button Tester\n");
    printf("  11 Snake Game\n");
    printf("  12 Calculator\n");
    printf("  13 Reaction Test\n");
    printf("  14 Color Test\n");
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ═══════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════
int main(int argc,char* argv[]){
    gfxInitDefault();
    PrintConsole top,bot;
    consoleInit(GFX_TOP,&top);
    consoleInit(GFX_BOTTOM,&bot);
    mcuHwcInit();ptmuInit();acInit();hidInit();

    FBState fb;memset(&fb,0,sizeof(fb));
    strncpy(fb.path,"sdmc:/",255);fbLoad(fb);
    Snake sn;snakeInit(sn);
    Calc calc;calcInit(calc);
    Reaction rx;memset(&rx,0,sizeof(rx));reactionInit(rx);

    int mainSel=0,subSel=0,calcSel=0,colorFrame=0;
    MenuState state=MAIN_MENU,prev=(MenuState)-1;
    int guess=50,target=1,attempts=0;bool won=false;
    srand((unsigned int)osGetTime());target=rand()%100+1;

    const char* mainOpts[]={
        " 1.System Info  "," 2.File Browser"," 3.Power       ",
        " 4.LED Control  "," 5.Mini-Game   "," 6.Network     ",
        " 7.HW Test      "," 8.Clock       "," 9.Storage     ",
        "10.Btn Test     ","11.Snake       ","12.Calculator  ",
        "13.Reaction     ","14.Color Test  ","15.About       "
    };
    const int MN=15;

    while(aptMainLoop()){
        hidScanInput();
        u32 kDown=hidKeysDown(),kHeld=hidKeysHeld();
        if((kDown&KEY_START)&&state==MAIN_MENU)break;

        // Update globals for bottom screen
        g_state=state;
        g_snakeScore=sn.score;
        g_guessAttempts=attempts;

        bool changed=(state!=prev);
        if(changed){
            consoleSelect(&top);cls();
            consoleSelect(&bot);cls();
            prev=state;
            if(state==FILE_BROWSER) fbLoad(fb);
            if(state==SNAKE_GAME)   snakeInit(sn);
            if(state==CALC)         calcInit(calc);
            if(state==REACTION_GAME){memset(&rx,0,sizeof(rx));reactionInit(rx);}
        }

        // ── Bottom Screen ──
        consoleSelect(&bot);
        drawBottom();

        // ── Top Screen ──
        consoleSelect(&top);
        colorFrame++;

        if(state==MAIN_MENU){
            cur();
            printf("\x1b[31mDarkFox-3DS v4.0\x1b[0m\n\n");
            int half=(MN+1)/2;
            for(int i=0;i<half;i++){
                int j=i+half;
                printf("%s%-17s\x1b[0m",i==mainSel?"\x1b[32m":" ",mainOpts[i]);
                if(j<MN) printf(" %s%-17s\x1b[0m",j==mainSel?"\x1b[32m":" ",mainOpts[j]);
                printf("\n");
            }
            printf("\n\x1b[90mDPad=Nav  A=Select  START=Quit\x1b[0m\n");
            if(kDown&KEY_DUP)    mainSel=(mainSel-1+MN)%MN;
            if(kDown&KEY_DDOWN)  mainSel=(mainSel+1)%MN;
            if(kDown&KEY_DLEFT)  mainSel=(mainSel-half+MN)%MN;
            if(kDown&KEY_DRIGHT) mainSel=(mainSel+half)%MN;
            if(kDown&KEY_A){
                subSel=0;
                switch(mainSel){
                    case 0:state=SYSTEM_INFO;   break; case 1:state=FILE_BROWSER; break;
                    case 2:state=POWER_MENU;    break; case 3:state=LED_FUN;      break;
                    case 4:state=MINI_GAME;     break; case 5:state=NETWORK_INFO; break;
                    case 6:state=HW_TEST;       break; case 7:state=CLOCK_VIEW;   break;
                    case 8:state=STORAGE_INFO;  break; case 9:state=BTN_TEST;     break;
                    case 10:state=SNAKE_GAME;   break; case 11:state=CALC;        break;
                    case 12:state=REACTION_GAME;break; case 13:state=COLOR_TEST;  break;
                    case 14:state=ABOUT;        break;
                }
            }
        } else {
            bool back=false;
            switch(state){
                case SYSTEM_INFO:   showSystemInfo();        if(kDown&KEY_B)back=true; break;
                case NETWORK_INFO:  showNetworkInfo();       if(kDown&KEY_B)back=true; break;
                case STORAGE_INFO:  showStorageInfo();       if(kDown&KEY_B)back=true; break;
                case HW_TEST:       showHWTest();            if(kDown&KEY_B)back=true; break;
                case CLOCK_VIEW:    showClock();             if(kDown&KEY_B)back=true; break;
                case BTN_TEST:      showBtnTest(kHeld);      if(kDown&KEY_B)back=true; break;
                case ABOUT:         showAbout();             if(kDown&KEY_B)back=true; break;
                case COLOR_TEST:    showColorTest(colorFrame);if(kDown&KEY_B)back=true;break;
                case POWER_MENU:
                    showPowerMenu(subSel);
                    if(kDown&KEY_DUP)   subSel=(subSel-1+3)%3;
                    if(kDown&KEY_DDOWN) subSel=(subSel+1)%3;
                    if(kDown&KEY_A){if(subSel==0||subSel==2)goto done;if(subSel==1)NS_RebootSystem();}
                    if(kDown&KEY_B)back=true; break;
                case LED_FUN:
                    showLED(subSel);
                    if(kDown&KEY_DUP)   subSel=(subSel-1+LED_N)%LED_N;
                    if(kDown&KEY_DDOWN) subSel=(subSel+1)%LED_N;
                    if(kDown&KEY_A) doLED(subSel);
                    if(kDown&KEY_B)back=true; break;
                case MINI_GAME:
                    showGuess(guess,attempts,won,target);
                    if(!won){
                        if(kDown&KEY_DUP)  {guess++;if(guess>100)guess=100;}
                        if(kDown&KEY_DDOWN){guess--;if(guess<1)  guess=1;  }
                        if(kDown&KEY_A){attempts++;if(guess==target)won=true;}
                    } else if(kDown&KEY_A){won=false;attempts=0;guess=50;target=rand()%100+1;}
                    if(kDown&KEY_B)back=true; break;
                case SNAKE_GAME:
                    snakeUpdate(sn,kDown);snakeDraw(sn);
                    if(kDown&KEY_B)back=true; break;
                case CALC:
                    showCalc(calc,calcSel);
                    if(kDown&KEY_DUP)    calcSel=(calcSel-4+16)%16;
                    if(kDown&KEY_DDOWN)  calcSel=(calcSel+4)%16;
                    if(kDown&KEY_DLEFT)  calcSel=(calcSel-1+16)%16;
                    if(kDown&KEY_DRIGHT) calcSel=(calcSel+1)%16;
                    if(kDown&KEY_A) calcPress(calc,calcSel);
                    if(kDown&KEY_X) calcInit(calc);
                    if(kDown&KEY_B)back=true; break;
                case REACTION_GAME:
                    showReaction(rx,kDown);
                    if(kDown&KEY_B)back=true; break;
                case FILE_BROWSER:
                    fbInput(fb,kDown);showFB(fb);
                    if((kDown&KEY_B)&&!fb.confirmDel)back=true; break;
                default:back=true;break;
            }
            if(back)state=MAIN_MENU;
        }
        gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
    }
done:
    acExit();ptmuExit();mcuHwcExit();gfxExit();
    return 0;
}
