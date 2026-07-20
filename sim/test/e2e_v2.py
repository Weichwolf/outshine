#!/usr/bin/env python3
"""Headless E2E for the clean architecture: spawn the aircraft container (clean xpjsb bridge + vanilla
iNav), act as the thin Command Center over MSP (arm -> upload waypoints -> NAV WP), and assert the
aircraft flies its mission. iNav flies natively; the CC only commands. One robust process — no bash
backgrounding. Usage: e2e_v2.py [aircraft] [seconds] ; optional env MOUNT_EEPROM, MOUNT_MOTOR."""
import socket, struct, subprocess, sys, threading, time, math, os

AC   = sys.argv[1] if len(sys.argv)>1 else "c172"
SECS = float(sys.argv[2]) if len(sys.argv)>2 else 180.0
ORIG = (47.666, 9.49789)                                   # EDNY 06 threshold
WPS  = [(47.6730,9.5150,153.98),(47.6770,9.5270,204.97),(47.6720,9.5100,152.02)]  # lat,lon,alt(home-rel m)
CAP_R = 200.0        # m 2D capture radius
TAKEOFF_AGL = 50.0
CLIMB_S = 8.0        # wings-level ANGLE climb-out after arming before engaging NAV WP (safe altitude)

def crc8(c,b):
    c^=b
    for _ in range(8): c=((c<<1)^0xD5)&0xff if c&0x80 else (c<<1)&0xff
    return c

class MSP:
    def __init__(s,h,p):
        s.f=socket.create_connection((h,p),timeout=3); s.f.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1); s.f.settimeout(0.4)
    def send(s,cmd,pl=b""):
        h=bytes([0x24,0x58,0x3C,0,cmd&0xff,cmd>>8,len(pl)&0xff,len(pl)>>8]); c=0
        for b in h[3:]+pl: c=crc8(c,b)
        s.f.sendall(h+pl+bytes([c]))
    def recv(s):
        st=0;n=0;got=0;cmd=0;pl=bytearray()
        try:
            for _ in range(4096):
                d=s.f.recv(1)
                if not d: return None,None
                b=d[0]
                if st==0: st=1 if b==0x24 else 0
                elif st==1: st=2 if b==0x58 else 0
                elif st==2: st=3 if b in (0x3E,0x21) else 0
                elif st==3: st=4
                elif st==4: cmd=b;st=5
                elif st==5: cmd|=b<<8;st=6
                elif st==6: n=b;st=7
                elif st==7: n|=b<<8;got=0;st=8 if n else 9
                elif st==8:
                    pl.append(b);got+=1
                    if got==n: st=9
                elif st==9: return cmd,bytes(pl)
        except socket.timeout: return None,None
        return None,None
    def req(s,cmd,pl=b""):
        s.send(cmd,pl)
        for _ in range(30):
            c,p=s.recv()
            if c==cmd: return p
            if c is None: return None

def cc_thread(port, stop, state):
    """thin CC: arm (edge + YAW_HI nav bypass) -> upload WPs -> NAV WP. Sends RC at ~30 Hz."""
    m=MSP("127.0.0.1",port); t0=time.time(); cal_done=-1; up=False; armed_at=-1
    while not stop.is_set():
        ts=time.time()-t0
        st=m.req(0x2000); af=struct.unpack("<I",st[9:13])[0] if st and len(st)>=13 else 0
        armed=bool(af&0x4); state['af']=af; state['armed']=armed
        rc=[1500,1500,1000,1500,1000,1000,1000,1500]        # roll pitch thr yaw AUX1..4
        if cal_done<0 and not (af&(1<<9)) and ts>0.5: cal_done=ts
        if cal_done<0: pass
        elif not armed:
            if (ts-cal_done)%3.0>=1.5: rc[4]=2000; rc[3]=2000  # ARM + YAW_HI (bypass NAV_UNSAFE)
        else:
            if armed_at<0: armed_at=ts
            if not up:
                for i,(a,b,al) in enumerate(WPS):
                    w=bytearray(21); w[0]=i+1; w[1]=1
                    w[2:6]=struct.pack("<i",int(a*1e7)); w[6:10]=struct.pack("<i",int(b*1e7))
                    w[10:14]=struct.pack("<i",int(al*100)); w[20]=0xa5 if i==len(WPS)-1 else 0
                    m.send(209,bytes(w))
                up=True; state['uploaded']=True
            if ts-armed_at < CLIMB_S:                       # wings-level ANGLE climb-out first: a light
                rc[4]=2000; rc[5]=2000; rc[2]=2000; rc[1]=1650  # airframe banked into the ground if NAV WP
            else:                                           # turns it to WP1 at 2 m AGL (crash). Climb, then:
                rc[4]=2000; rc[7]=2000                      # ARM + NAV WP -> iNav flies natively
        m.send(200,struct.pack("<8H",*rc))
        time.sleep(0.033)

