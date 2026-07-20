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
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
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

/* Send the whole buffer even on a non-blocking socket: on EAGAIN, wait for the
 * send buffer to drain (poll POLLOUT). Essential for the 32 MB cc.data payload,
 * which otherwise gets silently truncated. Returns 0 on success, -1 on error. */
static int send_all(int fd,const void*buf,size_t len){
    const uint8_t*p=buf; size_t off=0;
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

    /* runtime config: origin (home) coordinates from server env -> command center.
     * Set ORIGIN_LAT / ORIGIN_LON at container start to fly anywhere. */
    if(strcmp(path,"/config.js")==0){
        const char*la=getenv("ORIGIN_LAT"), *lo=getenv("ORIGIN_LON");
        /* TILES_URL points the browser at fb-tiles. Empty => the renderer falls back to the
         * legacy preloaded region archive. It must be reachable from the BROWSER, not from
         * this container, so it is a published host URL, not the podman-internal name. */
        const char*tu=getenv("TILES_URL");
        /* SIM_UTC (Unix seconds; 0/unset = real time) pins a reproducible sky for night/dusk shots;
         * the aircraft honours the same env for the sun, so both agree on the instant. */
        const char*su=getenv("SIM_UTC");
        char body[384]; int bn=snprintf(body,sizeof body,
            "window.FB_ORIGIN_LAT=%s;window.FB_ORIGIN_LON=%s;window.FB_SIM_UTC=%s;window.FB_TILES_URL='%s';\n",
            la&&*la?la:"52.045", lo&&*lo?lo:"9.385", su&&*su?su:"0", tu&&*tu?tu:"");
        char hdr[192]; int hn=snprintf(hdr,sizeof hdr,
            "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",bn);
        send(c->fd,hdr,hn,MSG_NOSIGNAL); send(c->fd,body,bn,MSG_NOSIGNAL); c->rxn=0; return -1;
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

/* ===================== MSP link to iNav — the CC's radio, MSP standard ============================
 * The ground station speaks to the aircraft over iNav's own MSP protocol (TCP 5760): it POLLS telemetry
 * (attitude/GPS/altitude/analog/comp-gps) and UPLINKS pilot RC as MSP_SET_RAW_RC. Same wire iNav uses on
 * real hardware — no custom radio. Telemetry is packed into telem_packet_t for the browser HUD/3D view. */
#include <netinet/tcp.h>
#include <math.h>
#define MSP_PORT 5762   /* iNav SITL UART3 (base 5760 + 2): a dedicated MSP link for the CC,
                             * separate from the bridge (5760) and the headless test CC (5761) */
static int msp_fd=-1; static uint8_t msp_rx[8192]; static int msp_rxn=0;
static double ORIGIN_LAT=52.045, ORIGIN_LON=9.385;
static float mt_roll,mt_pitch,mt_yaw,mt_alt,mt_vs,mt_gs,mt_batt=12;
static double mt_lat,mt_lon; static int mt_hdist,mt_hdir, mt_state=ST_DISARMED, mt_arm=0;

static int msp_connect(const char*host){
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0)return -1;
    struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_port=htons(MSP_PORT);
    if(inet_pton(AF_INET,host,&a.sin_addr)!=1){ struct hostent*he=gethostbyname(host); if(!he){close(fd);return -1;} memcpy(&a.sin_addr,he->h_addr,he->h_length); }
    if(connect(fd,(struct sockaddr*)&a,sizeof a)<0){ close(fd); return -1; }
    int one=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one); set_nonblock(fd);
    return fd;
}
static void msp1(uint8_t cmd,const uint8_t*p,uint8_t n){
    if(msp_fd<0) return;
    uint8_t b[6+64]; b[0]='$';b[1]='M';b[2]='<';b[3]=n;b[4]=cmd; uint8_t k=n^cmd;
    for(int i=0;i<n;i++){b[5+i]=p[i];k^=p[i];} b[5+n]=k; send(msp_fd,b,6+n,MSG_NOSIGNAL);
}
static uint8_t crc8s(const uint8_t*d,int n){ uint8_t c=0; for(int i=0;i<n;i++){c^=d[i]; for(int j=0;j<8;j++)c=(c&0x80)?((c<<1)^0xD5):(c<<1);} return c; }
static void msp2(uint16_t fn){ if(msp_fd<0)return; uint8_t b[9]={'$','X','<',0,(uint8_t)fn,(uint8_t)(fn>>8),0,0,0}; b[8]=crc8s(b+3,5); send(msp_fd,b,9,MSG_NOSIGNAL); }
static void msp_setrc(const uint16_t*rc,int n){
    if(msp_fd<0) return;
    uint8_t p[32]; for(int i=0;i<n;i++){p[2*i]=rc[i]&0xff;p[2*i+1]=rc[i]>>8;} msp1(200,p,2*n);
}
static void msp_poll(void){
    if(msp_fd<0) return;
    ssize_t n;
    while((n=recv(msp_fd,msp_rx+msp_rxn,sizeof msp_rx-msp_rxn,0))>0){ msp_rxn+=n; if(msp_rxn>(int)sizeof msp_rx-800)msp_rxn=0; }
    int i=0;
    while(i+3<msp_rxn){
        if(msp_rx[i]=='$'&&msp_rx[i+1]=='M'&&msp_rx[i+2]=='>'){
            if(i+5>msp_rxn) break;
            int ln=msp_rx[i+3],cmd=msp_rx[i+4]; if(i+6+ln>msp_rxn) break;
            uint8_t*pl=msp_rx+i+5;
            if(cmd==108&&ln>=6){ mt_roll=(int16_t)(pl[0]|pl[1]<<8)/10.0f; mt_pitch=(int16_t)(pl[2]|pl[3]<<8)/10.0f; mt_yaw=(int16_t)(pl[4]|pl[5]<<8); }
            else if(cmd==106&&ln>=16){ int32_t la,lo; memcpy(&la,pl+2,4); memcpy(&lo,pl+6,4); mt_lat=la/1e7; mt_lon=lo/1e7; mt_gs=(pl[12]|pl[13]<<8)/100.0f; }
            else if(cmd==109&&ln>=6){ int32_t a; memcpy(&a,pl,4); mt_alt=a/100.0f; mt_vs=(int16_t)(pl[4]|pl[5]<<8)/100.0f; }
            else if(cmd==110&&ln>=1){ mt_batt=pl[0]/10.0f; }
            else if(cmd==107&&ln>=4){ mt_hdist=pl[0]|pl[1]<<8; mt_hdir=(int16_t)(pl[2]|pl[3]<<8); }
            else if(cmd==121&&ln>=1){ int ns=pl[1]; mt_state = ns>=3?ST_CLIMB:(mt_arm?ST_MANUAL:ST_ARMED); }
            i+=6+ln;
        } else if(msp_rx[i]=='$'&&msp_rx[i+1]=='X'&&msp_rx[i+2]=='>'){
            if(i+8>msp_rxn) break;
            int ln=msp_rx[i+6]|msp_rx[i+7]<<8,fn=msp_rx[i+4]|msp_rx[i+5]<<8; if(i+9+ln>msp_rxn) break;
            if(fn==0x2000&&ln>=13){ uint32_t af; memcpy(&af,msp_rx+i+8+9,4); mt_arm=(af&0x4)!=0; if(!mt_arm)mt_state=ST_DISARMED; }
            i+=9+ln;
        } else i++;
    }
    if(i>0){ memmove(msp_rx,msp_rx+i,msp_rxn-i); msp_rxn-=i; }
}
static void msp_request_telemetry(void){ msp2(0x2000); msp1(108,0,0); msp1(106,0,0); msp1(109,0,0); msp1(110,0,0); msp1(107,0,0); msp1(121,0,0); }
static void build_telem(telem_packet_t*t){
    memset(t,0,sizeof *t); t->magic=FB_MAGIC_TELEM;
    t->roll=mt_roll; t->pitch=mt_pitch; t->yaw=mt_yaw; t->alt=mt_alt; t->gs=mt_gs; t->vs=mt_vs; t->airspeed=mt_gs;
    double clat=mt_lat?mt_lat:ORIGIN_LAT;
    t->x=(float)((mt_lon-ORIGIN_LON)*111320.0*cos(clat*M_PI/180.0));   /* ENU east  */
    t->y=(float)((mt_lat-ORIGIN_LAT)*111320.0);                        /* ENU north */
    t->batt=mt_batt; t->home_dist=mt_hdist;
    float rel=mt_hdir-mt_yaw; while(rel>180)rel-=360; while(rel<-180)rel+=360; t->home_bearing=rel;
    t->cloud=0; t->vis=40000; t->sun_el=35; t->sun_az=160;             /* daylight sky for the SVS view */
    t->state=(uint8_t)mt_state; t->rssi=100; static uint16_t sq; t->seq=sq++;
}

