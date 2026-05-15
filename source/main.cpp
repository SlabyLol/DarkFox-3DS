// DarkFox-3DS v3.0 - libctru 2.3 verified
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

enum MenuState {
    MAIN_MENU=0, SYSTEM_INFO, FILE_BROWSER, POWER_MENU,
    LED_FUN, MINI_GAME, NETWORK_INFO, HW_TEST,
    CLOCK_VIEW, STORAGE_INFO, BTN_TEST, SNAKE_GAME,
    CALC, ABOUT
};

#define FB_MAX 128
#define FB_NL   48
#define FB_VIS  15
#define LED_N    5

// ── Utilities ──────────────────────────────────────────────
void cur() { printf("\x1b[H"); }
void cls() { consoleClear(); }

const char* ip4str() {
    static char b[16];
    u32 ip = gethostid();
    snprintf(b,16,"%d.%d.%d.%d",
        (int)(ip&0xFF),(int)((ip>>8)&0xFF),
        (int)((ip>>16)&0xFF),(int)((ip>>24)&0xFF));
    return b;
}

const char* szStr(u64 b) {
    static char buf[24];
    if     (b>=1024ULL*1024*1024) snprintf(buf,24,"%.2fGB",(double)b/(1024.0*1024*1024));
    else if(b>=1024ULL*1024)      snprintf(buf,24,"%.2fMB",(double)b/(1024.0*1024));
    else if(b>=1024ULL)           snprintf(buf,24,"%.2fKB",(double)b/1024.0);
    else                          snprintf(buf,24,"%dB",(int)b);
    return buf;
}

void bar(int pct, int w) {
    printf("[");
    for(int i=0;i<w;i++) printf(i<pct*w/100?"#":"-");
    printf("] %d%%", pct);
}

// ── Bottom status bar ──────────────────────────────────────
void drawStatus(const char* sub) {
    printf("\x1b[H");
    printf("\x1b[31mDarkFox-3DS v3.0\x1b[0m\n");
    printf("====================\n");
    u8 bat=0, chg=0;
    MCUHWC_GetBatteryLevel(&bat);
    PTMU_GetBatteryChargeState(&chg);
    if(bat>50)      printf("\x1b[32m");
    else if(bat>20) printf("\x1b[33m");
    else            printf("\x1b[31m");
    printf("Bat:%3d%%\x1b[0m %s\n", bat, chg?"\x1b[33m[CHG]\x1b[0m":"     ");
    u64 ms=osGetTime(); time_t s=(time_t)(ms/1000);
    struct tm* t=gmtime(&s);
    printf("UTC: %02d:%02d:%02d\n",t->tm_hour,t->tm_min,t->tm_sec);
    u32 wifi=0; ACU_GetWifiStatus(&wifi);
    printf("Net: %s\n", wifi?"\x1b[32mOnline \x1b[0m":"Offline");
    printf("IP:  %s\n", ip4str());
    if(sub && sub[0]) { printf("--------------------\n"); printf("%s\n",sub); }
}

