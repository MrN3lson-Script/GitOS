typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define VGA ((u16*)0xB8000)
#define W 80
#define H 25

u8 TERM_BG = 0x00;
u8 TERM_FG = 0x0F;
u8 TERM_ROOT = 0x0F;
u8 TERM_ERR = 0x0C;
u8 TERM_SYS = 0x0E;

static inline u8  inb(u16 p) { u8 v; __asm__ volatile("inb %1,%0":"=a"(v):"dN"(p)); return v; }
static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0,%1"::"a"(v),"dN"(p)); }
static inline u16 inw(u16 p) { u16 v; __asm__ volatile("inw %1,%0":"=a"(v):"dN"(p)); return v; }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1"::"a"(v),"dN"(p)); }

static void cls() {
    u8 attr = (TERM_BG << 4) | TERM_FG;
    for(int i=0; i<W*H; i++) VGA[i] = ((u16)attr << 8) | ' ';
}
static void putc_at_attr(int x, int y, char c, u8 attr) {
    if(x<W && y<H) VGA[y*W+x] = ((u16)attr << 8) | (u8)c;
}
static void putc_at(int x, int y, char c, u8 fg) {
    u8 attr = (TERM_BG << 4) | fg;
    putc_at_attr(x, y, c, attr);
}
static void puts_at_attr(int x, int y, const char* s, u8 attr) {
    for(; *s && x<W; s++, x++) putc_at_attr(x, y, *s, attr);
}
static void puts_at(int x, int y, const char* s, u8 fg) {
    for(; *s && x<W; s++, x++) putc_at(x, y, *s, fg);
}
static void fill_row(int y, u8 attr) {
    for(int x=0; x<W; x++) VGA[y*W+x] = ((u16)attr << 8) | ' ';
}

static int slen(const char* s) { int n=0; while(s[n]) n++; return n; }
static void scpy(char* d, const char* s) { while((*d++ = *s++)); }
static void sncpy(char* d, const char* s, int n) { int i=0; while(i<n-1 && s[i]) { d[i]=s[i]; i++; } d[i]=0; }
static int scmp(const char* a, const char* b) { while(*a && *a==*b) { a++; b++; } return *a-*b; }
static void scat(char* d, const char* s) { while(*d) d++; while((*d++ = *s++)); }
static int starts(const char* s, const char* p) { while(*p) if(*s++ != *p++) return 0; return 1; }
static int scontains(const char* haystack, const char* needle) {
    int nlen = slen(needle);
    if(!nlen) return 0;
    while(*haystack) {
        if(starts(haystack, needle)) return 1;
        haystack++;
    }
    return 0;
}

static void sint(char* b, int n) {
    if(!n) { b[0]='0'; b[1]=0; return; }
    int neg=0;
    if(n<0) { neg=1; n=-n; }
    char t[12]; int i=0;
    while(n>0) { t[i++]='0'+(n%10); n/=10; }
    int j=0;
    if(neg) b[j++]='-';
    while(i>0) b[j++]=t[--i]; b[j]=0;
}
static int atoi(const char* s) {
    int neg=0; int res=0;
    if(*s=='-') { neg=1; s++; }
    while(*s >= '0' && *s <= '9') { res = res * 10 + (*s - '0'); s++; }
    return neg ? -res : res;
}

static void delay(int loops) {
    for(volatile int i=0; i<loops*10000; i++);
}

static int ata_wait_drq() {
    for(int i=0; i<100000; i++) {
        u8 status = inb(0x1F7);
        if(status & 0x80) continue;
        if(status & 0x01) return 0;
        if(status & 0x08) return 1;
    }
    return 0;
}
static void ata_read_sector(u32 lba, u16* buf) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1); outb(0x1F3, (u8)lba); outb(0x1F4, (u8)(lba >> 8)); outb(0x1F5, (u8)(lba >> 16));
    outb(0x1F7, 0x20);
    if(!ata_wait_drq()) { for(int i=0; i<256; i++) buf[i] = 0; return; }
    for(int i=0; i<256; i++) buf[i] = inw(0x1F0);
}
static void ata_write_sector(u32 lba, const u16* buf) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1); outb(0x1F3, (u8)lba); outb(0x1F4, (u8)(lba >> 8)); outb(0x1F5, (u8)(lba >> 16));
    outb(0x1F7, 0x30);
    if(!ata_wait_drq()) return;
    for(int i=0; i<256; i++) outw(0x1F0, buf[i]);
    outb(0x1F7, 0xE7);
}