/* browser control -> RC uplink over MSP (MSP_SET_RAW_RC): the pilot flies via the standard radio. */
static void on_client_msg(const uint8_t*p,int n){
    if(n!=(int)sizeof(ctrl_packet_t) || ((ctrl_packet_t*)p)->magic!=FB_MAGIC_CTRL) return;
    const ctrl_packet_t*c=(const ctrl_packet_t*)p;
    uint16_t rc[8]={ (uint16_t)(1500+c->roll*500), (uint16_t)(1500+c->pitch*500),
                     (uint16_t)(1000+c->throttle*1000), (uint16_t)(1500+c->yaw*500),
                     (uint16_t)(c->arm?2000:1000), 2000, 1000, 1000 };  /* AUX1=ARM, AUX2=ANGLE */
    for(int i=0;i<4;i++){ if(rc[i]<1000)rc[i]=1000; if(rc[i]>2000)rc[i]=2000; }
    msp_setrc(rc,8);
}

int main(void){
    signal(SIGPIPE, SIG_IGN);
    const char*ac=getenv("AIRCRAFT_ADDR"); if(!ac)ac="127.0.0.1";
    int port=getenv("HTTP_PORT")?atoi(getenv("HTTP_PORT")):8080;

    /* UDP: recv downlink, addr for uplink */
    if(getenv("ORIGIN_LAT")) ORIGIN_LAT=atof(getenv("ORIGIN_LAT"));
    if(getenv("ORIGIN_LON")) ORIGIN_LON=atof(getenv("ORIGIN_LON"));
    msp_fd=msp_connect(ac);   /* MSP link to iNav on the aircraft (retried below if the aircraft is not up yet) */

    /* TCP listen for HTTP/WS */
    int lfd=socket(AF_INET,SOCK_STREAM,0); int one=1; setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
    struct sockaddr_in sa={0}; sa.sin_family=AF_INET; sa.sin_addr.s_addr=INADDR_ANY; sa.sin_port=htons(port);
    if(bind(lfd,(struct sockaddr*)&sa,sizeof sa)<0){ perror("tcp bind"); return 1; }
    listen(lfd,8); set_nonblock(lfd);
    for(int i=0;i<MAX_CLIENTS;i++) cl[i].fd=-1;

    fprintf(stderr,"[flightbox] http :%d  MSP<->%s:%d (telemetry downlink + RC/WP uplink)\n",port,ac,MSP_PORT);

    long tick=0;
    for(;;){
        fd_set rs; FD_ZERO(&rs); FD_SET(lfd,&rs); int mx=lfd;
        if(msp_fd>=0){ FD_SET(msp_fd,&rs); if(msp_fd>mx)mx=msp_fd; }
        for(int i=0;i<MAX_CLIENTS;i++) if(cl[i].fd>=0){ FD_SET(cl[i].fd,&rs); if(cl[i].fd>mx)mx=cl[i].fd; }
        struct timeval tv={0,50000};   /* 50 ms tick -> ~20 Hz telemetry to the browser */
        if(select(mx+1,&rs,NULL,NULL,&tv)<0){ if(errno==EINTR)continue; perror("select"); break; }

        /* new connection */
        if(FD_ISSET(lfd,&rs)){
            int fd=accept(lfd,NULL,NULL);
            if(fd>=0){ set_nonblock(fd); int i; for(i=0;i<MAX_CLIENTS;i++) if(cl[i].fd<0){ cl[i].fd=fd; cl[i].is_ws=0; cl[i].rxn=0; break; }
                if(i==MAX_CLIENTS) close(fd); }
        }

        /* MSP replies from the aircraft */
        if(msp_fd>=0 && FD_ISSET(msp_fd,&rs)) msp_poll();

        /* ~20 Hz telemetry tick: (re)connect MSP, ask iNav for telemetry, pack it, push to WS clients */
        tick++;
        if(msp_fd<0 && tick%20==0){ msp_fd=msp_connect(ac); }   /* retry until the aircraft is up */
        if(msp_fd>=0){
            msp_request_telemetry(); msp_poll();
            telem_packet_t t; build_telem(&t);
            for(int i=0;i<MAX_CLIENTS;i++) if(cl[i].fd>=0&&cl[i].is_ws) ws_send(cl[i].fd,(uint8_t*)&t,sizeof t);
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
