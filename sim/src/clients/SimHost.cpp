/* fb-sim: a pure static-file host for the browser client plus a generated /config.js carrying the
 * runtime env into the page. The simulator itself runs IN the browser; this only serves it.
 * Env: HTTP_PORT (8080), ORIGIN_LAT/LON, TILES_URL, SIM_UTC.
 *
 * The ONE thing it collects is the standpoint log: `POST /shot/NAME.png` and `POST /shot/NAME.json`
 * land in SHOT_ROOT, and every posted snapshot is also appended as one line to shots.jsonl. The
 * channel is one-way and diagnostic — nothing stored here is ever served back into a simulation. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>

#define MAX_CLIENTS 8
#define WEB_ROOT "web"
#define SHOT_ROOT "shots"
#define SHOT_LOG SHOT_ROOT "/shots.jsonl"
/* THE RUN LOG. Every client posts its whole log here in batches; one file per run id, appended, so a
 * run is reconstructible from this directory alone -- which mod, which scene, which build, which
 * numbers. The channel is one-way and diagnostic, exactly like the shots. */
#define LOG_ROOT "logs"
/* WHAT A RUN PRODUCED. A browser has no filesystem to write a PNG or a CSV to, so a declared
 * artifact lands here under its run id -- the same evidence a native run leaves on disk. */
#define ART_ROOT "runs"
/* The cap is there so a wrong Content-Length cannot ask for the heap. The largest DECLARED product
 * sets it: mods/demo's class dump is spanM 400 / stepM 0.05 = 8000 x 8000 bytes + header = 64.0 MB,
 * and 16 MB refused it with a 413 that nothing noticed. Doubled once so the next declared span does
 * not have to come back here. */
#define MAX_BODY (128u * 1024u * 1024u)
/* Cross-origin isolation, which is what a SharedArrayBuffer costs: without both headers the wasm
 * module's shared memory cannot be allocated and the client does not start at all. fb-tiles answers
 * with `Access-Control-Allow-Origin: *`, so its tiles stay reachable as CORS responses. */
#define ISOLATE "Cross-Origin-Opener-Policy: same-origin\r\n" \
                "Cross-Origin-Embedder-Policy: require-corp\r\n"

typedef struct {
    int fd;
    uint8_t rx[8192];
    int rxn;
    uint8_t *body;        /* POST only, malloc'd to Content-Length */
    size_t need, got;
    char name[128];       /* the basename this body belongs to */
    int kind;             /* 0 = a shot, 1 = a run-log batch, 2 = telemetry, 3 = an artifact */
} client_t;
static client_t cl[MAX_CLIENTS];

static void client_close(client_t *c){
    if(c->fd>=0) close(c->fd);
    c->fd=-1; c->rxn=0; c->need=0; c->got=0; c->name[0]=0; c->kind=0;
    free(c->body); c->body=NULL;
}

static void set_nonblock(int fd){ int f=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,f|O_NONBLOCK); }

/* Sends the WHOLE buffer even on a non-blocking socket: without the EAGAIN wait the multi-MB
 * gpu.wasm payload is silently truncated. */
static int send_all(int fd,const void*buf,size_t len){
    const uint8_t*p=(const uint8_t*)buf; size_t off=0;
    while(off<len){
        ssize_t w=send(fd,p+off,len-off,MSG_NOSIGNAL);
        if(w>0){ off+=(size_t)w; continue; }
        if(w<0 && (errno==EAGAIN||errno==EWOULDBLOCK)){
            struct pollfd pf={fd,POLLOUT,0}; if(poll(&pf,1,5000)<=0) return -1; continue;
        }
        return -1;
    }
    return 0;
}

static void reply(int fd,const char*status,const char*body){
    char hdr[256]; int hn=snprintf(hdr,sizeof hdr,
        "HTTP/1.1 %s\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
        status,(int)strlen(body));
    send_all(fd,hdr,hn); send_all(fd,body,strlen(body));
}

/* THE NAME COMES OFF THE NETWORK AND BECOMES A FILE, so it is a basename out of one alphabet and
 * nothing else: no separator, no dot pair, no length that a path buffer cannot hold. */
