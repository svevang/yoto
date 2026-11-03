/* 48-byte Ultralight / MF0UL11 NDEF formatter/content copier; built against libnfc. */
#include <nfc/nfc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static nfc_context *ctx;
static nfc_device *dev;
static size_t dump_size;
static const char *card_type;
static void cleanup(void) { if(dev) nfc_close(dev); if(ctx) nfc_exit(ctx); }
static void die(const char *s) { fprintf(stderr,"Error: %s\n",s); exit(1); }
static void check(int r,const char *s) { if(r<0) die(s); }
static int exchange(const unsigned char *tx,size_t len,unsigned char *rx,size_t cap) {
 int n=nfc_initiator_transceive_bytes(dev,tx,len,rx,cap,1000);
 if(n<0) {
  fprintf(stderr,"NFC command %02x failed: %s (%d)\n",tx[0],nfc_strerror(dev),n);
  die("Reopen and read the card before retrying");
 }
 return n;
}
static void connect_card(void) {
 dev=nfc_open(ctx,"acr122_usb");if(!dev)die("Cannot open ACR122U; close other NFC apps or reconnect it");
 check(nfc_initiator_init(dev),"Reader initialization failed");
 check(nfc_device_set_property_bool(dev,NP_INFINITE_SELECT,false),"Cannot set finite polling");
 nfc_modulation m={NMT_ISO14443A,NBR_106};nfc_target t;
 if(nfc_initiator_select_passive_target(dev,m,NULL,0,&t)<=0)die("No card found");
 if(t.nti.nai.szUidLen!=7 || t.nti.nai.btSak!=0 || t.nti.nai.abtAtqa[1]!=0x44)
  die("Unsupported card family");
 /* Original Ultralight wraps READ 14/15 at page 16. Larger chips do not.
    Check two overlapping responses against manufacturer/UID pages before
    attempting GET_VERSION, which original Ultralight does not implement. */
 unsigned char head[16],end14[16],end15[16];
 unsigned char rd0[]={0x30,0},rd14[]={0x30,14},rd15[]={0x30,15};
 if(exchange(rd0,2,head,16)!=16 || exchange(rd14,2,end14,16)!=16 || exchange(rd15,2,end15,16)!=16)
  die("Incomplete memory-layout probe");
 if(!memcmp(end14+8,head,8) && !memcmp(end15+4,head,12)
    && !memcmp(end14+4,end15,4)) {
  dump_size=64;card_type="Ultralight 48-byte layout (legacy-compatible)";return;
 }
 unsigned char cmd=0x60,version[16];
 const unsigned char supported[]={0,4,3,1,1,0,0x0b,3};
 check(nfc_device_set_property_bool(dev,NP_EASY_FRAMING,false),"Cannot set raw framing");
 int n=nfc_initiator_transceive_bytes(dev,&cmd,1,version,sizeof version,1000);
 check(nfc_device_set_property_bool(dev,NP_EASY_FRAMING,true),"Cannot restore framing");
 if(n!=8 || memcmp(version,supported,8))die("Unsupported layout/version: neither legacy 48-byte Ultralight nor verified MF0UL11 EV1");
 dump_size=80;card_type="MF0UL11 EV1 (48-byte user memory)";
}
static void read_card(unsigned char out[80]) {
 for(size_t p=0;p<dump_size/4;p+=4) {
  unsigned char cmd[]={0x30,(unsigned char)p};
  if(exchange(cmd,2,out+4*p,16)!=16)die("Incomplete card read");
 }
}
static void uid_string(const unsigned char *data,char uid[15]) {
 snprintf(uid,15,"%02x%02x%02x%02x%02x%02x%02x",data[0],data[1],data[2],data[4],data[5],data[6],data[7]);
}
static void save_new(const char *path,const unsigned char data[80]) {
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
 if(fd<0)die("Backup path must be writable and must not already exist");
 FILE *f=fdopen(fd,"wb");if(!f){close(fd);die("Cannot open backup stream");}
 if(fwrite(data,1,dump_size,f)!=dump_size)die("Cannot write complete backup");
 if(fclose(f))die("Cannot finish backup");
}
static void write_page(int page,const unsigned char *data) {
 unsigned char cmd[6]={0xa2,(unsigned char)page},ack[16];memcpy(cmd+2,data,4);
 int n=exchange(cmd,6,ack,sizeof ack);
 if(n!=0 && (n!=1 || (ack[0]&15)!=10))die("Write not acknowledged; re-read before retrying");
}
int main(int argc,char **argv) {
 int format=argc==4 && !strcmp(argv[1],"format");
 int copy=argc==5 && !strcmp(argv[1],"write");
 int inspect=argc==2 && !strcmp(argv[1],"inspect");
 if(!format && !copy && !inspect) {
  fprintf(stderr,"Usage:\n  %s inspect\n  %s format TARGET_UID NEW_BACKUP.mfd\n  %s write TARGET_UID SOURCE.mfd NEW_BACKUP.mfd\n",argv[0],argv[0],argv[0]);return 2;
 }
 unsigned char source[80],before[80],after[80],expected[80];
 const unsigned char cc[]={0xe1,0x10,6,0};
 if(copy) {
  FILE *f=fopen(argv[3],"rb");if(!f)die("Cannot open source dump");
  size_t len=fread(source,1,80,f);
  if((len!=64 && len!=80) || fgetc(f)!=EOF)die("Source must be a 64-byte Ultralight or 80-byte MF0UL11 dump");
  fclose(f);
  if(memcmp(source+12,cc,4))die("Source must have a writable 48-byte NDEF header E1 10 06 00");
 }
 atexit(cleanup);nfc_init(&ctx);if(!ctx)die("Cannot initialize libnfc");
 connect_card();read_card(before);char uid[15];uid_string(before,uid);
 printf("%s\nUID: %s\nDump size: %zu bytes; user content: 48 bytes\nCC: %02x %02x %02x %02x\n",card_type,uid,dump_size,before[12],before[13],before[14],before[15]);
 size_t original_size=dump_size;
 if(inspect)return 0;
 if(strcmp(uid,argv[2]))die("UID mismatch: refusing to change a different card; use lowercase UID without spaces");
 if(before[10] || before[11])die("Card has static lock bits set");
 if(dump_size==80 && (before[67]!=0xff || (before[68]&0x40)))die("Card has protection settings; this helper handles unprotected cards only");
 memcpy(expected,before,dump_size);
 if(format) {
  const unsigned char blank[]={0,0,0,0};
  if(memcmp(before+12,blank,4) && memcmp(before+12,cc,4))die("Existing OTP/CC is incompatible; refusing to change it");
  memcpy(expected+12,cc,4);memset(expected+16,0,48);
  expected[16]=3;expected[17]=0;expected[18]=0xfe;
 } else {
  if(memcmp(before+12,cc,4))die("Format the target card first");
  memcpy(expected+16,source+16,48);
 }
 const char *backup=argv[format?3:4];save_new(backup,before);
 printf("Backup: %s\n",backup);fflush(stdout);
 /* Only user pages 4..15, then optional capability container page 3. */
 for(int p=4;p<16;p++)if(memcmp(before+4*p,expected+4*p,4))write_page(p,expected+4*p);
 if(format && memcmp(before+12,expected+12,4))write_page(3,expected+12);
 /* Reopen instead of relying on immediate deselect/reselect on ACR122U. */
 nfc_close(dev);dev=NULL;usleep(250000);connect_card();
 if(dump_size!=original_size)die("Card layout changed during verification");
 read_card(after);
 if(memcmp(after,expected,dump_size))die("Verification mismatch: inspect card and backup before retrying");
 printf("Verified all %zu readable bytes. UID, lock bytes and configuration preserved.\n",dump_size);
 return 0;
}
