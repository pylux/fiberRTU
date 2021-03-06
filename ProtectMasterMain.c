#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"
#include "defaulttest.h"
#include "process.h"
#include "sql.h"
#include "otdr.h"
#include "uploadCycTestData.h" 
#include "myModbus.h"


/***测试链表节点结构***/ //LinkA
/*
    CM :局站号
    ANo:告警级别（优先级）
    PNo:告警主序号
    SNoRecvOnline:光路号
    SNoRecvBackup:
    PowerGateOnline:
    PowerGateBackup:
    masterSwitchPos:

    SNoSendOnline: 
    SNoSendBackup:
    slaverSwitchPos:
    slaverIP:
    slaverModuleNo:
    ConnectPos:
    

    fristAlarmFlag:首次告警标识：首次产生告警，置位为1，告警消失：置位为0。
    nextAlarmTime :告警未消失时，下次向服务器上传告警信息的时间
    alarmClick    :上传告警的周期
*/

typedef struct checkProtectNode checkProtectNode;
struct checkProtectNode{
        int    CM;
	int    ANo;
        int    PNo;
        int    SNoRecvOnline;
        int    SNoRecvBackup;           
        float  PowerGateOnline;
        float  PowerGateBackup;
        int    masterSwitchPos;

        int    SNoSendOnline;
        int    SNoSendBackup;

        int    slaverSwitchPos;
        char   slaverIP[16];
        int    slaverModuleNo;
        int    ConnectPos;

        int    fristAlarmFlag;
        time_t nextAlarmTime;
        time_t alarmClick;

	struct checkProtectNode *next;
};

/***告警链表节点***/   //LinkB
/*
    SNO:光路号
    ANo:告警组号（优先级）
    Order:节点序列号，按优先级排序依据 Order = ANo*100 + SNo
*/
typedef struct alarmNode alarmNode;
struct alarmNode{
	int    SNo;
        int    CM;
	int    ANo;
        int    Order;
	struct alarmNode *next;
};


/*全局变量*/
int sem_id=0;                      //信号量ID（数据库互斥访问）      
int modbus_sem_id=0;               //信号量ID（ModBus互斥访问）  
int otdr_sem_id=0;                 //信号量ID（OTDR互斥访问）                    

checkProtectNode *linkHead_check_A;//测试节点链表头
alarmNode *linkHead_alarm_B;       //异常节点链表头
int num_A =0;                      //测试链表节点数
int num_B =0;                      //异常链表节点数
int flagNew=0;                     //节点更新/插入标志

static int   sigLock=0;
static queue *myQueue=NULL;
static pthread_mutex_t mutex;
/*****************************LinkA-checkLink************************************/
/***插入测试节点****/
/*
     (1) 尾插法.uint32tostring
*/
checkProtectNode *insert_protect_node(checkProtectNode *head,checkProtectNode *newNode)
{
        checkProtectNode *current=NULL;
        checkProtectNode *pre=NULL;

	current = head;
        while(current!=NULL){
            pre = current;
            current = current->next;
        }
        newNode->next = current;
        if(pre==NULL){  
            head = newNode; 
        }else{
            pre->next = newNode;
        }
        num_A++;
        return (head);
}
/***创建新链表***/
/*


*/
checkProtectNode *creat_protect_link(){
	checkProtectNode *head=NULL,*p1=NULL;
        head = (checkProtectNode *) malloc (sizeof(checkProtectNode ));
        p1 =   (checkProtectNode *) malloc (sizeof(checkProtectNode ));
        head=NULL;
        p1->CM             =0;
        p1->ANo            =0;
        p1->PNo            =0;
        p1->SNoRecvOnline  =0;
        p1->SNoRecvBackup  =0;
        p1->PowerGateOnline=0;
        p1->PowerGateBackup=0;
        p1->masterSwitchPos=0;

        p1->SNoSendOnline  =0;
        p1->SNoSendBackup  =0;

        p1->slaverSwitchPos=0;
        p1->slaverModuleNo =0;
        p1->ConnectPos     =0;

        p1->fristAlarmFlag =0;
        p1->nextAlarmTime  =0;
        p1->alarmClick     =0;
    
 
	head = insert_protect_node(head,p1);
	return(head);
}

/***删除节点***/
/*
   (1)以光路号SNo为索引
*/
checkProtectNode *delete_protect_node(checkProtectNode *head,int PNo){
	checkProtectNode *p1=NULL,*p2=NULL;
	if(head==NULL){
		printf("This is a void execl");
		return(head);
	}
	p1= head;
	while(p1->PNo!=PNo && p1->next !=NULL){
		p2=p1;
		p1 =p1->next;
	}
	if(p1->PNo==PNo)
	{
		if(p1==head){
			head =p1->next;
		        free(p1);
                        p1=NULL;
                 }
		else
                 {
			p2->next =p1->next;
		        free(p1);
                        p1=NULL;
                 }
		num_A--;
	}else
		printf("Sorry,the PNo you want to delete is not fount!\n");
	return(head);
}

/***查找结点***/
/*
   (1)以光路号SNo为索引
*/
checkProtectNode *find_protect_node(checkProtectNode *head,int PNo)
{
	checkProtectNode * current=NULL;
        current = head;
        while(current!=NULL){
            if(current->PNo == PNo)
                return current;
	    current = current->next;
        }
        return NULL;
}

/***遍历链表***/
void output_all_protect_node(checkProtectNode *head){
	checkProtectNode *p=NULL;
        int i=0;
	p= head;
	if(p==NULL){
		printf("Don't have node in protect test link!\n");
		return ;
	}
	else
		printf("There are %d lines on alarm testing linkA:\n",num_A);
	while(p!=NULL){
                printf("---------------------Protect Node:%d----------------\n",++i);
                printf("|         PNo:%4d      ANo:%4d      CM:%3d          |\n",p->PNo,p->CM,p->ANo);  
                printf("|SNoRecvOnline:%4d          SNoRecvBackup:%4d       |\n",p->SNoRecvOnline,p->SNoRecvBackup); 
                printf("|PowerGateOnline:%2.2f      PowerGateBackup:%2.2f   |\n",p->PowerGateOnline,p->PowerGateBackup); 
                printf("|MasterSwitchPos:%4d        SlaverSwitchPos:%4d     |\n",p->masterSwitchPos,p->slaverSwitchPos); 
                printf("|SNoSendOnline:%4d          SNoSendBackup:%4d       |\n",p->SNoSendOnline,p->SNoSendBackup); 
                printf("|SlaverModuleNo:%4d         ConnectPos:%4d          |\n",p->slaverModuleNo,p->ConnectPos); 
                printf("|FristAlarmFlag:%4d         NextAlarmTime:%4d  |\n",p->fristAlarmFlag,p->nextAlarmTime);
                printf("|slaverIP:%s      alarmClick:%d           |\n",p->slaverIP,p->alarmClick);  
                printf("---------------------------------------------------\n\n",i+1);
                p=p->next;

	}
}