static int safe_name(const char*s){
    size_t n=strlen(s);
    if(n==0||n>96) return 0;
    for(size_t i=0;i<n;i++){
        const unsigned char ch=(unsigned char)s[i];
        if(!(isalnum(ch)||ch=='-'||ch=='_'||ch=='.')) return 0;
    }
    return strstr(s,"..")==NULL;
}

/* The picture lands beside its entry; the entry ALSO goes to the one append-only log, because a
 * reader tailing a sequence of standpoints must not have to list a directory to find the next one. */
static int shot_store(const char*name,const uint8_t*body,size_t n){
    mkdir(SHOT_ROOT,0775);
    char file[384]; snprintf(file,sizeof file,"%s/%s",SHOT_ROOT,name);
    FILE*f=fopen(file,"wb");
    if(!f) return 0;
    const size_t w=fwrite(body,1,n,f);
    fclose(f);
    if(w!=n) return 0;
    const size_t len=strlen(name);
    if(len>5&&strcmp(name+len-5,".json")==0){
        FILE*l=fopen(SHOT_LOG,"a");
        if(!l) return 0;
        /* ONE LINE PER ENTRY is the whole contract of this file, so a newline inside a posted body
         * is flattened rather than allowed to split the record. */
        for(size_t i=0;i<n;i++) fputc((body[i]=='\n'||body[i]=='\r')?' ':body[i],l);
        fputc('\n',l);
        fclose(l);
    }
    fprintf(stderr,"[fb-sim] shot %s (%zu B)\n",name,n);
    return 1;
}

/* Appended, never rewritten: a client posts its log in batches and the order of the batches is the
 * order of the run. */
static int artifact_store(const char*name,const uint8_t*body,size_t n){
    mkdir(ART_ROOT,0775);
    char file[384]; snprintf(file,sizeof file,"%s/%s",ART_ROOT,name);
    FILE*f=fopen(file,"wb");
    if(!f) return 0;
    const size_t w=fwrite(body,1,n,f);
    fclose(f);
    fprintf(stderr,"[fb-sim] artifact %s (%zu B)\n",name,n);
    return w==n;
}

static int log_append(const char*name,const uint8_t*body,size_t n,const char*ext){
    mkdir(LOG_ROOT,0775);
    char file[384]; snprintf(file,sizeof file,"%s/%s.%s",LOG_ROOT,name,ext);
    FILE*f=fopen(file,"ab");
    if(!f) return 0;
    const size_t w=fwrite(body,1,n,f);
    fclose(f);
    fprintf(stderr,"[fb-sim] log %s (+%zu B)\n",name,n);
    return w==n;
}

/* THE BUILD'S IDENTITY, taken off the bytes this server actually hands out. A browser cannot hash
 * the module it is currently executing without fetching it a second time, and an environment
 * variable would only repeat what somebody typed. FNV-1a 64 because this is an identity and not a
 * signature: it names a build, it defends nothing. */
static unsigned long long wasm_build_id(void){
    FILE*f=fopen(WEB_ROOT "/gpu.wasm","rb");
    if(!f) return 0;
    unsigned long long h=1469598103934665603ULL;
    unsigned char b[65536]; size_t n;
    while((n=fread(b,1,sizeof b,f))>0)
        for(size_t i=0;i<n;i++){ h^=b[i]; h*=1099511628211ULL; }
    fclose(f);
    return h;
}

