
#include "myModbus.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
/*全局变量*/
 
int modbus_sem_id =0;                     //信号量ID（ModBus互斥访问） 

static modbus_mapping_t *modbus_mapping;  

static modbus_t *modbus_tcp = NULL;

static netInfor serverInfor;

uint32_t gettime_ms(void)
{
    struct timeval tv;
    gettimeofday (&tv, NULL);

    return (uint32_t) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int16_t uint16toint16(uint16_t t){

    int16_t a;
      
    if( t & 0x8000 == 0){
        return t;
    }else{
        a=-t;

    }
    return -1*a;
}

static void close_sigint(int dummy)
{
    if (serverInfor.server_socket != -1) {
	close(serverInfor.server_socket);
    }
    modbus_free(modbus_tcp);
    modbus_mapping_free(modbus_mapping);
    //delLinkPV();
    printf("exit normally!\n");
    exit(dummy);
}


int modbus_server(void)
{
   // modbus_t *modbus_tcp = NULL;
    modbus_t *modbus_rtu =NULL;
    int8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int master_socket,server_socket;
    uint32_t start;
    uint32_t end;
    uint32_t elapsed;
    fd_set rdset;
    fd_set refset;
    int    fdmax,rc,j;
    int    RegisterNumber,WriteAddress,ReadAddress;
    uint16_t ConnectPos;
    struct timeval t_start,t_end;
    slaverModuleInformatin  *moduleState=NULL;
    modbus_tcp = modbus_new_tcp("0.0.0.0", 1502);
    modbus_mapping = modbus_mapping_new(MODBUS_MAX_READ_BITS, 0,
                                    MODBUS_MAX_READ_REGISTERS, 0);
    if (modbus_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n",
                modbus_strerror(errno));
        modbus_free(modbus_tcp);
        return -1;
    }

    modbus_set_debug(modbus_tcp, TRUE);
    serverInfor.server_socket = modbus_tcp_listen(modbus_tcp, NB_CONNECTION);
    if (serverInfor.server_socket == -1) {
        fprintf(stderr, "Unable to listen TCP connection\n");
        modbus_free(modbus_tcp);
        return -1;
    }
    FD_ZERO(&refset);
    FD_SET(serverInfor.server_socket, &refset);
    fdmax = serverInfor.server_socket;
    for(;;){
        rdset = refset;
        if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) {
            perror("Server select() failure.");
            close_sigint(1);
        }
        for (master_socket = 0; master_socket <= fdmax; master_socket++) {
            if(!FD_ISSET(master_socket, &rdset)) {
                continue;
            }
            if(master_socket ==serverInfor.server_socket) {                
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;
                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                newfd = accept(serverInfor.server_socket, (struct sockaddr *)&clientaddr, &addrlen);
                if (newfd == -1) {
                    perror("Server accept() error");
                } else {
                    FD_SET(newfd, &refset);
                    if (newfd > fdmax) {
                        fdmax = newfd;
                    }
                    printf("New connection from %s:%d on socket %d\n",inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                }
            }else{
                    modbus_set_socket(modbus_tcp, master_socket);                
                    rc = modbus_receive(modbus_tcp, query);
                    if (rc > 0) {

                    if(query[MBAP_LENGTH] == _FC_WRITE_MULTIPLE_REGISTERS){    //0x10
                       RegisterNumber    = (query[MBAP_LENGTH + NB_HIGH] << 8)     + query[MBAP_LENGTH + NB_LOW];
                       WriteAddress      = (query[MBAP_LENGTH + OFFSET_HIGH] << 8) + query[MBAP_LENGTH + OFFSET_LOW];   
                       modbus_reply(modbus_tcp, query, rc, modbus_mapping);                                                                                   
                       switch(WriteAddress){

                            case DO_SLAVER_SWITCH_ADDRESS:{
                                     printf("Modbus TCP 收到ModBus-RTU写操作指令\n"); 

                                     moduleState=newSlaverModule();
                                     moduleState->ModNo=modbus_mapping->tab_registers[WriteAddress+MODULE_NUMBER];
                                     ReadAddress = moduleState->ModNo*0x0010;      
                       
                                     memcpy(moduleState,modbus_mapping->tab_registers+ReadAddress,sizeof(uint16_t)*9);
                                     if(moduleState->detail.useFlag==ON_autoPROTECT || moduleState->detail.useFlag==ON_onlyGROUP){   
                                     	moduleState->detail.SwitchPosA = modbus_mapping->tab_registers[WriteAddress+DO_SLAVER_SWITCH_Pos];
                                     	moduleState->detail.SwitchPosB = modbus_mapping->tab_registers[WriteAddress+DO_SLAVER_SWITCH_Pos];
                                     	memcpy(modbus_mapping->tab_registers+ReadAddress,moduleState,sizeof(uint16_t)*9);   
                                     }    

                                     printf("ModBus-RTU主站向 ModBus-RTU从站发送写操作指令 \n"); 
				     if(!setModbus_P())                                              
					 exit(EXIT_FAILURE);    
				      modbus_rtu =newModbus(MODBUS_DEV,MODBUS_BUAD);
				      doOpticalProtectSwitch(modbus_rtu,(moduleState->ModNo-1)*4+SW_A,moduleState->detail.SwitchPosA,MODE5_PROTECT_SLAVER);
		                      doOpticalProtectSwitch(modbus_rtu,(moduleState->ModNo-1)*4+SW_B,moduleState->detail.SwitchPosB,MODE5_PROTECT_SLAVER);
				      freeModbus(modbus_rtu);
                                      modbus_rtu=NULL;            
			              if(!setModbus_V())                                                              
				          exit(EXIT_FAILURE);

                                     printf("当前状态:%d\n",moduleState->ModNo);
                                     for(j=0;j<9;j++){
                                         printf("[%d]=%d\n",j+ReadAddress,uint16toint16(modbus_mapping->tab_registers[j+ReadAddress]));
                                     }
                                     freeSlaverModule(moduleState);
                                     moduleState=NULL;
                                 }break;
                            case MODULE_1_INFORMATION_ADDRESS+MODULE_INFORMATION_MasterCM:                                
                            case MODULE_2_INFORMATION_ADDRESS+MODULE_INFORMATION_MasterCM:                                 
                            case MODULE_3_INFORMATION_ADDRESS+MODULE_INFORMATION_MasterCM:                                 
                            case MODULE_4_INFORMATION_ADDRESS+MODULE_INFORMATION_MasterCM:
                            case MODULE_5_INFORMATION_ADDRESS+MODULE_INFORMATION_MasterCM:
                            case MODULE_6_INFORMATION_ADDRESS+MODULE_INFORMATION_MasterCM:
                            case MODULE_7_INFORMATION_ADDRESS+MODULE_INFORMATION_MasterCM:
                            case MODULE_8_INFORMATION_ADDRESS+MODULE_INFORMATION_MasterCM:{
                                     printf("创建保护从模块_%x    mb_mapping[%d]!\n",WriteAddress/0x0010,WriteAddress);
                                     moduleState=newSlaverModule();
                                     moduleState->ModNo=WriteAddress/0x0010;
                                     ReadAddress = moduleState->ModNo*0x0010;
                                     //setLink_P();
                                     memcpy(moduleState,modbus_mapping->tab_registers+ReadAddress,sizeof(uint16_t)*9);
                                     moduleState->detail.useFlag    = OFF_PROTECT;
                                     memcpy(modbus_mapping->tab_registers+ReadAddress,moduleState,sizeof(uint16_t)*9);

                                     printf("当前状态:%d\n",moduleState->ModNo);
                                     for(j=0;j<9;j++){
                                         printf("[%d]=%d\n",j+ReadAddress,uint16toint16(modbus_mapping->tab_registers[j+ReadAddress]));
                                     }
                                     freeSlaverModule(moduleState);
                                     moduleState=NULL;
                                 }break;
                            default: printf("该地址不支持写操作   mb_mapping[%d]!\n",WriteAddress);
                       }
 
                    }else if(query[MBAP_LENGTH] == _FC_READ_HOLDING_REGISTERS){//0x03
                             RegisterNumber    = (query[MBAP_LENGTH + NB_HIGH] << 8)     + query[MBAP_LENGTH + NB_LOW];
                             ReadAddress       = (query[MBAP_LENGTH + OFFSET_HIGH] << 8) + query[MBAP_LENGTH + OFFSET_LOW]; 
				     //获取光功率    
                             printf("Modbus-TCP从站收到远端ModBus-TCP主站读操作指令\n");                     
                             float powerValue[2];
                             int16_t powerValueInt[2];
                             uint16_t ModuleNo = ReadAddress/0x00010;
		             uint16_t SNoA= (ModuleNo-1)*8+PD_A; 
		             uint16_t SNoB= (ModuleNo-1)*8+PD_B; 
                             start = gettime_ms();

		             if(!setModbus_P())                                              
			         exit(EXIT_FAILURE); 
                             printf("现场ModBus-RTU主站向现场ModBus-RTU从站发送读操作指令 \n");  
                             gettimeofday(&t_start,NULL);   
		             modbus_rtu =newModbus(MODBUS_DEV,MODBUS_BUAD);
		             powerValue[0] =getOneOpticalValue(modbus_rtu,SNoA,MODE5_PROTECT_SLAVER);   
                             powerValue[1] =getOneOpticalValue(modbus_rtu,SNoB,MODE5_PROTECT_SLAVER);  
		             freeModbus(modbus_rtu);
                             modbus_rtu=NULL;            
			     if(!setModbus_V())                                                              
				 exit(EXIT_FAILURE);
                             powerValueInt[0] = (int16_t)(powerValue[0]*100);
                             powerValueInt[1] = (int16_t)(powerValue[1]*100);
			     end = gettime_ms();

                             gettimeofday(&t_end,NULL);     
   
                             memcpy(modbus_mapping->tab_registers+ReadAddress+MODULE_INFORMATION_PowerValueA,powerValueInt,sizeof(uint16_t)*2);
                             printf("现场ModBus-RTU读耗时:%ld ms\n",(t_end.tv_usec-t_start.tv_usec)/1000);

                             printf("现场ModBus-RTU读到值A:%f  读到值B:%f\n", (float)(uint16toint16(powerValueInt[0])/100.0),(float)(uint16toint16(powerValueInt[0])/100.0));
                             
                             printf("向远端ModBus-TCP主站回复数据帧(读到值A,B值封装在帧中) \n");
                             modbus_reply(modbus_tcp, query, rc, modbus_mapping);

                             
                    //}else if(query[MBAP_LENGTH] == _FC_WRITE_SINGLE_REGISTER){//0x06
                    }else{
                         printf("Don't support funtion!\n");
                    }

                }else if (rc == -1) {

                    printf("Connection closed on socket %d\n", master_socket);
                    close(master_socket);

                    FD_CLR(master_socket, &refset);

                    if (master_socket == fdmax) {
                        fdmax--;
                    }
                }
            }
        }
    }

    return 0;
}