/***************************LinkB-alarmLink************************************/
/***插入异常有序节点****/
/*
     (1) 根据故障优先级大小排序.
*/
alarmNode *insert_B(alarmNode *head,alarmNode *newNode)
{

        alarmNode *current=NULL;
        alarmNode *pre=NULL;
	current = head;
        while(current!=NULL&&current->Order < newNode->Order){
            pre = current;
            current = current->next;
        }
        newNode->next = current;
        if(pre==NULL){  
            head = newNode; 
        }else{
            pre->next = newNode;
        }
        num_B++;
        return (head);
}

/***创建新链表***/
alarmNode *link_creat_B(){
	alarmNode *head=NULL,*p1=NULL;
        head = (alarmNode *) malloc (sizeof(alarmNode ));
        p1   = (alarmNode *) malloc (sizeof(alarmNode ));
	head =NULL;
        p1->SNo =0;
        p1->CM =0;
        p1->ANo =0;
        p1->Order =0;
	head = insert_B(head,p1);
	return(head);
}

/***判断链表是否为空***/
int isEmpty_B(alarmNode *head){
        return (head==NULL);
}

/***删除头节点***/
/*
    (1)链表非空的前提下才能删除
*/
alarmNode *deleteFirst_B(alarmNode *head ){
        if (isEmpty_B(head)){
            return NULL;
        }
        alarmNode *temp=NULL;
        temp = (alarmNode *) malloc (sizeof(alarmNode ));
        temp = head;
        head = head->next;
        num_B--;
        return temp;
    }

/***输出头节点***/
/*
    (1)链表非空情况下才能输出
*/
alarmNode * outFirstnode_B(alarmNode *head)
{
        alarmNode *p0=NULL;
	if(head==NULL){
		return(head);                               
	}
        p0 = (alarmNode *) malloc (sizeof(alarmNode ));        
        p0->SNo           = head->SNo;
        p0->CM            = head->CM;
        p0->ANo           = head->ANo;
        p0->Order         = head->Order;
	return(p0);
}

/***删除节点***/
/*
   (1)以光路号SNo为索引
*/
alarmNode *delete_B(alarmNode *head,int SNo){
	alarmNode *p1=NULL,*p2=NULL;
	if(head==NULL){
		printf("This is a void execl");
		return(head);
	}
	p1= head;
	while(p1->SNo!=SNo && p1->next !=NULL){
		p2=p1;
		p1 =p1->next;
	}
	if(p1->SNo==SNo)
	{
		if(p1==head){
			head =p1->next;
		        free(p1);
                 }
		else
                 {
			p2->next =p1->next;
		        free(p1);
                        p1=NULL;
                 }
		num_B--;
	}else
		printf("Sorry,the SNo you want delete is not fount!\n");
	return(head);
}

/***查找结点***/
/*
   (1)以光路号SNo为索引
*/
alarmNode *findNode_B(alarmNode *head,int SNo)
{
	alarmNode * current=NULL;
        current = head;
        while(current!=NULL){
            if(current->SNo == SNo)
                return current;
	     current = current->next;
        }
        return NULL;
}

/***遍历链表***/
void outPutALL_B(alarmNode *head){
	alarmNode *p=NULL;
	p= head;
	if(p==NULL)
	   return ;
        else {
          printf("There are %d lines on testing_B.\n",num_B); 
	  while(p!=NULL){
		printf("SNo:%d,rtuCM:%d,Level:%d,Order:%d\n",p->SNo,p->CM,p->ANo,p->Order);
		p=p->next;
	  }
       }
}
alarmNode * insertOTDtestNode(alarmNode *head,int SNo,int CM,int ANo){
       alarmNode * node=NULL;
       node=(alarmNode *)malloc(sizeof(alarmNode));
       node->SNo = SNo;
       node->CM  = CM;
       node->ANo = ANo;     
       node->Order = ANo*100 +SNo; 
       head=insert_B(head,node);
       return head;
}
alarmNode * deleteALL_B(alarmNode *head){
	alarmNode *p=NULL;
	union sigval mysigval;
	char* process;  
	int ret = 0,n,signum;  
        pid_t cycPID[MAX_PID_NUM];  
	int recvInt=-1; 
	p= head;
	if(p==NULL){
		return p=NULL;
	}
	else{
	    while(p!=NULL){
		           /*向otdrMain发送启动信号*/

		        printf("Send message to otdrMain! SNo=%d\n",p->SNo);                  
			ret = get_pid_by_name("/web/cgi-bin/otdrMain", cycPID, MAX_PID_NUM);  
			printf("otdrMain:process '/web/cgi-bin/otdrMain' is existed? (%d): %c ", ret, (ret > 0)?'y':'n');  
			signum=SIGRTMIN;                                     
			mysigval.sival_int = p->SNo+400;    
                        if(ret>0){  
                                             
				for(n=0;n<ret;n++){                                      
					printf("otdrMain PID:%u\n", cycPID[n]);                  
					if(sigqueue(cycPID[n],signum,mysigval)==-1)
						printf("send signal error\n");
				}  
		                
				   /*等待信号的成功处理消息*/			    
				recvInt = recvMessageQueue_OTDR("4-OK",PROTECT_MESSAGE_KEY);
				if(recvInt==0){
				      printf("ProtectMain Recv back message from otdrMain  sucessful!");
		                      p=delete_B(p,p->SNo);
				 }else{
				      printf("ProtectMain Recv back message from otdrMain  Faild:Time out!");
				 } 
                                 if(recvInt==0 && p!=NULL)  
			              p=delete_B(p,p->SNo);		                                                               
	
			}else{
                               printf("otdrMain Processing don't exsit!\n");
                               if(p!=NULL) 
                                  p=delete_B(p,p->SNo);
                        }   
	   }
           printf("\n\n");
        }    
       return p;
}


checkProtectNode * removeWaitingNode(checkProtectNode *head) 
{
	 sqlite3 *mydb;
	 char PNo[5];
	 sql *mysql;
         checkProtectNode *find;
         int rc,i=0,intPNo[100];
         find=head;
	 mysql = SQL_Create();

	 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
	 if( rc != SQLITE_OK )
	     printf("Lookup SQL error\n");

	 mysql->db = mydb;
	 mysql->tableName   = "ProtectGroupTable";	        
         while(find!=NULL){
                uint32tostring(find->PNo,PNo);
                find=find->next;
                mysql->mainKeyValue = PNo;
                if(!SQL_existIN_db(mysql)){          
                    head=delete_protect_node(head,atoi(PNo));
                }
 
	  }                     
        
	 SQL_Destory(mysql);  
	 sqlite3_close(mydb);
 
         return(head);
}

