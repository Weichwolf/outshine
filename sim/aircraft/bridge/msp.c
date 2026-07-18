/* FlightBox — MSP client to iNav SITL (TCP 5760): RC inject + telemetry read. See msp.h. */
#define _GNU_SOURCE
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "msp.h"

int msp_fd=-1;
static uint8_t msp_rx[8192]; static int msp_rxn=0;
float t_roll=0,t_pitch=0; static int t_yaw=0,t_fix=0,t_sats=0; int t_batt10=126;
static int t_inav_dth=0,t_inav_dir=0;   /* iNav's own distance/direction to home (MSP_COMP_GPS) */
static int t_estalt=0;                  /* iNav estimated altitude, cm (MSP_ALTITUDE) */
static double t_inav_lat=0,t_inav_lon=0;
uint32_t t_armflags=0; uint32_t t_modeflags=0;   /* MSP_STATUS flightModeFlags: iNavs bestätigte aktive Modi */
static uint8_t boxids[64]; static int nboxids=0;
int msp_connect(void){
    int fd=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in a={0}; a.sin_family=AF_INET; a.sin_port=htons(5760);
    inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);
    if(connect(fd,(struct sockaddr*)&a,sizeof a)<0){ close(fd); return -1; }
    int one=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one);
    int fl=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK);
    return fd;
}
void msp1(uint8_t cmd,const uint8_t*p,uint8_t n){
    if(msp_fd<0)return; uint8_t b[6+80]; b[0]='$';b[1]='M';b[2]='<';b[3]=n;b[4]=cmd; uint8_t k=n^cmd;
    for(int i=0;i<n;i++){b[5+i]=p[i];k^=p[i];} b[5+n]=k; send(msp_fd,b,6+n,MSG_NOSIGNAL);
}
static uint8_t crc8s(const uint8_t*d,int n){ uint8_t c=0; for(int i=0;i<n;i++){c^=d[i]; for(int j=0;j<8;j++)c=(c&0x80)?((c<<1)^0xD5):(c<<1);} return c; }
void msp2(uint16_t fn){
    if(msp_fd<0)return; uint8_t b[9]={'$','X','<',0,(uint8_t)(fn&0xff),(uint8_t)(fn>>8),0,0,0}; b[8]=crc8s(b+3,5); send(msp_fd,b,9,MSG_NOSIGNAL);
}
void msp_poll(void){
    if(msp_fd<0)return; ssize_t n;
    while((n=recv(msp_fd,msp_rx+msp_rxn,sizeof msp_rx-msp_rxn,0))>0){ msp_rxn+=n; if(msp_rxn>(int)sizeof msp_rx-800)msp_rxn=0; }
    int i=0;
    while(i+3<msp_rxn){
        if(msp_rx[i]=='$'&&msp_rx[i+1]=='M'&&msp_rx[i+2]=='>'){
            if(i+5>msp_rxn)break; int ln=msp_rx[i+3],cmd=msp_rx[i+4]; if(i+6+ln>msp_rxn)break; uint8_t*pl=msp_rx+i+5;
            if(cmd==108&&ln>=6){ t_roll=(int16_t)(pl[0]|pl[1]<<8)/10.0f; t_pitch=(int16_t)(pl[2]|pl[3]<<8)/10.0f; t_yaw=(int16_t)(pl[4]|pl[5]<<8); }
            else if(cmd==106&&ln>=2){ t_fix=pl[0]; t_sats=pl[1];
                if(ln>=10){ int32_t la,lo; memcpy(&la,pl+2,4); memcpy(&lo,pl+6,4); t_inav_lat=la/1e7; t_inav_lon=lo/1e7; } }
            else if(cmd==107&&ln>=4){ t_inav_dth=pl[0]|pl[1]<<8; t_inav_dir=pl[2]|pl[3]<<8; }
            else if(cmd==109&&ln>=4){ int32_t a; memcpy(&a,pl,4); t_estalt=a; }   /* est alt cm */
            else if(cmd==110&&ln>=1){ t_batt10=pl[0]; }
            else if(cmd==101&&ln>=10) memcpy(&t_modeflags,pl+6,4);
            else if(cmd==119){ nboxids=ln<64?ln:64; memcpy(boxids,pl,nboxids); }
            i+=6+ln;
        } else if(msp_rx[i]=='$'&&msp_rx[i+1]=='X'&&msp_rx[i+2]=='>'){
            if(i+8>msp_rxn)break; int ln=msp_rx[i+6]|msp_rx[i+7]<<8,fn=msp_rx[i+4]|msp_rx[i+5]<<8; if(i+9+ln>msp_rxn)break;
            if(fn==0x2000&&ln>=13) memcpy(&t_armflags,msp_rx+i+8+9,4);
            i+=9+ln;
        } else i++;
    }
    if(i>0){ memmove(msp_rx,msp_rx+i,msp_rxn-i); msp_rxn-=i; }
}
int mode_active(int boxid){ for(int k=0;k<nboxids;k++) if(boxids[k]==boxid) return (t_modeflags>>k)&1; return 0; }
