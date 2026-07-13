/* FlightBox — ground station container ("the heart").
 *  - HTTP server: serves the WASM command center from ./web
 *  - WebSocket bridge: telemetry+video -> browser, control <- browser
 *  - UDP bridge: telemetry+video <- aircraft, control -> aircraft (fake radio)
 * Pure C, single-threaded select() loop. Env: AIRCRAFT_ADDR (default 127.0.0.1),
 * HTTP_PORT (default 8080). */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "protocol.h"

#define MAX_CLIENTS 8
#define WEB_ROOT "web"

/* ---------- SHA1 (for the WebSocket handshake) ---------- */
typedef struct { uint32_t h[5]; uint64_t len; uint8_t buf[64]; int n; } sha1_t;
static uint32_t rol(uint32_t v,int c){ return (v<<c)|(v>>(32-c)); }
static void sha1_blk(sha1_t*s,const uint8_t*p){
    uint32_t w[80];
    for(int i=0;i<16;i++) w[i]=(p[i*4]<<24)|(p[i*4+1]<<16)|(p[i*4+2]<<8)|p[i*4+3];
    for(int i=16;i<80;i++) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4];
    for(int i=0;i<80;i++){
        uint32_t f,k;
        if(i<20){f=(b&c)|(~b&d);k=0x5A827999;}
        else if(i<40){f=b^c^d;k=0x6ED9EBA1;}
        else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
        else {f=b^c^d;k=0xCA62C1D6;}
        uint32_t t=rol(a,5)+f+e+k+w[i]; e=d;d=c;c=rol(b,30);b=a;a=t;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;
}
static void sha1_init(sha1_t*s){ s->h[0]=0x67452301;s->h[1]=0xEFCDAB89;s->h[2]=0x98BADCFE;s->h[3]=0x10325476;s->h[4]=0xC3D2E1F0;s->len=0;s->n=0; }
static void sha1_upd(sha1_t*s,const uint8_t*p,size_t n){ s->len+=n; while(n--){ s->buf[s->n++]=*p++; if(s->n==64){sha1_blk(s,s->buf);s->n=0;} } }
static void sha1_fin(sha1_t*s,uint8_t out[20]){
    uint64_t bits=s->len*8; uint8_t c=0x80; sha1_upd(s,&c,1);
    c=0; while(s->n!=56) sha1_upd(s,&c,1);
    uint8_t L[8]; for(int i=0;i<8;i++) L[i]=(bits>>(56-i*8))&0xff; sha1_upd(s,L,8);
    for(int i=0;i<5;i++){ out[i*4]=s->h[i]>>24;out[i*4+1]=s->h[i]>>16;out[i*4+2]=s->h[i]>>8;out[i*4+3]=s->h[i]; }
}
static void b64(const uint8_t*in,int n,char*out){
    static const char*T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i,o=0; for(i=0;i+2<n;i+=3){ out[o++]=T[in[i]>>2];out[o++]=T[((in[i]&3)<<4)|(in[i+1]>>4)];out[o++]=T[((in[i+1]&15)<<2)|(in[i+2]>>6)];out[o++]=T[in[i+2]&63]; }
    if(i<n){ out[o++]=T[in[i]>>2]; if(i+1<n){ out[o++]=T[((in[i]&3)<<4)|(in[i+1]>>4)];out[o++]=T[(in[i+1]&15)<<2]; } else { out[o++]=T[(in[i]&3)<<4];out[o++]='='; } out[o++]='='; }
    out[o]=0;
}

/* ---------- client state ---------- */
typedef struct { int fd; int is_ws; uint8_t rx[8192]; int rxn; } client_t;
static client_t cl[MAX_CLIENTS];

static void set_nonblock(int fd){ int f=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,f|O_NONBLOCK); }