#define MAX_PATH 128
#define MAX_FNAME 32
#define NFILES 64
#define FSIZE  2048

typedef struct {
    char name[MAX_FNAME];
    char data[FSIZE];
    int sz;
    int is_dir;
    int parent;
    u32 ctime;
    u32 atime;
    u8  perm;
} __attribute__((packed)) FSNode;

static FSNode fs[NFILES];
static int fscnt = 0;
static int cwd = 0;

static u32 ticks = 0;

static void fs_init() {
    for(int i=0; i<NFILES; i++) { fs[i].name[0]=0; fs[i].is_dir=0; }
    fs[0].name[0]='/'; fs[0].name[1]=0;
    fs[0].is_dir=1;
    fs[0].parent=0;
    fs[0].perm=075;
    fscnt=1;
    cwd=0;
}

static int fs_find_in(int parent, const char* name) {
    for(int i=0; i<fscnt; i++) {
        if(fs[i].name[0] && fs[i].parent==parent && !scmp(fs[i].name, name)) return i;
    }
    return -1;
}

static int fs_new_node(int parent, const char* name, int is_dir) {
    if(fscnt >= NFILES) return -1;
    sncpy(fs[fscnt].name, name, MAX_FNAME);
    fs[fscnt].is_dir = is_dir;
    fs[fscnt].parent = parent;
    fs[fscnt].sz = 0;
    fs[fscnt].data[0] = 0;
    fs[fscnt].ctime = ticks;
    fs[fscnt].atime = ticks;
    fs[fscnt].perm = is_dir ? 075 : 064;
    return fscnt++;
}

static int fs_resolve(const char* path, int* out_node) {
    if(!path || !*path) { *out_node = cwd; return 0; }
    int start = (path[0]=='/') ? 0 : cwd;
    if(path[0]=='/' && path[1]==0) { *out_node = 0; return 0; }
    
    char component[MAX_FNAME];
    int idx = (path[0]=='/') ? 1 : 0;
    int current = start;
    
    while(path[idx]) {
        int ci=0;
        while(path[idx] && path[idx]!='/' && ci<MAX_FNAME-1) component[ci++]=path[idx++];
        component[ci]=0;
        if(path[idx]=='/') idx++;
        
        if(!scmp(component, ".")) continue;
        if(!scmp(component, "..")) { current = fs[current].parent; continue; }
        
        int found = fs_find_in(current, component);
        if(found<0 || !fs[found].is_dir) return -1;
        current = found;
    }
    *out_node = current;
    return 0;
}

static void fs_get_path(int node, char* buf) {
    if(node==0) { buf[0]='/'; buf[1]=0; return; }
    char tmp[MAX_PATH];
    int idx=0;
    int cur=node;
    while(cur!=0) {
        int len=slen(fs[cur].name);
        for(int i=len-1; i>=0; i--) tmp[idx++]=fs[cur].name[i];
        tmp[idx++]='/';
        cur=fs[cur].parent;
    }
    tmp[idx++]='/';
    int j=0;
    buf[j++]='/';
    for(int i=idx-2; i>=0; i--) buf[j++]=tmp[i];
    buf[j]=0;
}

static void disk_sync_write() {
    u8* ptr = (u8*)fs;
    int total_sectors = (sizeof(fs) + 511) / 512;
    for (int s = 0; s < total_sectors; s++) {
        u16 sector_buf[256];
        for (int i = 0; i < 512; i++) ((u8*)sector_buf)[i] = ptr[s * 512 + i];
        ata_write_sector(100 + s, sector_buf);
    }
}
static void disk_sync_read() {
    u8* ptr = (u8*)fs;
    int total_sectors = (sizeof(fs) + 511) / 512;
    for (int s = 0; s < total_sectors; s++) {
        u16 sector_buf[256];
        ata_read_sector(100 + s, sector_buf);
        for (int i = 0; i < 512; i++) ptr[s * 512 + i] = ((u8*)sector_buf)[i];
    }
    fscnt = 0;
    for(int i=0; i<NFILES; i++) if(fs[i].name[0] != 0) fscnt = i+1;
}

