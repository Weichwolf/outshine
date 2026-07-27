/* fb-sim: a pure static-file host for the browser client plus a generated /config.js carrying the
 * runtime env into the page. The simulator itself runs IN the browser; this only serves it.
 * Env: HTTP_PORT (8080), ORIGIN_LAT/LON, TILES_URL, SIM_UTC. */
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
#include <netinet/in.h>

#define MAX_CLIENTS 8
#define WEB_ROOT "web"

typedef struct { int fd; uint8_t rx[8192]; int rxn; } client_t;
static client_t cl[MAX_CLIENTS];

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

/* 1 = served (the caller closes the fd), 0 = headers still incomplete, keep reading. */
static int http_handle(client_t*c){
    c->rx[c->rxn<(int)sizeof c->rx-1?c->rxn:(int)sizeof c->rx-1]=0;
    char*req=(char*)c->rx;
    if(!strstr(req,"\r\n\r\n")) return 0;   /* wait for full headers */

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
        char body[384]; int bn=snprintf(body,sizeof body,
            "window.FB_ORIGIN_LAT=%s;window.FB_ORIGIN_LON=%s;window.FB_SIM_UTC=%s;window.FB_TILES_URL='%s';window.FB_GROUND_CLEAR=%.3f;\n",
            la&&*la?la:"52.045", lo&&*lo?lo:"9.385", su&&*su?su:"0", tu&&*tu?tu:"", clr);
        char hdr[192]; int hn=snprintf(hdr,sizeof hdr,
            "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",bn);
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
    char hdr[256]; int hn=snprintf(hdr,sizeof hdr,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n",mime,sz);
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
            ssize_t n=recv(cl[i].fd,cl[i].rx+cl[i].rxn,sizeof cl[i].rx-cl[i].rxn,0);
            if(n<=0){ close(cl[i].fd); cl[i].fd=-1; continue; }
            cl[i].rxn+=(int)n;
            if(http_handle(&cl[i])){ close(cl[i].fd); cl[i].fd=-1; }
        }
    }
    return 0;
}