/* 1 = served (the caller closes the fd), 0 = headers still incomplete or a body is being collected. */
static int http_handle(client_t*c){
    /* The recv is capped one byte short of the buffer so this terminator lands on FREE space. It used
     * to be clamped to the last slot instead, which zeroed a byte of whatever rode in behind the
     * headers — invisible until a POST body arrived, and then one corrupt PNG chunk per shot. */
    c->rx[c->rxn]=0;
    char*req=(char*)c->rx;
    char*hdrEnd=strstr(req,"\r\n\r\n");
    if(!hdrEnd) return 0;   /* wait for full headers */

    if(strncmp(req,"POST ",5)==0){
        char path[256]="/"; sscanf(req,"POST %255s",path);
        const char*cl_=strcasestr(req,"\r\nContent-Length:");
        long need=cl_?atol(cl_+17):-1;
        const char*base=NULL;
        if(strncmp(path,"/shot/",6)==0){ base=path+6; c->kind=0; }
        else if(strncmp(path,"/log/",5)==0){ base=path+5; c->kind=1; }
        else if(strncmp(path,"/telemetry/",11)==0){ base=path+11; c->kind=2; }
        else if(strncmp(path,"/artifact/",10)==0){ base=path+10; c->kind=3; }
        if(!base||!safe_name(base)){ reply(c->fd,"404 Not Found","no"); c->rxn=0; return 1; }
        if(need<0||(unsigned long)need>MAX_BODY){ reply(c->fd,"413 Payload Too Large","no"); c->rxn=0; return 1; }
        snprintf(c->name,sizeof c->name,"%s",base);
        c->need=(size_t)need;
        c->got=0;
        c->body=(uint8_t*)malloc(c->need?c->need:1);
        if(!c->body){ reply(c->fd,"500 Internal Server Error","no"); c->rxn=0; return 1; }
        /* Whatever of the body already rode in with the headers. */
        const size_t have=(size_t)(c->rxn-(int)(hdrEnd+4-req));
        const size_t take=have<c->need?have:c->need;
        memcpy(c->body,hdrEnd+4,take);
        c->got=take;
        c->rxn=0;
        if(c->got<c->need) return 0;   /* the read loop keeps filling c->body */
        const int ok=c->kind==3?artifact_store(c->name,c->body,c->got)
                    :c->kind  ?log_append(c->name,c->body,c->got,c->kind==2?"csv":"log")
                              :shot_store(c->name,c->body,c->got);
        reply(c->fd,ok?"200 OK":"500 Internal Server Error",ok?"ok":"no");
        return 1;
    }

    char path[256]="/"; sscanf(req,"GET %255s",path);
    char*q=strchr(path,'?'); if(q)*q=0;

    /* Server env -> the page. Set ORIGIN_LAT/ORIGIN_LON at container start to fly anywhere. */
    if(strcmp(path,"/config.js")==0){
        const char*la=getenv("ORIGIN_LAT"), *lo=getenv("ORIGIN_LON");
        /* A published host URL, not the podman-internal name: it must be reachable from the BROWSER,
         * not from this container. */
        const char*tu=getenv("TILES_URL");
        /* 0/unset = real time; a fixed value pins a reproducible sky. */
        const char*su=getenv("SIM_UTC");
        /* The camera never sinks below (ground + clearance), so the eye rests at the model's
         * geometry-true lowest height rather than on the dirt. */
        double clr=0.0; { FILE*cf=fopen("/tmp/fb_clearance","r"); if(cf){ if(fscanf(cf,"%lf",&clr)!=1) clr=0; fclose(cf); } }
        /* WHICH MOD, WHICH SCENE -- the browser's two words, and the only thing a client is told
         * beyond where the data lives. */
        const char*md=getenv("OUTSHINE_MOD"), *sc=getenv("OUTSHINE_SCENE");
        char body[512]; int bn=snprintf(body,sizeof body,
            "window.FB_ORIGIN_LAT=%s;window.FB_ORIGIN_LON=%s;window.FB_SIM_UTC=%s;window.FB_TILES_URL='%s';window.FB_GROUND_CLEAR=%.3f;"
            "window.FB_MOD=window.FB_MOD||'%s';window.FB_SCENE=window.FB_SCENE||'%s';window.FB_BUILD='%016llx';\n",
            la&&*la?la:"52.045", lo&&*lo?lo:"9.385", su&&*su?su:"0", tu&&*tu?tu:"", clr,
            md&&*md?md:"demo", sc&&*sc?sc:"walk", wasm_build_id());
        char hdr[320]; int hn=snprintf(hdr,sizeof hdr,
            "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\n" ISOLATE "Content-Length: %d\r\nConnection: close\r\n\r\n",bn);
        send(c->fd,hdr,hn,MSG_NOSIGNAL); send(c->fd,body,bn,MSG_NOSIGNAL); c->rxn=0; return 1;
    }

    char file[300];
    if(strcmp(path,"/")==0) snprintf(file,sizeof file,"%s/index.html",WEB_ROOT);
    else snprintf(file,sizeof file,"%s%s",WEB_ROOT,path);
    const char*mime="application/octet-stream";
    if(strstr(file,".html"))mime="text/html";
    else if(strstr(file,".js"))mime="application/javascript";
    else if(strstr(file,".wasm"))mime="application/wasm";
    else if(strstr(file,".css"))mime="text/css";
    FILE*f=fopen(file,"rb");
    if(!f){ const char*e="HTTP/1.1 404 Not Found\r\nContent-Length: 3\r\n\r\n404"; send(c->fd,e,strlen(e),MSG_NOSIGNAL); c->rxn=0; return 1; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    char hdr[256]; int hn=snprintf(hdr,sizeof hdr,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\n" ISOLATE "Content-Length: %ld\r\nConnection: close\r\n\r\n",mime,sz);
    send_all(c->fd,hdr,hn);
    char b[65536]; size_t r; while((r=fread(b,1,sizeof b,f))>0){ if(send_all(c->fd,b,r)<0) break; }
    fclose(f); c->rxn=0; return 1;
}

int main(void){
    signal(SIGPIPE, SIG_IGN);
    int port=getenv("HTTP_PORT")?atoi(getenv("HTTP_PORT")):8080;

    int lfd=socket(AF_INET,SOCK_STREAM,0); int one=1; setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    struct sockaddr_in sa={}; sa.sin_family=AF_INET; sa.sin_addr.s_addr=INADDR_ANY; sa.sin_port=htons(port);
    if(bind(lfd,(struct sockaddr*)&sa,sizeof sa)<0){ perror("tcp bind"); return 1; }
    listen(lfd,8); set_nonblock(lfd);
    for(int i=0;i<MAX_CLIENTS;i++) cl[i].fd=-1;

    fprintf(stderr,"[fb-sim] http :%d serving %s/ (browser Command Center)\n",port,WEB_ROOT);

    for(;;){
        fd_set rs; FD_ZERO(&rs); FD_SET(lfd,&rs); int mx=lfd;
        for(int i=0;i<MAX_CLIENTS;i++) if(cl[i].fd>=0){ FD_SET(cl[i].fd,&rs); if(cl[i].fd>mx)mx=cl[i].fd; }
        if(select(mx+1,&rs,NULL,NULL,NULL)<0){ if(errno==EINTR)continue; perror("select"); break; }

        if(FD_ISSET(lfd,&rs)){
            int fd=accept(lfd,NULL,NULL);
            if(fd>=0){ set_nonblock(fd); int i; for(i=0;i<MAX_CLIENTS;i++) if(cl[i].fd<0){ cl[i].fd=fd; cl[i].rxn=0; break; }
                if(i==MAX_CLIENTS) close(fd); }
        }

        for(int i=0;i<MAX_CLIENTS;i++){
            if(cl[i].fd<0||!FD_ISSET(cl[i].fd,&rs)) continue;
            if(cl[i].body){   /* a POST body is still arriving; length, not a delimiter, ends it */
                ssize_t n=recv(cl[i].fd,cl[i].body+cl[i].got,cl[i].need-cl[i].got,0);
                if(n<=0){ client_close(&cl[i]); continue; }
                cl[i].got+=(size_t)n;
                if(cl[i].got<cl[i].need) continue;
                const int ok=cl[i].kind==3
                    ?artifact_store(cl[i].name,cl[i].body,cl[i].got)
                    :cl[i].kind
                    ?log_append(cl[i].name,cl[i].body,cl[i].got,cl[i].kind==2?"csv":"log")
                    :shot_store(cl[i].name,cl[i].body,cl[i].got);
                reply(cl[i].fd,ok?"200 OK":"500 Internal Server Error",ok?"ok":"no");
                client_close(&cl[i]);
                continue;
            }
            const size_t room=sizeof cl[i].rx-1-(size_t)cl[i].rxn;
            if(room==0){ reply(cl[i].fd,"431 Request Header Fields Too Large","no"); client_close(&cl[i]); continue; }
            ssize_t n=recv(cl[i].fd,cl[i].rx+cl[i].rxn,room,0);
            if(n<=0){ client_close(&cl[i]); continue; }
            cl[i].rxn+=(int)n;
            if(http_handle(&cl[i])) client_close(&cl[i]);
        }
    }
    return 0;
}