static u8 kbd_scan() {
    while(!(inb(0x64) & 1)) ticks++;
    return inb(0x60);
}
static char sc2ch(u8 sc) {
    static const char t[128] = {
        0,  0, '1','2','3','4','5','6','7','8','9','0','-','=', 0,
        0, 'q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0, 'a','s','d','f','g','h','j','k','l',';','\'','`', 0,
       '\\','z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
    };
    return sc < 128 ? t[sc] : 0;
}

static int readline(char* buf, int max, int row, u16 prompt_len, u8 fg) {
    int i=0; buf[0]=0;
    while(1){
        puts_at(prompt_len, row, buf, fg);
        putc_at(prompt_len+i, row, '_', fg);
        u8 sc=kbd_scan();
        if(sc==0x1C) { buf[i]=0; putc_at(prompt_len+i, row, ' ', fg); return i; }
        if(sc==0x0E && i>0) {
            buf[--i]=0;
            putc_at(prompt_len+i, row, ' ', fg);
            putc_at(prompt_len+i+1, row, ' ', fg);
            continue;
        }
        if(sc==0x01) return -1;
        char c=sc2ch(sc);
        if(c && i<max-1) { buf[i++]=c; buf[i]=0; }
    }
}

static int shell_row;

static void sh_print(const char* s, u8 fg) {
    int len=slen(s);
    int pos=2;
    while(pos<W && *s) {
        int chunk=W-pos;
        if(chunk>len) chunk=len;
        for(int i=0; i<chunk; i++) putc_at(pos++, shell_row, *s++, fg);
        shell_row++;
        if(shell_row>=H-2) { cls(); shell_row=1; fill_row(0, 0xF0); puts_at_attr(2,0," Shell ",0xF0); }
        pos=2;
        len=slen(s);
    }
    if(*(s-1)!='\n') shell_row++;
    if(shell_row>=H-2) { cls(); shell_row=1; fill_row(0, 0xF0); puts_at_attr(2,0," Shell ",0xF0); }
}

static void run_matrix() {
    cls(); u32 rnd = 0xACE1u;
    for(int t=0; t<120; t++) {
        int x = rnd % W; rnd = (rnd >> 1) ^ (-(rnd & 1u) & 0xB400u); int y = rnd % H;
        putc_at(x, y, '0' + (rnd % 10), 0x0F); delay(1000);
    }
    cls(); shell_row=2;
}

static void execute_script(const char* path) {
    int node;
    if(fs_resolve(path, &node)<0 || fs[node].is_dir) { sh_print("run: not a file", TERM_ERR); return; }
    sh_print("--- Executing Script ---", 0x0F);
    char* ptr = fs[node].data;
    char line[64];
    while(*ptr) {
        int i=0;
        while(*ptr && *ptr!='\n' && i<62) line[i++]=*ptr++;
        line[i]=0;
        if(*ptr=='\n') ptr++;
        if(starts(line,"print ")) sh_print(line+6, TERM_FG);
        else if(!scmp(line,"clear")) { cls(); shell_row=1; }
        else if(!scmp(line,"delay")) delay(10000);
        else if(starts(line,"color ")) {
            if(line[6]=='1') TERM_BG=0x0F; else TERM_BG=0x00;
            cls(); shell_row=1;
        }
        else if(!scmp(line,"matrix")) run_matrix();
    }
    sh_print("--- Program Finished ---", 0x0F);
}

static void cmd_ls(const char* path) {
    int node;
    if(fs_resolve(path, &node)<0 || !fs[node].is_dir) { sh_print("ls: not a directory", TERM_ERR); return; }
    char buf[80];
    for(int i=0; i<fscnt; i++) {
        if(fs[i].name[0] && fs[i].parent==node) {
            buf[0]=fs[i].is_dir ? 'd' : '-';
            buf[1]='r'; buf[2]='w';
            buf[3]=(fs[i].perm&001)?'x':'-';
            buf[4]=' '; buf[5]=0;
            scat(buf, fs[i].name);
            if(fs[i].is_dir) scat(buf,"/");
            sh_print(buf, TERM_FG);
        }
    }
}

static void cmd_cd(const char* path) {
    int node;
    if(fs_resolve(path, &node)<0 || !fs[node].is_dir) { sh_print("cd: no such directory", TERM_ERR); return; }
    cwd=node;
}

