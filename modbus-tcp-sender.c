#include "myModbus.h"
/* Tests based on PI-MBUS-300 documentation */
int modbus_sem_id =0;                     //信号量ID（ModBus互斥访问） 
int16_t uint16toint16(uint16_t t){

    int16_t a;
      
    if( t & 0x8000 == 0){
        return t;
    }else{
        a=-t;

    }
    return -1*a;
}

uint32_t gettime_ms(void)
{
    struct timeval tv;
    gettimeofday (&tv, NULL);

    return (uint32_t) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int main(int argc, char *argv[])
{
    modbus_t * mb=NULL;
    slaverModuleInformatin * slaverModule=NULL;
    slaverModule=newSlaverModule();
    slaverModule->ModNo=1;
    mb=newModBus_TCP_Client("192.168.0.149");
    struct timeval t_start,t_end;
    uint32_t start ,end;
    gettimeofday(&t_start,NULL);  
    start=gettime_ms();
    if(mb!=NULL) getSlaverModuleInformation(mb,slaverModule);
    gettimeofday(&t_end,NULL);  
    end=gettime_ms();
    printf("远程ModBus-TCP访问总耗时:%ld ms\n",end-start);
    printf("远程ModBus-TCP访问总耗时:%ld us\n",t_end.tv_usec - t_start.tv_usec);
    printf("远程ModBus-TCP读到值A:%f 读到值B:%f\n ",(float)(uint16toint16(slaverModule->detail.powerValueA)/100.0),(float)(uint16toint16(slaverModule->detail.powerValueB)/100.0));
    freeModbus_TCP_Client(mb);                          
    return 0;
}
