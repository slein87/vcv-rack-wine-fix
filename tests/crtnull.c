/* Calls one stdio function of a CRT DLL with a NULL stream and reports return value and errno.
   Usage: crtnull.exe "|<dll>|<function>|[h]"   (h = install an empty invalid parameter handler)
   One call per process, because a crash would take the remaining tests with it. Built without a CRT. */
typedef unsigned long DWORD; typedef unsigned long long u64; typedef long long i64;
__declspec(dllimport) void* LoadLibraryExA(const char*, void*, DWORD);
__declspec(dllimport) void* GetProcAddress(void*, const char*);
__declspec(dllimport) void ExitProcess(unsigned);
__declspec(dllimport) void* GetStdHandle(DWORD);
__declspec(dllimport) int WriteFile(void*, const void*, DWORD, DWORD*, void*);
__declspec(dllimport) char* GetCommandLineA(void);
__declspec(dllimport) unsigned SetErrorMode(unsigned);

static u64 slen(const char* s){u64 n=0; while(s[n]) n++; return n;}
static int seq(const char* a,const char* b){ while(*a&&*a==*b){a++;b++;} return *a==*b; }
static void out(const char* s){DWORD w; WriteFile(GetStdHandle((DWORD)-11), s, (DWORD)slen(s), &w, 0);}
static void outhex(u64 v){char b[19]; int i=18; b[i]=0; do{int d=v&15; b[--i]=d<10?'0'+d:'a'+d-10; v>>=4;}while(v); b[--i]='x'; b[--i]='0'; out(b+i);}
static void outnum(i64 v){char b[32]; int i=31; b[i]=0; int neg=v<0; if(neg) v=-v; do{b[--i]='0'+(v%10); v/=10;}while(v); if(neg) b[--i]='-'; out(b+i);}

static char args[3][64]; static char buf[64]; static unsigned short wbuf[32];
static const unsigned short wx[] = {'x',0};
static void nohandler(const void* a,const void* b,const void* c,unsigned d,u64 e){ (void)a;(void)b;(void)c;(void)d;(void)e; }

struct fn { const char* name; int kind; };
static const struct fn fns[] = {
 {"fread",0},{"fwrite",0},{"fgets",1},{"fgetws",11},{"fputs",2},{"fputws",12},
 {"fgetc",3},{"getc",3},{"fgetwc",3},{"_getw",3},{"ftell",3},{"_ftelli64",3},{"feof",3},{"ferror",3},
 {"_fileno",3},{"fclose",3},{"rewind",3},{"clearerr",3},
 {"fputc",4},{"putc",4},{"fputwc",4},{"_putw",4},{"ungetc",4},{"ungetwc",4},
 {"fseek",5},{"_fseeki64",5},{"setvbuf",6},{0,0}};

void entry(void){
    SetErrorMode(0x8003); /* no error dialogs on crash */
    char* c = GetCommandLineA(); int k=-1, j=0;
    for(; *c; c++){ if(*c=='"') continue; if(*c=='|'){ if(k>=0&&k<3) args[k][j]=0; k++; j=0; if(k>2) { c++; break; } continue;} if(k>=0 && k<3 && j<63) args[k][j++]=*c; }
    int wanthandler = (args[2][0]=='h');
    void* m = LoadLibraryExA(args[0],0,0);
    if(!m){ out(args[0]); out(" cannot be loaded\n"); ExitProcess(90); }
    int kind=-1; for(int i=0; fns[i].name; i++) if(seq(fns[i].name,args[1])) kind=fns[i].kind;
    void* p = GetProcAddress(m,args[1]);
    out(args[0]); out(" "); out(args[1]); out(wanthandler?" [handler]":"");
    if(kind<0||!p){ out(" -> export missing\n"); ExitProcess(91); }
    if(wanthandler){ void* (*sh)(void*) = (void*(*)(void*))GetProcAddress(m,"_set_invalid_parameter_handler"); if(sh) sh((void*)nohandler); else out(" (no _set_invalid_parameter_handler)"); }
    int* (*perrno)(void) = (int*(*)(void))GetProcAddress(m,"_errno");
    if(perrno) *perrno() = 57005; /* 0xdead */
    u64 r=0;
    switch(kind){
      case 0:  r=((u64(*)(void*,u64,u64,void*))p)(buf,1,16,0); break;
      case 1:  r=(u64)((char*(*)(char*,int,void*))p)(buf,16,0); break;
      case 11: r=(u64)((void*(*)(void*,int,void*))p)(wbuf,16,0); break;
      case 2:  r=(u64)(i64)((int(*)(const char*,void*))p)("x",0); break;
      case 12: r=(u64)(i64)((int(*)(const void*,void*))p)(wx,0); break;
      case 3:  r=(u64)((i64(*)(void*))p)(0); break;
      case 4:  r=(u64)(i64)((int(*)(int,void*))p)('x',0); break;
      case 5:  r=(u64)(i64)((int(*)(void*,i64,int))p)(0,0,0); break;
      case 6:  r=(u64)(i64)((int(*)(void*,char*,int,u64))p)(0,0,4 /*_IONBF*/,0); break;
    }
    out(" -> returned, ret="); outhex(r); out(" (int "); outnum((i64)(int)r); out(") errno="); if(perrno) outnum(*perrno()); else out("?"); out("\n");
    ExitProcess(0);
}