#include <signal.h>  
#include <execinfo.h>  
#define SIZE 1000  
void *buffer[SIZE];  
void fault_trap(int n,struct siginfo *siginfo,void *myact)  
{  
        int i, num;  
        char **calls;  
        char buf[1024];
        char cmd[1024];
        FILE *fh;

        printf("Fault address:%X\n",siginfo->si_addr);     
        num = backtrace(buffer, SIZE);  
        calls = backtrace_symbols(buffer, num);  
        for (i = 0; i < num; i++)  
                printf("%s\n", calls[i]);  
        exit(0);
}  
void setuptrap()  
{  
        struct sigaction act;  
        sigemptyset(&act.sa_mask);     
        act.sa_flags=SA_SIGINFO;      
        act.sa_sigaction=fault_trap;  
        sigaction(SIGSEGV,&act,NULL);  
}

int main()  
{  
    pthread_t tha,thb;  
    void *retval;  
        modbus_sem_id = semget((key_t)5678, 1, (4777 | IPC_CREAT));                        //每一个需要用到信号量的进程,在第一次启动的时候需要初始化信号量 
        if(!setModbusPV())                                                                  //程序第一次被调用，初始化信号量
        {  
              printf("Failed to initialize modbus_semaphore\n");  
              exit(EXIT_FAILURE); 
        }                  
    modbus_sem_id = semget((key_t)5678, 1, 4777 | IPC_CREAT);     
    modbus_server();
    return 0;  
  
}



