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
 
 return 99;
}