checkProtectNode * insertWaitingNode(checkProtectNode * head)
{
	 sqlite3 *mydb;
	 sql  *mysql;
	 char *zErrMsg = 0,*ModNo,*SNoOnline;
         char PNo[10],strModNo[10],SNoA[10],SNoB[10],sSNoA[10],sSNoB[10];
         int   CM=0,ANo=0,slaverSwitchPos=0,masterSwitchPos=0,slaverModuleNo=0,ConnectPos=0,SNoRecvOnline=0,SNoRecvBackup=0,SNoSendOnline=0,SNoSendBackup=0;
         float PowerGateBackup=0.0,PowerGateOnline=0.0;
         char  slaverIP[16];
	 int rc,i,ret=-1,rednum=0,PN=0;
         int fiberType=-1,ModuleType=-1,protectFlag=0;
	 char resultPNo[64][5];
         char **result = NULL;
         checkProtectNode *node=NULL,*find=NULL;      
         time_t T3,T4;     
              
	 mysql = SQL_Create();
      
	 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
      
	 if( rc != SQLITE_OK ){
	    printf( "Lookup SQL error\n");
	 }
	 mysql->db = mydb;
	 mysql->tableName   =  "ProtectGroupTable";	
         mysql->filedsValue =  "-1";                                
         mysql->filedsName  =  "Status";
   
         PN=SQL_findPNo(mysql,resultPNo);

         if(PN>0){
		for(i =0 ;i<PN;i++){
		     strcpy(PNo,resultPNo[i]);     
                     uint32tostring((atoi(PNo)-1)/4+1,strModNo);                  // 获取所属模块类型 
                     mysql->mainKeyValue  = strModNo;
                     mysql->tableName     =  "SubModuleTypeTable";
                     mysql->filedsName    =  "ModuleType";
                     SQL_lookupPar(mysql,&result,&rednum); 
                     ModuleType=atoi(result[0]); 
                     SQL_freeResult(&result,&rednum);  
    
                     if(ModuleType ==3 ){
			     mysql->tableName     =  "ProtectGroupTable";
			     mysql->mainKeyValue  =   PNo;
    
			     mysql->filedsName    =  "SwitchPos"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             masterSwitchPos=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);
      
			     mysql->filedsName    =  "sSwitchPos"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             slaverSwitchPos=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);

			     mysql->filedsName    =  "sModNo"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             slaverModuleNo=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);
		              
			     mysql->filedsName    = "ConnectPos"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     ConnectPos=atoi(result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->filedsName    = "sIP"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(slaverIP,result[0]);
			     SQL_freeResult(&result,&rednum);
 
			     mysql->filedsName    = "SNoA"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(SNoA,result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->filedsName    = "SNoB"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(SNoB,result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->filedsName    = "sSNoA"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(sSNoA,result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->filedsName    = "sSNoB"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(sSNoB,result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->tableName     =  "AlarmTestSegmentTable";   
			     mysql->filedsName    =  "fiberType"; 
                             mysql->mainKeyValue  =  SNoA;
			     SQL_lookupPar(mysql,&result,&rednum); 
		             fiberType=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);
 
                             if(fiberType==1){
		                         SNoRecvOnline = atoi(SNoA);

					 mysql->filedsName    = "rtuCM"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 CM =atoi(result[0]);
					 SQL_freeResult(&result,&rednum);
		 
					 mysql->filedsName    = "Level"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 ANo =atoi(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "AT06"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 PowerGateOnline=atof(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "T3"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 T3 =computTime(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "T4"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 T4= computTime(result[0]);
					 SQL_freeResult(&result,&rednum);
                                    
                             }else{
		                         SNoRecvBackup = atoi(SNoA);

					 mysql->filedsName    = "AT06"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 PowerGateBackup=atof(result[0]);
					 SQL_freeResult(&result,&rednum);
                             }

                             mysql->mainKeyValue  =  SNoB;
                             mysql->filedsName    =  "fiberType"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             fiberType=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);
                             if(fiberType==1){
		                         SNoRecvOnline = atoi(SNoB);

					 mysql->filedsName    = "rtuCM"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 CM =atoi(result[0]);
					 SQL_freeResult(&result,&rednum);
		 
					 mysql->filedsName    = "Level"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 ANo =atoi(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "AT06"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 PowerGateOnline=atof(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "T3"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 T3 =computTime(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "T4"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 T4= computTime(result[0]);
					 SQL_freeResult(&result,&rednum);
                                    
                             }else{
		                         SNoRecvBackup = atoi(SNoB);

					 mysql->filedsName    = "AT06"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 PowerGateBackup=atof(result[0]);
					 SQL_freeResult(&result,&rednum);
                             }

                             mysql->mainKeyValue  =  sSNoA;
                             mysql->filedsName    =  "fiberType";                              
			     SQL_lookupPar(mysql,&result,&rednum); 
		             fiberType=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);
                             if(fiberType==1){
		                         SNoSendOnline = atoi(sSNoA);
		                           
                             }else{
		                         SNoSendBackup = atoi(sSNoA);
                             }

                             mysql->mainKeyValue  =  sSNoB;
                             mysql->filedsName    =  "fiberType"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             fiberType=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);
                             if(fiberType==1){
		                         SNoSendOnline = atoi(sSNoB);
                                   
                             }else{
		                         SNoSendBackup = atoi(sSNoB);
                             }
  
		             node=(checkProtectNode *)malloc(sizeof(checkProtectNode));		    
		             node->CM             =CM;
			     node->ANo            =ANo;
                             node->PNo            =atoi(PNo);           
			     node->SNoRecvOnline  =SNoRecvOnline;
			     node->SNoRecvBackup  =SNoRecvBackup;
			     node->PowerGateOnline=PowerGateOnline;
			     node->PowerGateBackup=PowerGateBackup;
			     node->masterSwitchPos=masterSwitchPos;

			     node->SNoSendOnline  =SNoSendOnline;
			     node->SNoSendBackup  =SNoSendBackup;

			     node->slaverSwitchPos=slaverSwitchPos;
			     node->slaverModuleNo =slaverModuleNo;
                             strcpy(node->slaverIP,slaverIP);
			     node->ConnectPos     =ConnectPos;

			     node->fristAlarmFlag = 0;
			     node->nextAlarmTime  = getLocalTimestamp();
			     node->alarmClick     = T3; 
  
                             find=find_protect_node(head,atoi(PNo));                          // 查看链表中是否已经存在SNo光路
			     if(find ==NULL){
				  head = insert_protect_node(head,node);                
			     }else{

			          head = delete_protect_node(head,atoi(PNo));     
				  head = insert_protect_node(head,node); 
		             }                             
  			     
                             mysql->tableName    =  "ProtectGroupTable";
		             mysql->filedsValue  =  "1";                
		             mysql->filedsName   =  "Status";
		             mysql->mainKeyValue =  PNo;
		             if(!semaphore_p())  
		                 exit(EXIT_FAILURE);                                //P
		             rc=SQL_modify(mysql);
		             if( rc != SQLITE_OK ){
				      printf( "Modify SQL error\n");
				      sqlite3_free(zErrMsg);
			     }
		             if(!semaphore_v())                                         //V
		                 exit(EXIT_FAILURE);
     
		    } 
                        
                    
		}
         }

	 SQL_Destory(mysql);  

	 sqlite3_close(mydb);

         return(head);

}

checkProtectNode  * removeAllNode(checkProtectNode * head)
{
   checkProtectNode * p=NULL;
   p=head;
   if(p==NULL)
      return p=NULL;  
   else
     while(p!=NULL)
        p=delete_protect_node(p,p->PNo);
   return p;
}

/**************************************************************************/
/***初始化测试链表****/
/*
    (1)创建一个空链表
    (2)将数据库障碍告警测试表中具有启动标识（status=1）的光路，加入初始化链表。
          --->光路状态为1 表示正在进行障碍告警测试表，因此在初始化过程中需要将其加入调度链表.
          --->保证每次启动，需要进程测试的节点能加入链表（例如，停机重启之后).

*/
checkProtectNode * InitA_CycleLink(void)
{
	 sqlite3 *mydb;
	 sql  *mysql;
	 char *zErrMsg = 0,*ModNo,*SNoOnline;
         char PNo[10],strModNo[10],SNoA[10],SNoB[10],sSNoA[10],sSNoB[10];
         int   CM=0,ANo=0,slaverSwitchPos=0,masterSwitchPos=0,slaverModuleNo=0,ConnectPos=0,SNoRecvOnline=0,SNoRecvBackup=0,SNoSendOnline=0,SNoSendBackup=0;
         float PowerGateBackup=0.0,PowerGateOnline=0.0;
         char  slaverIP[16];
	 int rc,i,ret=-1,rednum=0,PN=0;
         int fiberType=-1,ModuleType=-1,protectFlag=0;
	 char resultPNo[64][5];
         char **result = NULL;
         checkProtectNode *head=NULL,*node=NULL;
        
         time_t T3,T4;
         head = creat_protect_link();
         head = delete_protect_node(head,0);                                     
	 mysql = SQL_Create();

	 rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
	 if( rc != SQLITE_OK ){
	    printf( "Lookup SQL error\n");
	 }

	 mysql->db = mydb;
	 mysql->tableName   =  "ProtectGroupTable";	
         mysql->filedsValue =  "1";                                
         mysql->filedsName  =  "Status";
         PN=SQL_findPNo(mysql,resultPNo);
         if(PN>0){
		for(i =0 ;i<PN;i++){
   		    
		     strcpy(PNo,resultPNo[i]);    
                           
                     uint32tostring((atoi(PNo)-1)/4+1,strModNo);                  // 获取所属模块类型 
                     mysql->mainKeyValue  = strModNo;
                     mysql->tableName     =  "SubModuleTypeTable";
                     mysql->filedsName    =  "ModuleType";
                     SQL_lookupPar(mysql,&result,&rednum); 
                     ModuleType=atoi(result[0]); 
                     SQL_freeResult(&result,&rednum);     
                     if(ModuleType ==3 ){
			     mysql->tableName     =  "ProtectGroupTable";
			     mysql->mainKeyValue  =   PNo;
    
			     mysql->filedsName    =  "SwitchPos"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             masterSwitchPos=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);

			     mysql->filedsName    =  "sSwitchPos"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             slaverSwitchPos=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);

			     mysql->filedsName    =  "sModNo"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             slaverModuleNo=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);
		              
			     mysql->filedsName    = "ConnectPos"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     ConnectPos=atoi(result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->filedsName    = "sIP"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(slaverIP,result[0]);
			     SQL_freeResult(&result,&rednum);
 
			     mysql->filedsName    = "SNoA"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(SNoA,result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->filedsName    = "SNoB"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(SNoB,result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->filedsName    = "sSNoA"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(sSNoA,result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->filedsName    = "sSNoB"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
			     strcpy(sSNoB,result[0]);
			     SQL_freeResult(&result,&rednum);

			     mysql->tableName     =  "AlarmTestSegmentTable";   
			     mysql->filedsName    =  "fiberType"; 
                             mysql->mainKeyValue  =  SNoA;
			     SQL_lookupPar(mysql,&result,&rednum); 
		             fiberType=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);

                             if(fiberType==1){
		                         SNoRecvOnline = atoi(SNoA);

					 mysql->filedsName    = "rtuCM"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 CM =atoi(result[0]);
					 SQL_freeResult(&result,&rednum);
		 
					 mysql->filedsName    = "Level"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 ANo =atoi(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "AT06"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 PowerGateOnline=atof(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "T3"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 T3 =computTime(result[0]);

					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "T4"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 T4= computTime(result[0]);

					 SQL_freeResult(&result,&rednum);
                                    
                             }else{
		                         SNoRecvBackup = atoi(SNoA);

					 mysql->filedsName    = "AT06"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 PowerGateBackup=atof(result[0]);
					 SQL_freeResult(&result,&rednum);
                             }

                             mysql->mainKeyValue  =  SNoB;
                             mysql->filedsName    =  "fiberType"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             fiberType=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);

                             if(fiberType==1){
		                         SNoRecvOnline = atoi(SNoB);

					 mysql->filedsName    = "rtuCM"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 CM =atoi(result[0]);
					 SQL_freeResult(&result,&rednum);
		 
					 mysql->filedsName    = "Level"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 ANo =atoi(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "AT06"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 PowerGateOnline=atof(result[0]);
					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "T3"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 T3 =computTime(result[0]);

					 SQL_freeResult(&result,&rednum);

					 mysql->filedsName    = "T4"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 T4= computTime(result[0]);

					 SQL_freeResult(&result,&rednum);
                                    
                             }else{
		                         SNoRecvBackup = atoi(SNoB);

					 mysql->filedsName    = "AT06"; 
					 SQL_lookupPar(mysql,&result,&rednum); 
					 PowerGateBackup=atof(result[0]);
					 SQL_freeResult(&result,&rednum);
                             }

                             mysql->mainKeyValue  =  sSNoA;
                             mysql->filedsName    =  "fiberType";                              
			     SQL_lookupPar(mysql,&result,&rednum); 
		             fiberType=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);

                             if(fiberType==1){
		                         SNoSendOnline = atoi(sSNoA);
		                           
                             }else{
		                         SNoSendBackup = atoi(sSNoA);
                             }

                             mysql->mainKeyValue  =  sSNoB;
                             mysql->filedsName    =  "fiberType"; 
			     SQL_lookupPar(mysql,&result,&rednum); 
		             fiberType=atoi(result[0]);
		             SQL_freeResult(&result,&rednum);
                             if(fiberType==1){
		                         SNoSendOnline = atoi(sSNoB);
                                   
                             }else{
		                         SNoSendBackup = atoi(sSNoB);
                             }

                             

		             node=(checkProtectNode *)malloc(sizeof(checkProtectNode));		    
		             node->CM             =CM;
			     node->ANo            =ANo;
                             node->PNo            =atoi(PNo);           
			     node->SNoRecvOnline  =SNoRecvOnline;
			     node->SNoRecvBackup  =SNoRecvBackup;
			     node->PowerGateOnline=PowerGateOnline;
			     node->PowerGateBackup=PowerGateBackup;
			     node->masterSwitchPos=masterSwitchPos;

			     node->SNoSendOnline  =SNoSendOnline;
			     node->SNoSendBackup  =SNoSendBackup;

			     node->slaverSwitchPos=slaverSwitchPos;
			     node->slaverModuleNo =slaverModuleNo;
                             strcpy(node->slaverIP,slaverIP);
			     node->ConnectPos     =ConnectPos;

			     node->fristAlarmFlag =0;
			     node->nextAlarmTime  = getLocalTimestamp();
			     node->alarmClick     =T3;                               
			     head=insert_protect_node(head,node); 

		    } 
                        
                    
		}
         }

	 SQL_Destory(mysql);  
	 sqlite3_close(mydb);

         return(head);
}

alarmNode * InitB_CycleLink(void)
{
         alarmNode *head,*node;
         head = link_creat_B();
         head = delete_B(head,0); 
         return(head);
}

int updateProtectInformation(int recvPNo,int recvSNoOnline,int recvSNoBackup, int sendSNoOnline ,int sendSNoBackup, int masterSwitchPos, int slaverSwitchPos )
{

      sqlite3 *mydb;
      sql  *mysql;
      char PNo[10],RecvON[10],RecvBK[10],SendON[10],SendBK[10],mSwPos[10],sSwPos[10];
      int rc;
      uint32tostring(recvPNo        , PNo);
      uint32tostring(recvSNoOnline  , RecvON);
      uint32tostring(recvSNoBackup  , RecvBK);
      uint32tostring(sendSNoOnline  , SendON);
      uint32tostring(sendSNoBackup  , SendBK);
      uint32tostring(masterSwitchPos, mSwPos);
      uint32tostring(slaverSwitchPos, sSwPos);

      mysql = SQL_Create();

      rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
      if( rc != SQLITE_OK ){
	    printf( "Lookup SQL error\n");
            return -1;
      }
 
      if(!semaphore_p())  
           exit(EXIT_FAILURE);  
      mysql->db = mydb;
      mysql->tableName     =  "ProtectGroupTable";	
      mysql->mainKeyValue  =   PNo;
      mysql->filedsName    =  "SwitchPos";                                
      mysql->filedsValue   =  mSwPos; 
      SQL_modify(mysql); 

      mysql->filedsName    =  "sSwitchPos";                                
      mysql->filedsValue   =  sSwPos; 
      SQL_modify(mysql); 
  
      mysql->tableName     =  "AlarmTestSegmentTable";
      mysql->filedsName    =  "fiberType";  
      mysql->mainKeyValue  =  RecvON;                             
      mysql->filedsValue   =   "1"; 
      SQL_modify(mysql); 

      mysql->mainKeyValue  =  RecvBK;                             
      mysql->filedsValue   =   "0"; 
      SQL_modify(mysql); 

      mysql->mainKeyValue  =  SendON;                             
      mysql->filedsValue   =   "1"; 
      SQL_modify(mysql); 

      mysql->mainKeyValue  =  SendBK;                             
      mysql->filedsValue   =   "0"; 
      SQL_modify(mysql); 


      mysql->tableName     =  "PortOccopyTable";
      mysql->filedsName    =  "FiberType";  
      mysql->mainKeyValue  =  RecvON;                             
      mysql->filedsValue   =   "2"; 
      SQL_modify(mysql); 

      mysql->mainKeyValue  =  RecvBK;                             
      mysql->filedsValue   =   "1"; 
      SQL_modify(mysql); 

      mysql->mainKeyValue  =  SendON;                             
      mysql->filedsValue   =   "2"; 
      SQL_modify(mysql); 

      mysql->mainKeyValue  =  SendBK;                             
      mysql->filedsValue   =   "1"; 
      SQL_modify(mysql); 

       if(!semaphore_v())  
           exit(EXIT_FAILURE);  

      SQL_Destory(mysql);  
      sqlite3_close(mydb);
    
      return 0;
}

int uploadSwitchInfromation(int CM,int recvSNoOnline, int sendSNoOnline , int masterSwitchPos, int slaverSwitchPos,int Type)
{

        sqlite3 *mydb;
        sql  *mysql;
        backData *bData=NULL; 
        char ** result;
        int  rednum,rc;
        char RecvON[10],SendON[10],mSwPos[10],sSwPos[10],strSNo[10];

        bData=backData_Create();
        bData->SwitchStatusRecv  =masterSwitchPos;
        bData->SwitchStatusSend  =slaverSwitchPos;

        bData->SNoOnlineRecv  =recvSNoOnline;
        bData->SNoOnlineSend  =sendSNoOnline;

        bData->level      =0;

	mysql = SQL_Create();

	rc = sqlite3_open("/web/cgi-bin/System.db", &mydb);
	if( rc != SQLITE_OK )
	   printf("Lookup IP SQL error\n");
			 
	mysql->db = mydb;
        mysql->tableName        = "AlarmTestSegmentTable";
	mysql->filedsName       = "IP01";
	uint32tostring(recvSNoOnline,strSNo);
	mysql->mainKeyValue = strSNo; 
        if( SQL_existIN_db(mysql)){ 
		SQL_lookupPar(mysql,&result,&rednum);
		strcpy(bData->backIP,result[0]);
		
		SQL_freeResult(&result,&rednum); 
		upload(bData,recvSNoOnline,CM,Type);
        }
        SQL_Destory(mysql);  
        sqlite3_close(mydb);
        
}
                 
alarmNode  *rollPolingProtect(checkProtectNode *headA,alarmNode *headB)
{
     	checkProtectNode *p=NULL;
        alarmNode *q=NULL,*node=NULL;   
        modbus_t  *mb=NULL;
        time_t    nowTime=0,nextAlarmTime=0,alarmClick=0;
	char      **result = NULL;
        char      slaverIP[16];
	int       rc=0,rednum=0,fristAlarmFlag=0,swFlag=0, getPowerValueCounter=0,recvPNo=0,sendPNo=0;
        int       CM=0,ANo=0,slaverSwitchPos=0,masterSwitchPos=0,slaverModuleNo=0,ConnectPos=0,SNoRecvOnline=0,SNoRecvBackup=0,SNoSendOnline=0,SNoSendBackup=0,TempInt=0;
        int       sendSwitchPos=0,recvSwitchPos=0,DoSwitchCounter=5,subPort=0,moduleNo=0,SWPos_Send=0,SWPos_Recv=0,SWNo_Recv=0,SWNo_Send=0;
        float     PowerGateBackup=0.0,PowerGateOnline=0.0,powerValue=0.0,powerValueBak=0.0,TempFolat=0.0;
  
	p= headA;
        q= headB;

	while(p!=NULL){   
             
	        ANo=p->ANo; 
		CM =p->CM;
		recvPNo=p->PNo;                                                                       
		SNoRecvOnline=p->SNoRecvOnline;
		SNoRecvBackup=p->SNoRecvBackup;
		PowerGateOnline=p->PowerGateOnline;   
		PowerGateBackup=p->PowerGateBackup; 
		SNoSendOnline=p->SNoSendOnline;
		SNoSendBackup=p->SNoSendBackup;
		masterSwitchPos=p->masterSwitchPos;  
		slaverSwitchPos=p->slaverSwitchPos;  
		fristAlarmFlag=p->fristAlarmFlag; 
		nextAlarmTime=p->nextAlarmTime; 
		alarmClick=p->alarmClick;
        
 
             swFlag=0;

	     if(!setModbus_P())                                            
		 exit(EXIT_FAILURE); 		        
             mb=newModbus(MODBUS_DEV,MODBUS_BUAD);
             powerValue    = getOneOpticalValue(mb,SNoRecvOnline,MODE3_PROTECT_MASTER);                   //获取下行在纤光功率光功率值    
             powerValueBak = getOneOpticalValue(mb,SNoRecvBackup,MODE3_PROTECT_MASTER);                   //获取备纤上的光功率值         
	     if(   powerValue==0)powerValue   =-70.0;  
	     if(powerValueBak==0)powerValueBak=-70.0;    
             freeModbus(mb);  	                 
             if(!setModbus_V())                                       
                 exit(EXIT_FAILURE);  
                         
             if( powerValue < PowerGateOnline ){  
   
		     nowTime = getLocalTimestamp();  
		     subPort =(SNoRecvBackup-1)%8+1; //(1-8)   SNoRecvBackup(1-64)
                     moduleNo=(SNoRecvBackup-1)/8+1; //(1-8)
		     if(subPort==1 || subPort==5){
			SWNo_Recv=(moduleNo-1)*4 + SW_A; 
			SWNo_Send=(moduleNo-1)*4 + SW_B; 
			SWPos_Recv=subPort==1?PARALLEL:ACROSS;
                        SWPos_Send=subPort==1?PARALLEL:ACROSS;
		     }
		     if(subPort==3 || subPort==7){
                        SWNo_Recv =(moduleNo-1)*4  + SW_C;
                        SWNo_Send =(moduleNo-1)*4  + SW_D;
                        SWPos_Recv=subPort==3?PARALLEL:ACROSS;
                        SWPos_Send=subPort==3?PARALLEL:ACROSS;
                     }

                     if(powerValueBak < PowerGateBackup){             
		             if(fristAlarmFlag == 0){         //异常状态C: 首次出现异常 -->fristAlarmFlag=0   实际光功率值<阈值                 
				      printf("StateC--->在纤光路%d首次出现异常...powerValue:%f <---> PowerGateOnline:%f\n",SNoRecvOnline ,powerValue,PowerGateOnline); 	                                              
				      fristAlarmFlag = 1;
				      swFlag=1;   
				      if(!setModbus_P())                                       
					 exit(EXIT_FAILURE); 	
				         mb=newModbus(MODBUS_DEV,MODBUS_BUAD);
				         doOpticalProtectSwitch(mb,SWNo_Recv,SWPos_Recv,MODE3_PROTECT_MASTER);           //切换光路上行光开关为原来的相反状态
				         doOpticalProtectSwitch(mb,SWNo_Send,SWPos_Send,MODE3_PROTECT_MASTER);           //切换光路下行光开关为原来的相反状态				         
				         getPowerValueCounter=40;                                                        //超时计数
					 while(getPowerValueCounter-- > 0){
				              powerValue = getOneOpticalValue(mb,SNoRecvBackup,MODE3_PROTECT_MASTER);    //获取备纤上的光功率值
				              if(powerValue <  PowerGateBackup){
				                  //usleep(1024);
                                                  printf("Wiat for successful!----> SNoRecvBackup:%d powerValue:%f PowerGateBackup:%f\n",SNoRecvBackup,powerValue,PowerGateBackup);
				                     
				              }else{
				                  fristAlarmFlag = 0;                                 
				                  getPowerValueCounter= 10;
                                                  printf("光保护切换成功!\n",SNoRecvBackup);
                                                  break;
				              }
				                 
					}
				        freeModbus(mb);   	                      
					if(!setModbus_V())                                            //V  
					     exit(EXIT_FAILURE); 
				        if(fristAlarmFlag ==0 ){                                      //光路切换成功
				                 //更新节点
				                 masterSwitchPos= SWPos_Recv; 
                                                 slaverSwitchPos= SWPos_Send;  
                                                 if(masterSwitchPos==ACROSS ){                      
                                                     if(SNoRecvBackup > SNoRecvOnline){ 
                                                        TempInt=SNoRecvBackup;SNoRecvBackup=SNoRecvOnline;SNoRecvOnline=TempInt; //SNoRecvBackup SNoRecvOnline互换
                                                        TempInt=SNoSendBackup;SNoSendBackup=SNoSendOnline;SNoSendOnline=TempInt; //SNoSendBackup SNoSendOnline互换  
						        TempFolat=PowerGateOnline;PowerGateOnline=PowerGateBackup;PowerGateBackup=TempFolat; //PowerGateBackup PowerGateOnline互换      
                                                     }
                                                 }                               
                                                 if(masterSwitchPos==PARALLEL){
                                                     if( SNoRecvBackup < SNoRecvOnline){ 
                                                         TempInt=SNoRecvBackup;SNoRecvBackup=SNoRecvOnline;SNoRecvOnline=TempInt; 
                                                         TempInt=SNoSendBackup;SNoSendBackup=SNoSendOnline;SNoSendOnline=TempInt; 
						         TempFolat=PowerGateOnline;PowerGateOnline=PowerGateBackup; PowerGateBackup=TempFolat;
                                                     }  
                                                   
                                                 }
				                 //修改数据库状态  
                                                 //pthread_mutex_lock(&mutex);
                                                 updateProtectInformation(recvPNo,SNoRecvOnline,SNoRecvBackup,SNoSendOnline,SNoSendBackup,masterSwitchPos,slaverSwitchPos); 

				                 //上报业务自动切换消息 

                                                 uploadSwitchInfromation(CM,SNoRecvOnline,SNoSendOnline,masterSwitchPos,slaverSwitchPos,RESPONCE_ProtectSwitch);   // 
				                 //执行OTDR测试(上行在纤  下行在纤  上行备纤  下行备纤)
                                                // pthread_mutex_unlock(&mutex);
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoRecvOnline,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoSendOnline,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoRecvBackup,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoSendBackup,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 outPutALL_B(linkHead_alarm_B);
				                 
                                                         
				       }else{                                                         //光路切换失败(备纤异常)

                                                 printf("保护切换失败，状态回退...\n");
                                                 //状态回退
					         if(!setModbus_P())                                   //P  
						    exit(EXIT_FAILURE); 	
						 mb=newModbus(MODBUS_DEV,MODBUS_BUAD);
						 doOpticalProtectSwitch(mb,SWNo_Recv,masterSwitchPos,MODE3_PROTECT_MASTER);               
						 doOpticalProtectSwitch(mb,SWNo_Send,slaverSwitchPos,MODE3_PROTECT_MASTER);               
                                                 freeModbus(mb); 
					         if(!setModbus_V())                                   //V  
					            exit(EXIT_FAILURE); 
                                                 //pthread_mutex_lock(&mutex);
				                 //上报主备纤均损坏故障消息 
                                                 uploadSwitchInfromation(CM,SNoRecvOnline,SNoSendOnline,masterSwitchPos,slaverSwitchPos,RESPONCE_LineFaultWarming); 
                                                 //pthread_mutex_unlock(&mutex);
				                 //执行OTDR测试(上行在纤  下行在纤 上行备纤  下行备纤 )  
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoRecvOnline,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoSendOnline,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoRecvBackup,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoSendBackup,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 outPutALL_B(linkHead_alarm_B);
				                
                                                      
				       }
				       nextAlarmTime  = getLocalTimestamp()+alarmClick;                  
		             }else if(nowTime >= nextAlarmTime){                       //状态D:长期处于异常 -->fristAlarmFlag=1   实际光功率值<阈值  
				                 swFlag=1;
					         printf("StateD--->在纤光路%d长期处于异常...powerValue:%f <---> PowerGateOnline:%f\n",SNoRecvOnline ,powerValue,PowerGateOnline);                            
				                 //上报主备纤均损坏故障消息 
                                                 //pthread_mutex_lock(&mutex);
                                                 uploadSwitchInfromation(CM,SNoRecvOnline,SNoSendOnline,masterSwitchPos,slaverSwitchPos,RESPONCE_LineFaultWarming);
                                                 //pthread_mutex_unlock(&mutex);
				                 //执行OTDR测试(上行在纤  下行在纤 上行备纤  下行备纤 )  
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoRecvOnline,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoSendOnline,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoRecvBackup,CM,ANo);
                                                 else{ p=NULL;break;}
                                                 if(p!=NULL) q=insertOTDtestNode(q,SNoSendBackup,CM,ANo);
                                                 else{ p=NULL;break;}
                                                                     
				                 nextAlarmTime  = getLocalTimestamp()+alarmClick;   
		             } 
                         }else if(fristAlarmFlag!=0){
                              //状态E:备纤先于在纤修复，业务光自动切换到备纤 --> fristAlarmFlag=0  备纤实际光功率值>=阈值    
                              printf("StateE--->备纤光路%d先于在纤光路%d修复，业务光自动切换到备纤\n",SNoRecvBackup,SNoRecvOnline);
			      swFlag=1;
			      if(!setModbus_P())                             
				  exit(EXIT_FAILURE); 	
			      mb=newModbus(MODBUS_DEV,MODBUS_BUAD); 
			      doOpticalProtectSwitch(mb,SWNo_Recv,SWPos_Recv,MODE3_PROTECT_MASTER);               
			      doOpticalProtectSwitch(mb,SWNo_Send,SWPos_Send,MODE3_PROTECT_MASTER);     
                              masterSwitchPos= SWPos_Recv;   
                              slaverSwitchPos= SWPos_Send;                                          
                              freeModbus(mb);  	 
			      if(!setModbus_V())                               
				  exit(EXIT_FAILURE);    
			      masterSwitchPos= SWPos_Recv; 
                              slaverSwitchPos= SWPos_Send;  
                              if(masterSwitchPos==ACROSS ){                       //以光开关状态为依据 将原来的在纤更新为备纤 将原来的备纤更新为在纤 
                                       if(SNoRecvBackup > SNoRecvOnline){ 
                                                 TempInt=SNoRecvBackup;   
                                                 SNoRecvBackup   =  SNoRecvOnline;
                                                 SNoRecvOnline   =  TempInt; 

                                                 TempInt=SNoSendBackup;   
                                                 SNoSendBackup   =  SNoSendOnline;
                                                 SNoSendOnline   =  TempInt; 

						 TempFolat=PowerGateOnline;   
						 PowerGateOnline=PowerGateBackup; 
		                                 PowerGateBackup=TempFolat;
                                      }
                              }                               
                              if(masterSwitchPos==PARALLEL){
                                       if( SNoRecvBackup  < SNoRecvOnline){ 
                                                 TempInt=SNoRecvBackup;   
                                                 SNoRecvBackup   =  SNoRecvOnline;
                                                 SNoRecvOnline   =  TempInt; 
 
                                                 TempInt=SNoSendBackup;   
                                                 SNoSendBackup   =  SNoSendOnline;
                                                 SNoSendOnline   =  TempInt; 

						 TempFolat=PowerGateOnline;   
						 PowerGateOnline=PowerGateBackup; 
		                                 PowerGateBackup=TempFolat;
                                        }  
                                                   
                             }
                             //pthread_mutex_lock(&mutex);
                             uploadSwitchInfromation(CM,SNoRecvOnline,SNoSendOnline,masterSwitchPos,slaverSwitchPos,RESPONCE_ProtectSwitch);  //异常恢复
                             if(p!=NULL) q=insertOTDtestNode(q,SNoRecvOnline,CM,ANo);
                             else{ p=NULL;break;}
                             if(p!=NULL) q=insertOTDtestNode(q,SNoSendOnline,CM,ANo);
                             else{ p=NULL;break;}
                             if(p!=NULL) q=insertOTDtestNode(q,SNoRecvBackup,CM,ANo);
                             else{ p=NULL;break;}
                             if(p!=NULL) q=insertOTDtestNode(q,SNoSendBackup,CM,ANo);
                             else{ p=NULL;break;}
                             fristAlarmFlag= 0;
                             //pthread_mutex_unlock(&mutex);
                         } 
                 }else if(fristAlarmFlag!=0){       //正常             
                                                    //状态A:在纤光路%d从异常中恢复 -->        
                             printf("StateA--->在纤光路%d从异常中恢复...powerValue:%f <---> PowerGateOnline:%f\n",SNoRecvOnline,    powerValue,         PowerGateOnline); 
			     swFlag=1; 
                            // pthread_mutex_lock(&mutex);
                             uploadSwitchInfromation(CM,SNoRecvOnline,SNoSendOnline,masterSwitchPos,slaverSwitchPos,RESPONCE_ProtectSwitch);  //异常恢复 
                             //pthread_mutex_unlock(&mutex);
                             if(p!=NULL) q=insertOTDtestNode(q,SNoRecvOnline,CM,ANo);
                             else{ p=NULL;break;}
                             if(p!=NULL) q=insertOTDtestNode(q,SNoSendOnline,CM,ANo);
                             else{ p=NULL;break;}
                             if(p!=NULL) q=insertOTDtestNode(q,SNoRecvBackup,CM,ANo);
                             else{ p=NULL;break;}
                             if(p!=NULL) q=insertOTDtestNode(q,SNoSendBackup,CM,ANo);
                             else{ p=NULL;break;}
                          
                             fristAlarmFlag= 0;
                } 
                else ;                              //状态B:正常  -->  fristAlarmFlag=0 实际光功率值>=阈值            
  
                if(p!=NULL){
			p->SNoRecvOnline=SNoRecvOnline;
			p->SNoRecvBackup=SNoRecvBackup;
			p->PowerGateOnline=PowerGateOnline;   
			p->PowerGateBackup=PowerGateBackup; 
			p->SNoSendOnline=SNoSendOnline;
			p->SNoSendBackup=SNoSendBackup;
			p->masterSwitchPos=masterSwitchPos;  
			p->slaverSwitchPos=slaverSwitchPos;  
			p->fristAlarmFlag=fristAlarmFlag;
			p->nextAlarmTime =nextAlarmTime;
			p=p->next;
                }else break;
                
                if(headA !=NULL && swFlag==1){
                     output_all_protect_node(headA);
                 } 

                    
       }
       return q;
}

void sigrecv_headle(){
       int ret,data,ModNo=0,PNo=0,subSNo=0,SNo=0;
       checkProtectNode * find=NULL;
       while(1){
               if(myQueue->count!=0){
		       ret=Queue_Delete(myQueue,&data);
		       printf("ProtectMasterMain(R): the int value is %d \n",data);
		       if(data>270 && data <370){ 
			        sendMessageQueue_Boa("270-OK-P",PROTECT_MESSAGE_TYPE);                                          //最大一次删除99个节点   
			        SNo = data%270;
                                ModNo=(SNo-1)/8+1;
                                subSNo=(SNo-1)%8 +1; 
                                if(subSNo==1 || subSNo==2 || subSNo==5 ||subSNo == 6 )
                                  PNo =(ModNo-1)*2 + 1;
                                if(subSNo==3 || subSNo==4 || subSNo==7 ||subSNo == 8 )
                                  PNo =(ModNo-1)*2 + 2; 
			        find=find_protect_node(linkHead_check_A,PNo);                        
			        if(find !=NULL){
			            linkHead_check_A = delete_protect_node(linkHead_check_A,PNo);  
                                    printf("Delete PNo:%d",PNo);     
		                }
                                output_all_protect_node(linkHead_check_A);  
			        
                       }
		       switch(data){
			   case 170:{ 
                                        sendMessageQueue_Boa("170-OK",PROTECT_MESSAGE_TYPE);

				        linkHead_check_A = insertWaitingNode(linkHead_check_A);      //启动自动光保护模式

				        output_all_protect_node(linkHead_check_A);
    
				  } break; 

			   case 250:{                                                                //取消自动光保护模式

                                        sendMessageQueue_Boa("250-OK",PROTECT_MESSAGE_TYPE);

				        linkHead_check_A = removeWaitingNode(linkHead_check_A);     
          
				        output_all_protect_node(linkHead_check_A);
	    
				  }  break;

			   case 370:{ 
                                        sendMessageQueue_Boa("370-OK",PROTECT_MESSAGE_TYPE);
 
                                        //pthread_mutex_lock(&mutex);
				        linkHead_check_A = insertWaitingNode(linkHead_check_A);     //请求光保护切换
				        output_all_protect_node(linkHead_check_A);
                                        //pthread_mutex_unlock(&mutex);
  
				  }    break;
			    case 260:{    
                                        sendMessageQueue_Boa("260-OK-P",PROTECT_MESSAGE_TYPE);                                    //清除RTU模式
				        linkHead_check_A = removeAllNode(linkHead_check_A);    
                                        output_all_protect_node(linkHead_check_A);
				       
			           }     break; 
    
			   default:   break;                                                              
		      }

              }else break;
      }
}
void work_line()
{   

       /*初始化测试链表*/
       alarmNode *node_B;

       linkHead_check_A=InitA_CycleLink();

       linkHead_alarm_B=InitB_CycleLink();

       if(linkHead_check_A !=NULL)
            output_all_protect_node(linkHead_check_A);
       else
            printf("linkA Head:NULL\n");

       if(linkHead_alarm_B !=NULL)
            outPutALL_B(linkHead_alarm_B);
       else
            printf("linkB Head:NULL\n");
        /*执行调度程序*/
        while(1){
            sigrecv_headle();
            linkHead_alarm_B=rollPolingProtect(linkHead_check_A,linkHead_alarm_B); 
   
            linkHead_alarm_B=deleteALL_B(linkHead_alarm_B);
 
        }
}
void addNewtoLink(int signum,siginfo_t *info,void *myact);
void main(){
       int flagCycle = 0,flagAlarm=0 ; 
       pid_t cyclePID[MAX_PID_NUM],alarmPID[MAX_PID_NUM];
       pthread_t tha,thb;  
       void *retval;
        /*初始化信号机制（与BOA通信）*/
       struct sigaction act;
       int sig;
       sig=SIGRTMIN+1;  
       sigemptyset(&act.sa_mask);
       act.sa_sigaction=addNewtoLink;
       act.sa_flags=SA_SIGINFO|SA_RESTART;                                                                                                                                                 
       if(sigaction(sig,&act,NULL)<0)                          
       {
              printf("install sigal error\n");
       }

       /*初始化信号量*/
       sem_id        = semget((key_t)1234, 1, (4777 | IPC_CREAT ));                         //创建数据库信号量 :每一个需要用到信号量的进程,在第一次启动的时候需要获取指定的信号量
       modbus_sem_id = semget((key_t)5678, 1, (4777 | IPC_CREAT ));                         //创建ModBus信号量 :每一个需要用到信号量的进程,在第一次启动的时候需要获取指定的信号量

       myQueue = Queue_Initiate();

       work_line();      

}

void addNewtoLink(int signum,siginfo_t *info,void *myact){
      while(sigLock==1);
      sigLock=1;
      Queue_Append(myQueue,info->si_int);
      sigLock=0; 
}