FLT=None
def latest_flt():
    r=subprocess.run("podman logs --tail 40 fb-aircraft 2>&1",shell=True,capture_output=True,text=True)
    for ln in reversed(r.stdout.splitlines()):
        if ln.startswith("[flt]"):
            try:
                pos=ln.split()[2]; lat,lon=map(float,pos.split(","))
                agl=float(ln.split("alt")[1].split("g")[0])
                nan="nan" in ln
                wp=int(ln.split("wp=")[1].split()[0])
                return lat,lon,agl,wp,nan
            except: return None
    return None

def dist(a,b,c,d): return math.hypot((c-a)*111320,(d-b)*111320*math.cos(math.radians(a)))

def main():
    subprocess.run("podman rm -f fb-aircraft",shell=True,capture_output=True)
    mounts=""
    if os.environ.get("MOUNT_EEPROM"): mounts+=f' -v {os.environ["MOUNT_EEPROM"]}:/app/models/{AC}/eeprom.bin'
    if os.environ.get("MOUNT_MOTOR"):  mounts+=f' -v {os.environ["MOUNT_MOTOR"]}:/app/models/{AC}/engine/{AC}_motor.xml:ro'
    subprocess.run(f"podman run -d --name fb-aircraft --network flightboxnet -p 5761:5761 {mounts} "
       f"-e AIRCRAFT={AC} -e TILES_ADDR=fb-tiles:8081 -e WX_LIVE=0 -e WIND_SPEED=0 -e TURB=0 "
       f"-e FB_TIME_SCALE=1 -e FLT_LOG_S=3 -e ORIGIN_LAT={ORIG[0]} -e ORIGIN_LON={ORIG[1]} fb-aircraft",
       shell=True,capture_output=True)
    time.sleep(7)
    stop=threading.Event(); state={}
    th=threading.Thread(target=cc_thread,args=(5761,stop,state),daemon=True); th.start()

    armed=took=crashed=False; hit=[False]*len(WPS); peak=0; t0=time.time(); lastwp=1
    while time.time()-t0<SECS:
        time.sleep(3)
        s=latest_flt()
        if not s: continue
        lat,lon,agl,wp,nan=s
        if state.get('armed'): armed=True
        if agl>TAKEOFF_AGL: took=True
        peak=max(peak,agl)
        if nan and took: crashed=True; break
        for i,w in enumerate(WPS):
            if not hit[i] and dist(lat,lon,w[0],w[1])<CAP_R: hit[i]=True
        if wp>lastwp: lastwp=wp; print(f"  [t={time.time()-t0:.0f}] iNav advanced to WP{wp}  pos={lat:.4f},{lon:.4f} agl={agl:.0f} 2Dhit={sum(hit)}/{len(WPS)}")
        if all(hit): break
    stop.set()
    print(f"\n== {AC} == ARM={'OK' if armed else 'FAIL'}  TAKEOFF={'OK' if took else 'FAIL(peak %.0f)'%peak}"
          f"  WAYPOINT={sum(hit)}/{len(WPS)}{'  CRASH' if crashed else ''}  iNav_active_wp={lastwp}")
    subprocess.run("podman stop -t 2 fb-aircraft",shell=True,capture_output=True)

main()
