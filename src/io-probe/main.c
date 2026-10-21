#include "src/bootstrap-api/io.h"
int io_test(const pangu_io_api *a)
{
 pangu_i32 fd=-1, initial_fd=-1, invalid_fd=2147483647; pangu_u64 n=99, before=0, after=0; char buf[16];
 if(a->abi_version!=1 || a->struct_size!=64 || a->reserved) return 1;
 if(a->open(a->context,(void *)0,0,&fd)!=1 || fd!=-1) return 2;
 if(a->open(a->context,"",0,&fd)!=1) return 3;
 if(a->open(a->context,"input",2,&fd)!=1) return 4;
 if(a->open(a->context,"missing",0,&fd)!=3 || fd!=-1) return 5;
 if(a->open(a->context,"input",1,&fd)!=4 || fd!=-1) return 6;
 if(a->open(a->context,"input",0,&fd)!=0) return 30;
 initial_fd=fd;
 if(a->close(a->context,&fd)!=0) return 31;
 if(a->close(a->context,&invalid_fd)!=8 || invalid_fd!=-1) return 32;
 if(a->open(a->context,"link",0,&fd)!=8 || fd!=-1) return 7;
 if(a->open(a->context,"directory",0,&fd)!=9 || fd!=-1) return 8;
 if(a->open(a->context,"fifo",0,&fd)!=9 || fd!=-1) return 9;
 if(a->read(a->context,-1,buf,1,&n)!=1 || n!=0) return 10;
 if(a->close(a->context,&fd)!=1) return 11;
 if(a->open(a->context,"input",0,&fd)!=0 || fd<0) return 12;
 if(fd!=initial_fd) return 33;
 if(a->read(a->context,fd,(void *)0,1,&n)!=1 || n!=0) return 34;
 if(a->read(a->context,fd,buf,0x80000000ULL,&n)!=1 || n!=0) return 35;
 if(a->read(a->context,fd,buf,1,(void *)0)!=1) return 36;

 if(a->open(a->context,"input",0,&fd)!=5) return 13;
 if(a->read(a->context,fd,(void *)0,0,&n)!=0 || n!=0) return 14;
 if(a->read(a->context,fd,buf,16,&n)!=0 || n!=5) return 15;
 if(buf[0]!='h' || buf[4]!='o') return 16;
 if(a->read(a->context,fd,buf,16,&n)!=0 || n!=0) return 17;
 if(a->write(a->context,fd,"x",1,&n)!=8 || n!=0) return 18;
 if(a->close(a->context,&fd)!=0 || fd!=-1) return 19;
 if(a->close(a->context,&fd)!=1) return 20;
 if(a->open(a->context,"created",1,&fd)!=0) return 21;
 if(a->write(a->context,fd,"ok",2,&n)!=0 || n!=2) return 22;
 if(a->close(a->context,&fd)!=0 || fd!=-1) return 23;
 if(a->open(a->context,"created",0,&fd)!=0) return 24;
 if(a->read(a->context,fd,buf,16,&n)!=0 || n!=2 || buf[0]!='o' || buf[1]!='k') return 25;
 if(a->close(a->context,&fd)!=0) return 26;
 if(a->monotonic_ns(a->context,(void *)0)!=1) return 27;
 if(a->monotonic_ns(a->context,&before)!=0) return 28;
 for(pangu_u32 i=0;i<1000;i++) {
  if(a->monotonic_ns(a->context,&after)!=0 || after<before) return 29;
  before=after;
 }
 return 0;
}
