/* Freestanding reproducer (no CRT, builds with clang + lld, no mingw needed).
   mode f: calls msvcrt!fread(buf, 1, 16, NULL)
   mode a: loads libRack.dll and calls rack::system::archiveDirectory(archive, dir, 1) */
typedef unsigned long DWORD; typedef unsigned long long u64;
__declspec(dllimport) void* LoadLibraryExA(const char*, void*, DWORD);
__declspec(dllimport) void* GetProcAddress(void*, const char*);
__declspec(dllimport) void ExitProcess(unsigned);
__declspec(dllimport) void* GetStdHandle(DWORD);
__declspec(dllimport) int WriteFile(void*, const void*, DWORD, DWORD*, void*);
__declspec(dllimport) char* GetCommandLineA(void);
__declspec(dllimport) DWORD GetLastError(void);

static u64 slen(const char* s){u64 n=0; while(s[n]) n++; return n;}
static void out(const char* s){DWORD w; WriteFile(GetStdHandle((DWORD)-11), s, (DWORD)slen(s), &w, 0);}
static void outnum(long long v){char b[32]; int i=31; b[i]=0; int neg=v<0; if(neg) v=-v; do{b[--i]='0'+(v%10); v/=10;}while(v); if(neg) b[--i]='-'; out(b+i);}

/* libstdc++ std::string (cxx11 ABI) */
struct gstr { const char* p; u64 n; u64 cap; u64 pad; };
static void mk(struct gstr* s, const char* c){ s->p=c; s->n=slen(c); s->cap=s->n; s->pad=0; }

static char args[3][600];
static char buf[64];

void entry(void){
    /* command line: prog "|mode|arg1|arg2", fields separated by '|' */
    char* c = GetCommandLineA(); int k=-1, j=0;
    for(; *c; c++){ if(*c=='|'){ if(k>=0) args[k][j]=0; k++; j=0; if(k>2) break; continue;} if(*c=='"') continue; if(k>=0 && j<599) args[k][j++]=*c; }
    if(k>=0 && k<=2) args[k][j]=0;
    out("mode="); out(args[0]); out("\n");
    if(args[0][0]=='f'){
        void* m = LoadLibraryExA("msvcrt.dll",0,0);
        u64 (*fr)(void*,u64,u64,void*) = (u64(*)(void*,u64,u64,void*))GetProcAddress(m,"fread");
        out("calling msvcrt!fread(buf,1,16,NULL)\n");
        u64 r = fr(buf,1,16,0);
        out("fread returned: "); outnum((long long)r); out("\n");
        ExitProcess(0);
    }
    /* mode a: |a|<libRack.dll>|<dir> -> archives dir to dir + ".tar.zst" */
    void* h = LoadLibraryExA(args[1],0,8 /*LOAD_WITH_ALTERED_SEARCH_PATH*/);
    if(!h){ out("LoadLibrary failed, error "); outnum(GetLastError()); out("\n"); ExitProcess(2); }
    void (*ad)(struct gstr*, struct gstr*, int) = (void(*)(struct gstr*, struct gstr*, int))
        GetProcAddress(h,"_ZN4rack6system16archiveDirectoryERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES8_i");
    if(!ad){ out("archiveDirectory export not found\n"); ExitProcess(3); }
    static char outp[700]; int i=0; for(const char* q=args[2]; *q; q++) outp[i++]=*q; const char* ext=".tar.zst"; for(const char* q=ext; *q; q++) outp[i++]=*q; outp[i]=0;
    struct gstr a, d; mk(&a,outp); mk(&d,args[2]);
    out("calling rack::system::archiveDirectory\n");
    ad(&a,&d,1);
    out("archiveDirectory returned\n");
    ExitProcess(0);
}