static void cmd_cat(const char* path) {
    int node;
    if(fs_resolve(path, &node)<0 || fs[node].is_dir) { sh_print("cat: no such file", TERM_ERR); return; }
    fs[node].atime=ticks;
    sh_print(fs[node].data, TERM_FG);
}

static void cmd_touch(const char* path) {
    char dirpath[MAX_PATH]; char fname[MAX_FNAME];
    int last=-1, len=slen(path);
    for(int i=0; i<len; i++) if(path[i]=='/') last=i;
    int parent;
    if(last<0) { parent=cwd; sncpy(fname, path, MAX_FNAME); }
    else {
        sncpy(dirpath, path, last+2);
        dirpath[last+1]=0;
        sncpy(fname, path+last+1, MAX_FNAME);
        if(fs_resolve(dirpath, &parent)<0) { sh_print("touch: no such directory", TERM_ERR); return; }
    }
    int existing = fs_find_in(parent, fname);
    if(existing>=0) { fs[existing].atime=ticks; return; }
    fs_new_node(parent, fname, 0);
    disk_sync_write();
}

static void cmd_mkdir(const char* path) {
    char dirpath[MAX_PATH]; char dname[MAX_FNAME];
    int last=-1, len=slen(path);
    for(int i=0; i<len; i++) if(path[i]=='/') last=i;
    int parent;
    if(last<0) { parent=cwd; sncpy(dname, path, MAX_FNAME); }
    else {
        sncpy(dirpath, path, last+2);
        dirpath[last+1]=0;
        sncpy(dname, path+last+1, MAX_FNAME);
        if(fs_resolve(dirpath, &parent)<0) { sh_print("mkdir: no such directory", TERM_ERR); return; }
    }
    if(fs_find_in(parent, dname)>=0) { sh_print("mkdir: already exists", TERM_ERR); return; }
    fs_new_node(parent, dname, 1);
    disk_sync_write();
}

static void cmd_rm(const char* path) {
    int node;
    if(fs_resolve(path, &node)<0) { sh_print("rm: no such file", TERM_ERR); return; }
    if(fs[node].is_dir) { sh_print("rm: is a directory, use rmdir", TERM_ERR); return; }
    if(fs[node].parent==node) { sh_print("rm: permission denied", TERM_ERR); return; }
    fs[node].name[0]=0;
    disk_sync_write();
}

static void cmd_rmdir(const char* path) {
    int node;
    if(fs_resolve(path, &node)<0 || !fs[node].is_dir) { sh_print("rmdir: not a directory", TERM_ERR); return; }
    if(node==0) { sh_print("rmdir: cannot remove root", TERM_ERR); return; }
    for(int i=0; i<fscnt; i++) if(fs[i].parent==node) { sh_print("rmdir: directory not empty", TERM_ERR); return; }
    fs[node].name[0]=0;
    disk_sync_write();
}

static void cmd_cp(const char* src, const char* dst) {
    int snode, dnode;
    if(fs_resolve(src, &snode)<0 || fs[snode].is_dir) { sh_print("cp: source not a file", TERM_ERR); return; }
    char dirpath[MAX_PATH]; char fname[MAX_FNAME];
    int last=-1, len=slen(dst);
    for(int i=0; i<len; i++) if(dst[i]=='/') last=i;
    int parent;
    if(last<0) { parent=cwd; sncpy(fname, dst, MAX_FNAME); }
    else {
        sncpy(dirpath, dst, last+2); dirpath[last+1]=0;
        sncpy(fname, dst+last+1, MAX_FNAME);
        if(fs_resolve(dirpath, &parent)<0) { sh_print("cp: target dir not found", TERM_ERR); return; }
    }
    int existing = fs_find_in(parent, fname);
    if(existing>=0 && fs[existing].is_dir) { parent=existing; sncpy(fname, fs[snode].name, MAX_FNAME); }
    
    int target = fs_find_in(parent, fname);
    if(target<0) target = fs_new_node(parent, fname, 0);
    if(target<0) { sh_print("cp: cannot create", TERM_ERR); return; }
    sncpy(fs[target].data, fs[snode].data, FSIZE);
    fs[target].sz = fs[snode].sz;
    fs[target].atime = ticks;
    disk_sync_write();
}

