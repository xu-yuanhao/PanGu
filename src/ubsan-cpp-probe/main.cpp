extern "C" __attribute__((noinline)) long long pangu_sanitizer_other(void){return 7;}
#ifdef __cplusplus
#define CABI extern "C"
#define ALIGN8 alignas(8)
#else
#define CABI
#define ALIGN8 _Alignas(8)
#endif
ALIGN8 static unsigned char buffer[16];
CABI int pangu_sanitizer_probe(unsigned long long argc){
 if(argc==1)return 0;
 if(argc==2){volatile int value=2147483647;return value+1;}
 if(argc==3){volatile unsigned int count=32;return (int)(1U<<count);}
 if(argc==4){int a[2]={1,2};volatile unsigned int index=2;return a[index];}
 if(argc==5){volatile unsigned int offset=1;int *p=(int *)(buffer+offset);return *p;}
 if(argc==6){volatile unsigned long long address=0;int *p=(int *)address;return *p;}
 if(argc==7){long long (*source)(void)=pangu_sanitizer_other;int (*volatile fn)(void)=nullptr;auto *out=reinterpret_cast<volatile unsigned char *>(&fn);auto *in=reinterpret_cast<const unsigned char *>(&source);for(unsigned long i=0;i<sizeof(fn);i++)out[i]=in[i];return fn();}
 return 99;
}
