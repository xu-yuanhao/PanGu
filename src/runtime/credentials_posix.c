#define _GNU_SOURCE
#include "credentials.h"
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
static int parent_directory(char *path){
 for(char *p=path+1;*p;p++)if(*p=='/'){*p=0;int result=mkdir(path,0700);*p='/';if(result&&errno!=EEXIST)return 7;}
 return 0;
}
static int write_bytes(int fd,const char *data,size_t size){
 while(size){ssize_t n=write(fd,data,size);if(n<0&&errno==EINTR)continue;if(n<=0)return 7;data+=n;size-=(size_t)n;}return 0;
}
int pg_credentials_file_access(const char *path,int writing,int (*visit)(void *,const char *,size_t,char **),void *context){
 int result=7,lock=-1,input=-1,output=-1,parent=-1;char *lockpath=0,*temporary=0,*data=0,*next=0,*directory=0;size_t size=0,capacity=4096;
 if(!path||!*path||!visit)return 5;
 directory=strdup(path);if(!directory)goto done;
 if(writing&&parent_directory(directory))goto done;
 if(asprintf(&lockpath,"%s.lock",path)<0){lockpath=0;goto done;}
 lock=open(lockpath,O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
 if(lock<0){if(!writing&&(errno==ENOENT||errno==ENOTDIR)){result=visit(context,"{}",2,&next);}goto done;}
 int acquired=0;for(unsigned attempt=0;attempt<10;attempt++){if(!flock(lock,(writing?LOCK_EX:LOCK_SH)|LOCK_NB)){acquired=1;break;}if(errno!=EWOULDBLOCK&&errno!=EAGAIN&&errno!=EINTR)goto done;if(attempt<9){struct timespec pause={0,20000000};while(nanosleep(&pause,&pause)&&errno==EINTR){}}}if(!acquired)goto done;
 input=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
 if(input<0){if(errno!=ENOENT)goto done;result=visit(context,"{}",2,&next);}
 else{
  data=malloc(capacity);if(!data)goto done;
  for(;;){if(size==capacity-1){if(capacity>SIZE_MAX/2)goto done;capacity*=2;char *grown=realloc(data,capacity);if(!grown)goto done;data=grown;}ssize_t n=read(input,data+size,capacity-size-1);if(n<0&&errno==EINTR)continue;if(n<0)goto done;if(!n)break;size+=(size_t)n;}
  data[size]=0;result=visit(context,data,size,&next);
 }
 if(result||!next)goto done;
 if(!writing){result=5;goto done;}
 result=7;
 if(asprintf(&temporary,"%s.XXXXXX",path)<0){temporary=0;goto done;}
 output=mkostemp(temporary,O_CLOEXEC);if(output<0||fchmod(output,0600)||write_bytes(output,next,strlen(next))||fsync(output))goto done;
 if(close(output)){output=-1;goto done;}output=-1;
 char *slash=strrchr(directory,'/');if(slash){if(slash==directory)slash[1]=0;else *slash=0;}else strcpy(directory,".");
 parent=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(parent<0||rename(temporary,path)||fsync(parent))goto done;
 result=0;
done:
 if(output>=0)close(output);
 if(temporary)unlink(temporary);
 if(input>=0)close(input);
 if(parent>=0)close(parent);
 if(lock>=0)close(lock);
 free(directory);free(lockpath);free(temporary);free(data);free(next);return result;
}