static void cmd_mv(const char* src, const char* dst) {
    cmd_cp(src, dst);
    cmd_rm(src);
}

static void cmd_grep(const char* pattern, const char* path) {
    int node;
    if(fs_resolve(path, &node)<0 || fs[node].is_dir) { sh_print("grep: no such file", TERM_ERR); return; }
    char* data = fs[node].data;
    char line[256];
    int li=0;
    for(int i=0; data[i]; i++) {
        if(data[i]=='\n' || i==fs[node].sz-1) {
            line[li]=0;
            if(scontains(line, pattern)) sh_print(line, TERM_FG);
            li=0;
        } else if(li<255) line[li++]=data[i];
    }
}

static void cmd_wc(const char* path) {
    int node;
    if(fs_resolve(path, &node)<0 || fs[node].is_dir) { sh_print("wc: no such file", TERM_ERR); return; }
    int lines=0, words=0, bytes=fs[node].sz;
    int in_word=0;
    for(int i=0; i<bytes; i++) {
        char c=fs[node].data[i];
        if(c=='\n') lines++;
        if(c==' '||c=='\n'||c=='\t') in_word=0;
        else if(!in_word) { in_word=1; words++; }
    }
    char buf[64]="lines:"; char n[12];
    sint(n, lines); scat(buf, n); scat(buf, " words:");
    sint(n, words); scat(buf, n); scat(buf, " bytes:");
    sint(n, bytes); scat(buf, n);
    sh_print(buf, TERM_FG);
}

static void cmd_head_tail(const char* path, int lines_count, int tail) {
    int node;
    if(fs_resolve(path, &node)<0 || fs[node].is_dir) { sh_print("file not found", TERM_ERR); return; }
    char* data = fs[node].data;
    int total_lines=0;
    for(int i=0; data[i]; i++) if(data[i]=='\n') total_lines++;
    
    int start_line=tail ? (total_lines>lines_count ? total_lines-lines_count : 0) : 0;
    int end_line=tail ? total_lines : (lines_count<total_lines ? lines_count : total_lines);
    
    int ln=0, pos=0;
    char line[256];
    for(int i=0; data[i] && ln<=end_line; i++) {
        if(ln>=start_line) {
            if(data[i]=='\n' || i==fs[node].sz-1) {
                line[pos]=0;
                sh_print(line, TERM_FG);
                pos=0; ln++;
            } else if(pos<255) line[pos++]=data[i];
        } else if(data[i]=='\n') ln++;
    }
}

static void cmd_echo(const char* s) {
    sh_print(s, TERM_FG);
}

static void cmd_man(const char* cmd) {
    if(!scmp(cmd,"ls")) sh_print("ls [path] - list directory contents", TERM_SYS);
    else if(!scmp(cmd,"cd")) sh_print("cd <path> - change directory", TERM_SYS);
    else if(!scmp(cmd,"pwd")) sh_print("pwd - print working directory", TERM_SYS);
    else if(!scmp(cmd,"cat")) sh_print("cat <file> - print file contents", TERM_SYS);
    else if(!scmp(cmd,"touch")) sh_print("touch <file> - create empty file", TERM_SYS);
    else if(!scmp(cmd,"mkdir")) sh_print("mkdir <dir> - create directory", TERM_SYS);
    else if(!scmp(cmd,"rm")) sh_print("rm <file> - remove file", TERM_SYS);
    else if(!scmp(cmd,"rmdir")) sh_print("rmdir <dir> - remove empty directory", TERM_SYS);
    else if(!scmp(cmd,"cp")) sh_print("cp <src> <dst> - copy file", TERM_SYS);
    else if(!scmp(cmd,"mv")) sh_print("mv <src> <dst> - move/rename file", TERM_SYS);
    else if(!scmp(cmd,"grep")) sh_print("grep <pattern> <file> - search text", TERM_SYS);
    else if(!scmp(cmd,"wc")) sh_print("wc <file> - count lines/words/bytes", TERM_SYS);
    else if(!scmp(cmd,"head")) sh_print("head [-n N] <file> - first N lines", TERM_SYS);
    else if(!scmp(cmd,"tail")) sh_print("tail [-n N] <file> - last N lines", TERM_SYS);
    else if(!scmp(cmd,"echo")) sh_print("echo <text> - print text", TERM_SYS);
    else if(!scmp(cmd,"whoami")) sh_print("whoami - current user", TERM_SYS);
    else if(!scmp(cmd,"history")) sh_print("history - show command history", TERM_SYS);
    else if(!scmp(cmd,"exit")) sh_print("exit - exit shell", TERM_SYS);
    else if(!scmp(cmd,"reboot")) sh_print("reboot - restart system", TERM_SYS);
    else if(!scmp(cmd,"shutdown")) sh_print("shutdown - power off", TERM_SYS);
    else if(!scmp(cmd,"clear")) sh_print("clear - clear screen", TERM_SYS);
    else if(!scmp(cmd,"color")) sh_print("color <0/1> - set background", TERM_SYS);
    else if(!scmp(cmd,"ps")) sh_print("ps - list processes (kernel threads: 1)", TERM_SYS);
    else if(!scmp(cmd,"free")) sh_print("free - memory usage (simulated)", TERM_SYS);
    else if(!scmp(cmd,"df")) sh_print("df - disk usage", TERM_SYS);
    else if(!scmp(cmd,"du")) sh_print("du <dir> - directory size", TERM_SYS);
    else sh_print("No manual entry for this command", TERM_ERR);
}