static void ws_send(int fd,const void*data,size_t len){
    uint8_t hdr[10]; int h=0; hdr[0]=0x82; /* FIN + binary */
    if(len<126){ hdr[1]=(uint8_t)len; h=2; }
    else if(len<65536){ hdr[1]=126; hdr[2]=len>>8; hdr[3]=len&0xff; h=4; }
    else { hdr[1]=127; for(int i=0;i<8;i++) hdr[2+i]=(len>>(56-i*8))&0xff; h=10; }
    /* best-effort; small LAN messages */
    if(send(fd,hdr,h,MSG_NOSIGNAL)<0) return;
    size_t off=0; const uint8_t*p=data;
    while(off<len){ ssize_t w=send(fd,p+off,len-off,MSG_NOSIGNAL); if(w<=0) break; off+=w; }
}

/* returns 1 if handshake done, 0 otherwise */
static int http_handle(client_t*c){
    c->rx[c->rxn<(int)sizeof c->rx-1?c->rxn:(int)sizeof c->rx-1]=0;
    char*req=(char*)c->rx;
    char*eoh=strstr(req,"\r\n\r\n"); if(!eoh) return 0;   /* wait for full headers */

    /* WebSocket upgrade? */
    char*key=strcasestr(req,"Sec-WebSocket-Key:");
    if(strstr(req,"GET /ws") && key){
        key+=18; while(*key==' ')key++; char k[128]; int i=0; while(*key!='\r'&&i<120) k[i++]=*key++; k[i]=0;
        char cat[200]; snprintf(cat,sizeof cat,"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",k);
        sha1_t s; sha1_init(&s); sha1_upd(&s,(uint8_t*)cat,strlen(cat)); uint8_t dig[20]; sha1_fin(&s,dig);
        char acc[40]; b64(dig,20,acc);
        char resp[256]; int n=snprintf(resp,sizeof resp,
            "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",acc);
        send(c->fd,resp,n,MSG_NOSIGNAL); c->is_ws=1; c->rxn=0; return 1;
    }

    /* static file GET */
    char path[256]="/"; sscanf(req,"GET %255s",path);
    char*q=strchr(path,'?'); if(q)*q=0;
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
    send(c->fd,hdr,hn,MSG_NOSIGNAL);
    char b[8192]; size_t r; while((r=fread(b,1,sizeof b,f))>0) send(c->fd,b,r,MSG_NOSIGNAL);
    fclose(f); c->rxn=0; return -1;   /* -1 => close after serving */
}

/* parse WS frames from client buffer; call cb() for each binary payload */
static void ws_parse(client_t*c, void(*cb)(const uint8_t*,int)){
    int off=0;
    while(c->rxn-off>=2){
        uint8_t*p=c->rx+off; uint8_t op=p[0]&0x0f; int masked=p[1]&0x80; uint64_t len=p[1]&0x7f; int hn=2;
        if(len==126){ if(c->rxn-off<4)break; len=(p[2]<<8)|p[3]; hn=4; }
        else if(len==127){ if(c->rxn-off<10)break; len=0; for(int i=0;i<8;i++)len=(len<<8)|p[2+i]; hn=10; }
        int need=hn+(masked?4:0)+(int)len; if(c->rxn-off<need) break;
        uint8_t*mask=p+hn; uint8_t*pl=p+hn+(masked?4:0);
        if(masked) for(uint64_t i=0;i<len;i++) pl[i]^=mask[i&3];
        if(op==0x2 || op==0x1){ cb(pl,(int)len); }        /* binary/text payload */
        /* op 0x8 close, 0x9 ping ignored for brevity */
        off+=need;
    }
    if(off){ memmove(c->rx,c->rx+off,c->rxn-off); c->rxn-=off; }
}

/* control -> UDP to aircraft. The aircraft's address is learned from the
 * downlink packets it sends us (recvfrom src), so no DNS needed here. */
static int udp_fd; static struct sockaddr_in up_addr; static int have_peer=0;
static void on_client_msg(const uint8_t*p,int n){
    if(have_peer && n==(int)sizeof(ctrl_packet_t) && ((ctrl_packet_t*)p)->magic==FB_MAGIC_CTRL)
        sendto(udp_fd,p,n,0,(struct sockaddr*)&up_addr,sizeof up_addr);
}