// ── 1. System Info ─────────────────────────────────────────
void showSystemInfo() {
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

    printf(" Model:  %-16s\n", model<6?mdl[model]:"Unknown");
    printf(" Bat:    %d%% %s\n", bat, chg?"[CHG]":"");
    printf(" 3DSld:  %d/255\n", sl3d);
    printf(" Sound:  %d/255\n", slSnd);
    printf(" MCU FW: %d.%d\n", fwH, fwL);
    printf(" RAM:    %s/%s\n", szStr(used),szStr(total));
    int rp=total>0?(int)(used*100/total):0;
    printf("  "); bar(rp,16); printf("\n");
    printf(" SysVer: %.24s\n", sysVer);
    u64 up=osGetTime()/1000;
    printf(" Uptime: %luh %lum\n",(unsigned long)(up/3600),(unsigned long)((up%3600)/60));
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ── 2. Network Info ────────────────────────────────────────
void showNetworkInfo() {
    cur();
    printf("\x1b[36m== Network Info ==\x1b[0m\n\n");
    u32 wifi=0; ACU_GetWifiStatus(&wifi);
    printf(" Status:  %s\n", wifi?"\x1b[32mConnected\x1b[0m":"Disconnected");
    printf(" IP:      %s\n", ip4str());
    char ssid[33]={0}; ACU_GetSSID(ssid);
    printf(" SSID:    %.22s\n", ssid[0]?ssid:"(none)");
    // FIX: use acSecurityMode type (not u32)
    acSecurityMode secMode=(acSecurityMode)0;
    ACU_GetSecurityMode(&secMode);
    const char* secNames[]={"Open","WEP40","WEP104","WEP128",
                             "WPA-TKIP","WPA-AES","WPA2-TKIP","WPA2-AES"};
    printf(" Security:%s\n", (int)secMode<8?secNames[(int)secMode]:"Unknown");
    bool proxyEn=false; ACU_GetProxyEnable(&proxyEn);
    printf(" Proxy:   %s\n", proxyEn?"Enabled":"Disabled");
    if(proxyEn){u32 port=0;ACU_GetProxyPort(&port);printf(" PxyPort: %lu\n",(unsigned long)port);}
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ── 3. Storage Info ────────────────────────────────────────
void showStorageInfo() {
    cur();
    printf("\x1b[33m== Storage Info ==\x1b[0m\n\n");
    // FIX: SYSTEM_MEDIATYPE_NAND does not exist in this libctru version
    // Use SYSTEM_MEDIATYPE_SD only
    FS_ArchiveResource sdR={0};
    FSUSER_GetArchiveResource(&sdR, SYSTEM_MEDIATYPE_SD);
    u64 sdTot =(u64)sdR.totalClusters*sdR.clusterSize;
    u64 sdFree=(u64)sdR.freeClusters *sdR.clusterSize;
    int sdPct = sdTot>0?(int)(100-(sdFree*100/sdTot)):0;
    printf(" \x1b[33mSD Card:\x1b[0m\n");
    printf("  Total: %s\n", szStr(sdTot));
    printf("  Free:  %s\n", szStr(sdFree));
    printf("  Used:  %s\n", szStr(sdTot-sdFree));
    printf("  "); bar(sdPct,16); printf("\n\n");
    // RAM info as second "storage"
    u32 used=(u32)osGetMemRegionUsed(MEMREGION_APPLICATION);
    u32 tot =(u32)osGetMemRegionSize(MEMREGION_APPLICATION);
    int rp=tot>0?(int)(used*100/tot):0;
    printf(" \x1b[33mApp RAM:\x1b[0m\n");
    printf("  Total: %s\n", szStr(tot));
    printf("  Used:  %s\n", szStr(used));
    printf("  Free:  %s\n", szStr(tot-used));
    printf("  "); bar(rp,16); printf("\n");
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ── 4. Hardware Test ───────────────────────────────────────
void showHWTest() {
    cur();
    printf("\x1b[33m== Hardware Test ==\x1b[0m\n\n");
    touchPosition tp; hidTouchRead(&tp);
    printf(" Touch:  X=%-4d Y=%-4d     \n", tp.px, tp.py);
    circlePosition cp; hidCircleRead(&cp);
    printf(" Circle: X=%-5d Y=%-5d   \n", cp.dx, cp.dy);
    angularRate gy; hidGyroRead(&gy);
    printf(" Gyro:   X=%-6d Y=%-6d \n", gy.x, gy.y);
    printf("         Z=%-6d          \n", gy.z);
    accelVector ac; hidAccelRead(&ac);
    printf(" Accel:  X=%-6d Y=%-6d \n", ac.x, ac.y);
    printf("         Z=%-6d          \n", ac.z);
    u8 sl=0,so=0;
    MCUHWC_Get3dSliderLevel(&sl); MCUHWC_GetSoundSliderLevel(&so);
    printf(" 3DSld:  %-3d  Sound:%-3d  \n", sl, so);
    printf("\n\x1b[21;1H\x1b[90mLive  B=Back\x1b[0m");
}

// ── 5. Clock ───────────────────────────────────────────────
void showClock() {
    cur();
    printf("\x1b[35m== Clock & Date ==\x1b[0m\n\n");
    u64 ms=osGetTime(); time_t s=(time_t)(ms/1000);
    struct tm* t=gmtime(&s);
    const char* days[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    const char* months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    printf("  \x1b[32m%02d:%02d:%02d\x1b[0m UTC\n\n",t->tm_hour,t->tm_min,t->tm_sec);
    printf("  %s %d %s %04d\n\n",days[t->tm_wday],t->tm_mday,months[t->tm_mon],t->tm_year+1900);
    printf("  Week: %d  Day: %d/365\n", (t->tm_yday+6)/7, t->tm_yday+1);
    // FIX: cast u32 to unsigned long for %lu
    unsigned long upSec=(unsigned long)(ms/1000);
    printf("\n  Uptime: %02luh %02lum %02lus\n", upSec/3600,(upSec%3600)/60,upSec%60);
    printf("\n\x1b[21;1H\x1b[90mLive  B=Back\x1b[0m");
}

// ── 6. Button Test ─────────────────────────────────────────
void showBtnTest(u32 held) {
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

// ── 7. Power Menu ──────────────────────────────────────────
void showPowerMenu(int s) {
    cur();
    printf("\x1b[31m== Power Options ==\x1b[0m\n\n");
    const char* o[]={"Return to Loader","Reboot System","Return to Home"};
    for(int i=0;i<3;i++) printf(" %s %s\n",s==i?"\x1b[32m>\x1b[0m":" ",o[i]);
    printf("\n\x1b[21;1H\x1b[90mA=Confirm  B=Back\x1b[0m");
}

// ── 8. LED Control ─────────────────────────────────────────
const char* ledOpts[LED_N]={
    "Wifi LED: ON","Wifi LED: OFF",
    "Power: Normal","Power: Sleep","Power: OFF"
};
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

// ── 9. Mini-Game ───────────────────────────────────────────
void showGuess(int g,int a,bool won,int tgt){
    cur();
    printf("\x1b[35m== Fox Guess (1-100) ==\x1b[0m\n\n");
    if(won){
        printf(" \x1b[32mFound %d in %d tries!\x1b[0m\n\n",tgt,a);
        printf(" Press A to play again.\n");
    } else {
        printf(" Guess:    \x1b[33m%d\x1b[0m      \n",g);
        printf(" Attempts: %d      \n",a);
        if(a>0){ if(g<tgt) printf(" \x1b[32mGo HIGHER!\x1b[0m   \n");
                 else      printf(" \x1b[31mGo LOWER! \x1b[0m   \n"); }
        else     printf("              \n");
        printf("\n DPad Up/Down + A\n");
    }
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ── 10. Snake Game ─────────────────────────────────────────
#define SN_W 38
#define SN_H 17
#define SN_MAX 200
struct Snake {
    int x[SN_MAX],y[SN_MAX],len,dir,fx,fy,score,tick,tickRate;
    bool dead;
};
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
    else        printf(" DPad=Move            ");
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}
void snakeUpdate(Snake& sn,u32 kDown){
    if(sn.dead){if(kDown&KEY_A)snakeInit(sn);return;}
    if(kDown&KEY_DRIGHT&&sn.dir!=1)sn.dir=0;
    if(kDown&KEY_DLEFT &&sn.dir!=0)sn.dir=1;
    if(kDown&KEY_DUP   &&sn.dir!=3)sn.dir=2;
    if(kDown&KEY_DDOWN &&sn.dir!=2)sn.dir=3;
    if(++sn.tick<sn.tickRate) return;
    sn.tick=0;
    for(int i=sn.len-1;i>0;i--){sn.x[i]=sn.x[i-1];sn.y[i]=sn.y[i-1];}
    if(sn.dir==0)sn.x[0]++; if(sn.dir==1)sn.x[0]--;
    if(sn.dir==2)sn.y[0]--; if(sn.dir==3)sn.y[0]++;
    if(sn.x[0]<=0||sn.x[0]>=SN_W-1||sn.y[0]<=0||sn.y[0]>=SN_H-1){sn.dead=true;return;}
    for(int i=1;i<sn.len;i++) if(sn.x[0]==sn.x[i]&&sn.y[0]==sn.y[i]){sn.dead=true;return;}
    if(sn.x[0]==sn.fx&&sn.y[0]==sn.fy){
        sn.score+=10; if(sn.len<SN_MAX)sn.len++; if(sn.tickRate>2)sn.tickRate--;
        sn.fx=1+rand()%(SN_W-2); sn.fy=1+rand()%(SN_H-2);
    }
}

// ── 11. Calculator ─────────────────────────────────────────
struct Calc{double val,prev;char op,disp[32],msg[32];bool newInput;};
void calcInit(Calc& c){c.val=0;c.prev=0;c.op=0;snprintf(c.disp,32,"0");c.newInput=true;c.msg[0]=0;}
void showCalc(Calc& c,int s){
    cur();
    printf("\x1b[33m== Calculator ==\x1b[0m\n\n");
    printf(" \x1b[32m%-20s\x1b[0m\n\n",c.disp);
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
        if(c.op=='+')      res+=c.val;
        else if(c.op=='-') res-=c.val;
        else if(c.op=='*') res*=c.val;
        else if(c.op=='/'){if(c.val!=0)res/=c.val;else{snprintf(c.msg,32,"Div by zero!");return;}}
        else               res=c.val;
        snprintf(c.disp,32,"%.6g",res);
        c.val=res;c.prev=0;c.op=0;c.newInput=true;
    } else {
        c.prev=c.val;c.op=b[0];c.newInput=true;
        snprintf(c.msg,32,"Op: %c",b[0]);
    }
}

// ── 12. File Browser ───────────────────────────────────────
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

// ── About ──────────────────────────────────────────────────
void showAbout(){
    cur();
    printf("\x1b[35m== DarkFox-3DS v3.0 ==\x1b[0m\n\n");
    printf(" Developer: DarkFox Team\n\n");
    printf("  1  System Info + RAM bar\n");
    printf("  2  File Browser (open/del)\n");
    printf("  3  Power (reboot/exit)\n");
    printf("  4  LED Control\n");
    printf("  5  Number Guess + hints\n");
    printf("  6  Network + Security\n");
    printf("  7  Hardware Test (live)\n");
    printf("  8  Clock & Date (live)\n");
    printf("  9  Storage + RAM bars\n");
    printf("  10 Button Tester (live)\n");
    printf("  11 Snake Game\n");
    printf("  12 Calculator\n");
    printf("\n\x1b[21;1H\x1b[90mB=Back\x1b[0m");
}

// ── Main ───────────────────────────────────────────────────
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

    int mainSel=0,subSel=0,calcSel=0;
    MenuState state=MAIN_MENU,prev=(MenuState)-1;
    int guess=50,target=1,attempts=0;bool won=false;
    srand((unsigned int)osGetTime());target=rand()%100+1;

    const char* mainOpts[]={
        " 1.System Info  "," 2.File Browser"," 3.Power       ",
        " 4.LED Control  "," 5.Mini-Game   "," 6.Network     ",
        " 7.HW Test      "," 8.Clock       "," 9.Storage     ",
        "10.Btn Test     ","11.Snake       ","12.Calculator  ",
        "13.About        "
    };
    const int MN=13;

    while(aptMainLoop()){
        hidScanInput();
        u32 kDown=hidKeysDown(),kHeld=hidKeysHeld();
        if((kDown&KEY_START)&&state==MAIN_MENU)break;

        bool changed=(state!=prev);
        if(changed){
            consoleSelect(&top);cls();
            consoleSelect(&bot);cls();
            prev=state;
            if(state==FILE_BROWSER)fbLoad(fb);
            if(state==SNAKE_GAME) snakeInit(sn);
            if(state==CALC)       calcInit(calc);
        }

        // Bottom
        consoleSelect(&bot);
        char sub[48]={0};
        if(state==SNAKE_GAME) snprintf(sub,48,"Score: %d",sn.score);
        if(state==MINI_GAME)  snprintf(sub,48,"Guess:%d Att:%d",guess,attempts);
        drawStatus(sub);

        // Top
        consoleSelect(&top);

        if(state==MAIN_MENU){
            cur();
            printf("\x1b[31mDarkFox-3DS v3.0\x1b[0m\n\n");
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
                    case 0:state=SYSTEM_INFO; break; case 1:state=FILE_BROWSER;break;
                    case 2:state=POWER_MENU;  break; case 3:state=LED_FUN;     break;
                    case 4:state=MINI_GAME;   break; case 5:state=NETWORK_INFO;break;
                    case 6:state=HW_TEST;     break; case 7:state=CLOCK_VIEW;  break;
                    case 8:state=STORAGE_INFO;break; case 9:state=BTN_TEST;    break;
                    case 10:state=SNAKE_GAME; break; case 11:state=CALC;       break;
                    case 12:state=ABOUT;      break;
                }
            }
        } else {
            bool back=false;
            switch(state){
                case SYSTEM_INFO:  showSystemInfo();   if(kDown&KEY_B)back=true; break;
                case NETWORK_INFO: showNetworkInfo();  if(kDown&KEY_B)back=true; break;
                case STORAGE_INFO: showStorageInfo();  if(kDown&KEY_B)back=true; break;
                case HW_TEST:      showHWTest();       if(kDown&KEY_B)back=true; break;
                case CLOCK_VIEW:   showClock();        if(kDown&KEY_B)back=true; break;
                case BTN_TEST:     showBtnTest(kHeld); if(kDown&KEY_B)back=true; break;
                case ABOUT:        showAbout();        if(kDown&KEY_B)back=true; break;
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
                    snakeUpdate(sn,kDown); snakeDraw(sn);
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
                case FILE_BROWSER:
                    fbInput(fb,kDown); showFB(fb);
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