static void cmd_ps() {
    sh_print("PID TTY TIME CMD", TERM_SYS);
    sh_print("  1 tty0 0:00 shell", TERM_FG);
    sh_print("  2 tty0 0:00 idle", TERM_FG);
}

static void cmd_free() {
    sh_print("MemTotal: 640K  MemFree: 512K  Swap: 0K", TERM_FG);
}

static void cmd_df() {
    sh_print("Filesystem 1K-blocks Used Available", TERM_SYS);
    sh_print("/dev/hda    51200     5    51195", TERM_FG);
}

static void cmd_du(const char* path) {
    int node;
    if(fs_resolve(path, &node)<0) { sh_print("du: no such directory", TERM_ERR); return; }
    int total=0;
    for(int i=0; i<fscnt; i++) if(fs[i].parent==node) total+=fs[i].sz;
    char buf[32]="size: "; char n[12]; sint(n,total); scat(buf,n); scat(buf," bytes");
    sh_print(buf, TERM_FG);
}

#define HIST_MAX 32
static char history[HIST_MAX][128];
static int hist_cnt=0;

static void add_history(const char* cmd) {
    if(hist_cnt<HIST_MAX) sncpy(history[hist_cnt++], cmd, 128);
}

static void shell() {
    cls();
    fill_row(0, 0xF0); fill_row(H-1, 0xF0);
    puts_at_attr(2, 0, " Shell v2.0 ", 0xF0);
    puts_at_attr(2, H-1, " Type 'help' or 'man <cmd>' | 'exit' to quit ", 0xF0);
    shell_row=2;
    
    char cwd_buf[MAX_PATH];
    fs_get_path(cwd, cwd_buf);
    sh_print(cwd_buf, TERM_SYS);
    
    char cmd[128];
    while(1){
        char prompt[32]="$ ";
        puts_at(0, shell_row, prompt, TERM_ROOT);
        if(readline(cmd, 100, shell_row, 2, TERM_FG)<0) continue;
        shell_row++;
        add_history(cmd);
        
        if(!scmp(cmd,"exit")) break;
        else if(!scmp(cmd,"help")) {
            sh_print("Commands: ls cd pwd cat touch mkdir rm rmdir cp mv", TERM_SYS);
            sh_print("grep wc head tail echo clear color whoami history", TERM_SYS);
            sh_print("man ps free df du reboot shutdown run <script>", TERM_SYS);
        }
        else if(!scmp(cmd,"pwd")) {
            fs_get_path(cwd, cwd_buf);
            sh_print(cwd_buf, TERM_FG);
        }
        else if(starts(cmd,"cd ")) cmd_cd(cmd+3);
        else if(starts(cmd,"ls")) {
            char* arg = cmd+2;
            while(*arg==' ') arg++;
            cmd_ls(*arg ? arg : ".");
        }
        else if(starts(cmd,"cat ")) cmd_cat(cmd+4);
        else if(starts(cmd,"touch ")) cmd_touch(cmd+6);
        else if(starts(cmd,"mkdir ")) cmd_mkdir(cmd+6);
        else if(starts(cmd,"rmdir ")) cmd_rmdir(cmd+6);
        else if(starts(cmd,"rm ")) cmd_rm(cmd+3);
        else if(starts(cmd,"cp ")) {
            char* p=cmd+3; while(*p==' ') p++;
            char* s=p; while(*s&&*s!=' ') s++;
            if(*s) { *s=0; s++; while(*s==' ') s++; cmd_cp(p, s); }
        }
        else if(starts(cmd,"mv ")) {
            char* p=cmd+3; while(*p==' ') p++;
            char* s=p; while(*s&&*s!=' ') s++;
            if(*s) { *s=0; s++; while(*s==' ') s++; cmd_mv(p, s); }
        }
        else if(starts(cmd,"grep ")) {
            char* p=cmd+5; while(*p==' ') p++;
            char* s=p; while(*s&&*s!=' ') s++;
            if(*s) { *s=0; s++; while(*s==' ') s++; cmd_grep(p, s); }
        }
        else if(starts(cmd,"wc ")) cmd_wc(cmd+3);
        else if(starts(cmd,"head ")) {
            int n=10; char* p=cmd+5;
            if(starts(p,"-n ")) { n=atoi(p+3); while(*p&&*p!=' ') p++; while(*p==' ') p++; }
            cmd_head_tail(p, n, 0);
        }
        else if(starts(cmd,"tail ")) {
            int n=10; char* p=cmd+5;
            if(starts(p,"-n ")) { n=atoi(p+3); while(*p&&*p!=' ') p++; while(*p==' ') p++; }
            cmd_head_tail(p, n, 1);
        }
        else if(starts(cmd,"echo ")) cmd_echo(cmd+5);
        else if(starts(cmd,"man ")) cmd_man(cmd+4);
        else if(!scmp(cmd,"whoami")) sh_print("root", TERM_FG);
        else if(!scmp(cmd,"history")) {
            for(int i=0; i<hist_cnt; i++) {
                char buf[8]; sint(buf,i); scat(buf," "); scat(buf,history[i]);
                sh_print(buf, TERM_FG);
            }
        }
        else if(!scmp(cmd,"ps")) cmd_ps();
        else if(!scmp(cmd,"free")) cmd_free();
        else if(!scmp(cmd,"df")) cmd_df();
        else if(starts(cmd,"du ")) cmd_du(cmd+3);
        else if(!scmp(cmd,"clear")||!scmp(cmd,"cls")) { cls(); shell_row=1; }
        else if(starts(cmd,"color ")) {
            if(cmd[6]=='1') TERM_BG=0x0F; else TERM_BG=0x00;
            cls(); shell_row=1;
        }
        else if(!scmp(cmd,"reboot")) { outb(0x64,0xFE); while(1); }
        else if(!scmp(cmd,"shutdown")) { outw(0x604,0x2000); outw(0xB004,0x2000); while(1); }
        else if(starts(cmd,"run ")) execute_script(cmd+4);
        else if(cmd[0]) {
            char err[96]; scpy(err,cmd); scat(err,": command not found");
            sh_print(err, TERM_ERR);
        }
    }
    cls();
    puts_at(2, 12, "System halted.", TERM_FG);
    while(1) __asm__("hlt");
}

extern "C" __attribute__((section(".text.boot"))) void kmain() {
    cls();
    fill_row(0, 0xF0); puts_at_attr(2, 0, "Loading...", 0xF0);
    delay(30);
    
    disk_sync_read();
    
    if(fs[0].name[0]!='/' || fs[0].is_dir!=1) {
        fs_init();
        fs_new_node(0, "home", 1);
        int home = fs_find_in(0, "home");
        int user = fs_new_node(home, "user", 1);
        int doc = fs_new_node(user, "documents", 1);
        int f = fs_new_node(doc, "readme.txt", 0);
        sncpy(fs[f].data, "Welcome! This is a sample file.", FSIZE);
        fs[f].sz = slen(fs[f].data);
        f = fs_new_node(doc, "script.gs", 0);
        sncpy(fs[f].data, "print Hello from GitScript!\ndelay\ncolor 1\nprint Done!\n", FSIZE);
        fs[f].sz = slen(fs[f].data);
        disk_sync_write();
    }
    cwd = 0;
    
    delay(20);
    shell();
    while(1) __asm__("hlt");
}
