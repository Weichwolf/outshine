/* xpjsb/xplink — see xplink.h. Wire format mirrors X-Plane's UDP DATA protocol as iNav's
 * target/SITL/sim/xplane.c drives it: RREF = subscribe (id + dref string), DREF = a control value. */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "xplink.h"
#include "sensors.h"
#include "actuators.h"

#define XPJSB_MAX_SUBS 128
#define XPJSB_DREF_LEN 96

typedef struct { int32_t id; char dref[XPJSB_DREF_LEN]; } sub_t;
static sub_t subs[XPJSB_MAX_SUBS];
static int   nsubs = 0;
static struct sockaddr_in inav_addr;
static socklen_t inav_len = 0;
static int   have_inav = 0;

static void add_sub(int32_t id, const char *dref){
    for(int i=0;i<nsubs;i++) if(subs[i].id==id){ snprintf(subs[i].dref,XPJSB_DREF_LEN,"%s",dref); return; }
    if(nsubs<XPJSB_MAX_SUBS){ subs[nsubs].id=id; snprintf(subs[nsubs].dref,XPJSB_DREF_LEN,"%s",dref); nsubs++; }
}

int xpjsb_xp_open(int port){
    int fd = socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0) return -1;
    struct sockaddr_in me = {0}; me.sin_family=AF_INET; me.sin_addr.s_addr=INADDR_ANY; me.sin_port=htons(port);
    if(bind(fd,(struct sockaddr*)&me,sizeof me)<0){ close(fd); return -1; }
    int f=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,f|O_NONBLOCK);
    return fd;
}

void xpjsb_xp_recv(int fd){
    uint8_t buf[1200]; struct sockaddr_in src; socklen_t sl=sizeof src; ssize_t n;
    while((n=recvfrom(fd,buf,sizeof buf,0,(struct sockaddr*)&src,&sl))>0){
        if(n>=13 && !memcmp(buf,"RREF",4)){
            if(!have_inav){ inav_addr=src; inav_len=sl; have_inav=1; }
            int32_t id; memcpy(&id,buf+9,4);
            char dr[XPJSB_DREF_LEN]; snprintf(dr,sizeof dr,"%.*s",XPJSB_DREF_LEN-1,(char*)buf+13);
            add_sub(id,dr);
        } else if(n>=9 && !memcmp(buf,"DREF",4)){
            float val; memcpy(&val,buf+5,4);
            xpjsb_actuator_dref((char*)buf+9, val);
        }
    }
}

void xpjsb_xp_send(int fd){
    if(!have_inav || nsubs==0) return;
    uint8_t out[5 + XPJSB_MAX_SUBS*8]; memcpy(out,"RREF",4); out[4]=0;
    int o=5;
    for(int i=0;i<nsubs;i++){
        float v = xpjsb_sensor_value(subs[i].dref);
        memcpy(out+o, &subs[i].id, 4); memcpy(out+o+4, &v, 4); o+=8;
    }
    sendto(fd,out,o,0,(struct sockaddr*)&inav_addr,inav_len);
}
