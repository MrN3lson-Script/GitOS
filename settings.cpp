typedef unsigned char  u8;
typedef unsigned short u16;

typedef void(*puts_t)(int,int,const char*,u16);
typedef void(*cls_t)(void);
typedef void(*fill_t)(int,u16);
typedef int (*rl_t)(char*,int,int,u16);
typedef u8  (*kbd_t)(void);

#define CW 0x0F00
#define CY 0x0F00
#define CG 0x0F00
#define CC 0x0F00
#define CR 0x0F00
#define CX 0x0F00

static inline u8 _inb(u16 p){u8 v;__asm__("inb %1,%0":"=a"(v):"dN"(p));return v;}
static int _sl(const char*s){int n=0;while(s[n])n++;return n;}
static void _sc(char*d,const char*s){while((*d++=*s++));}
static void _si(char*b,int n){
    if(!n){b[0]='0';b[1]=0;return;}
    char t[8];int i=0;
    while(n>0){t[i++]='0'+n%10;n/=10;}
    int j=0;while(i>0)b[j++]=t[--i];b[j]=0;
}

extern "C"
void run_settings(char*osname,int*timeout,int*defmode,
    void(*save)(void),puts_t P,cls_t CLS,fill_t FILL,rl_t RL,kbd_t KBD)
{
    int sel=0,run=1;

    auto draw=[&](){
        CLS();
        FILL(0,0xF000);
        P(2,0,"[ BOOT SETTINGS ] - Available in Open-Source mode only",0xF000);
        P(10,2,"GitOS Configuration",CY);
        P(10,3,"-------------------",CX);

        P(12,5,"[1] OS Name    : ",CY); P(30,5,osname,CW);
        char t[8]; _si(t,*timeout);
        P(12,7,"[2] Timeout    : ",CG); P(30,7,t,CW); P(32,7,"s",CX);
        P(12,9,"[3] Default    : ",CC);
        P(30,9,*defmode==0?"Open-Source Boot":"Restricted Boot",CW);

        u16 sc=sel==3?0xF000:CG;
        u16 dc=sel==4?0xF000:CR;
        P(12,11,sel==3?"-> [S] Save & Exit  ":"   [S] Save & Exit  ",sc);
        P(12,12,sel==4?"-> [D] Discard      ":"   [D] Discard      ",dc);

        int rs[]={5,7,9,11,12};
        for(int i=0;i<5;i++)
            P(10,rs[i],i==sel?">>":"  ",i==sel?CY:CX);

        P(10,14,"[UP/DN] Navigate  [ENTER] Edit  [ESC] Back",CX);
        P(10,15,"[1-3] Quick select field",CX);
    };

    while(run){
        draw();
        while(!(_inb(0x64)&1))
            for(volatile int d=0;d<500;d++);
        u8 sc=_inb(0x60);

        if(sc==0x48)sel=(sel+4)%5;
        if(sc==0x50)sel=(sel+1)%5;
        if(sc==0x01)return;
        if(sc==0x02)sel=0;
        if(sc==0x03)sel=1;
        if(sc==0x04)sel=2;
        if(sc==0x1F)sel=3;
        if(sc==0x20)sel=4;

        if(sc==0x1C){
            if(sel==0){
                P(10,5,"New name: ",CY);
                char buf[32]="";
                if(RL(buf,32,5,CY)>0)_sc(osname,buf);
            } else if(sel==1){
                *timeout=(*timeout%9)+1;
            } else if(sel==2){
                *defmode=(*defmode+1)%2;
            } else if(sel==3){
                save();
                P(10,17,"  Saved successfully!  ",CG);
                for(volatile int d=0;d<5000000;d++);
                run=0;
            } else {
                run=0;
            }
        }
    }
}