int main(void){
    signal(SIGPIPE, SIG_IGN);
    const char*ac=getenv("AIRCRAFT_ADDR"); if(!ac)ac="127.0.0.1";
    int port=getenv("HTTP_PORT")?atoi(getenv("HTTP_PORT")):8080;

    /* UDP: recv downlink, addr for uplink */
    udp_fd=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in du={0}; du.sin_family=AF_INET; du.sin_addr.s_addr=INADDR_ANY; du.sin_port=htons(FB_DOWN_PORT);
    if(bind(udp_fd,(struct sockaddr*)&du,sizeof du)<0){ perror("udp bind"); return 1; }
    set_nonblock(udp_fd);
    (void)ac;   /* aircraft addr is learned from incoming downlink packets */

    /* TCP listen for HTTP/WS */
    int lfd=socket(AF_INET,SOCK_STREAM,0); int one=1; setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    struct sockaddr_in sa={0}; sa.sin_family=AF_INET; sa.sin_addr.s_addr=INADDR_ANY; sa.sin_port=htons(port);
    if(bind(lfd,(struct sockaddr*)&sa,sizeof sa)<0){ perror("tcp bind"); return 1; }
    listen(lfd,8); set_nonblock(lfd);
    for(int i=0;i<MAX_CLIENTS;i++) cl[i].fd=-1;

    fprintf(stderr,"[flightbox] http :%d  uplink->%s:%d  downlink<-:%d\n",port,ac,FB_UP_PORT,FB_DOWN_PORT);

    for(;;){
        fd_set rs; FD_ZERO(&rs); FD_SET(lfd,&rs); FD_SET(udp_fd,&rs); int mx=lfd>udp_fd?lfd:udp_fd;
        for(int i=0;i<MAX_CLIENTS;i++) if(cl[i].fd>=0){ FD_SET(cl[i].fd,&rs); if(cl[i].fd>mx)mx=cl[i].fd; }
        struct timeval tv={1,0};
        if(select(mx+1,&rs,NULL,NULL,&tv)<0){ if(errno==EINTR)continue; perror("select"); break; }

        /* new connection */
        if(FD_ISSET(lfd,&rs)){
            int fd=accept(lfd,NULL,NULL);
            if(fd>=0){ set_nonblock(fd); int i; for(i=0;i<MAX_CLIENTS;i++) if(cl[i].fd<0){ cl[i].fd=fd; cl[i].is_ws=0; cl[i].rxn=0; break; }
                if(i==MAX_CLIENTS) close(fd); }
        }

        /* downlink from aircraft -> all WS clients */
        if(FD_ISSET(udp_fd,&rs)){
            static uint8_t buf[70000]; ssize_t n;
            struct sockaddr_in src; socklen_t sl=sizeof src;
            while((n=recvfrom(udp_fd,buf,sizeof buf,0,(struct sockaddr*)&src,&sl))>0){
                uint32_t mg = (n>=4)?((uint32_t*)buf)[0]:0;
                if(mg==FB_MAGIC_TELEM || mg==FB_MAGIC_VIDEO){
                    up_addr=src; up_addr.sin_port=htons(FB_UP_PORT); have_peer=1;  /* learn aircraft addr */
                    for(int i=0;i<MAX_CLIENTS;i++) if(cl[i].fd>=0&&cl[i].is_ws) ws_send(cl[i].fd,buf,n);
                }
            }
        }

        /* client data */
        for(int i=0;i<MAX_CLIENTS;i++){
            if(cl[i].fd<0||!FD_ISSET(cl[i].fd,&rs)) continue;
            ssize_t n=recv(cl[i].fd,cl[i].rx+cl[i].rxn,sizeof cl[i].rx-cl[i].rxn,0);
            if(n<=0){ close(cl[i].fd); cl[i].fd=-1; continue; }
            cl[i].rxn+=n;
            if(!cl[i].is_ws){ int r=http_handle(&cl[i]); if(r<0){ close(cl[i].fd); cl[i].fd=-1; } }
            else ws_parse(&cl[i], on_client_msg);
        }
    }
    return 0;
}
