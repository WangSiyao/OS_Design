
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "stdio.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "fs.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"


/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
	disp_str("-----\"kernel_main\" begins-----\n");

	struct task* p_task;
	struct proc* p_proc= proc_table;
	char* p_task_stack = task_stack + STACK_SIZE_TOTAL;
	u16   selector_ldt = SELECTOR_LDT_FIRST;
        u8    privilege;
        u8    rpl;
	int   eflags;
	int   i, j;
	int   prio;
	for (i = 0; i < NR_TASKS+NR_PROCS; i++) {
	        if (i < NR_TASKS) {     /* 任务 */
                        p_task    = task_table + i;
                        privilege = PRIVILEGE_TASK;
                        rpl       = RPL_TASK;
                        eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */
			prio      = 15;
                }
                else {                  /* 用户进程 */
                        p_task    = task_table + i;
                        privilege = PRIVILEGE_TASK;
                        rpl       = RPL_TASK;
                        eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */
			prio      = 5;
                }

		strcpy(p_proc->name, p_task->name);	/* name of the process */
		p_proc->pid = i;			/* pid */

		p_proc->ldt_sel = selector_ldt;

		memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
		       sizeof(struct descriptor));
		p_proc->ldts[0].attr1 = DA_C | privilege << 5;
		memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
		       sizeof(struct descriptor));
		p_proc->ldts[1].attr1 = DA_DRW | privilege << 5;
		p_proc->regs.cs	= (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ds	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.es	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.fs	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ss	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

		p_proc->regs.eip = (u32)p_task->initial_eip;
		p_proc->regs.esp = (u32)p_task_stack;
		p_proc->regs.eflags = eflags;

		/* p_proc->nr_tty		= 0; */

		p_proc->p_flags = 0;
		p_proc->p_msg = 0;
		p_proc->p_recvfrom = NO_TASK;
		p_proc->p_sendto = NO_TASK;
		p_proc->has_int_msg = 0;
		p_proc->q_sending = 0;
		p_proc->next_sending = 0;

		for (j = 0; j < NR_FILES; j++)
			p_proc->filp[j] = 0;

		p_proc->ticks = p_proc->priority = prio;

		p_task_stack -= p_task->stacksize;
		p_proc++;
		p_task++;
		selector_ldt += 1 << 3;
	}

        /* proc_table[NR_TASKS + 0].nr_tty = 0; */
        /* proc_table[NR_TASKS + 1].nr_tty = 1; */
        /* proc_table[NR_TASKS + 2].nr_tty = 1; */

	k_reenter = 0;
	ticks = 0;

	p_proc_ready	= proc_table;

	init_clock();
        init_keyboard();

	restart();

	while(1){}
}


/*****************************************************************************
 *                                get_ticks
 *****************************************************************************/
PUBLIC int get_ticks()
{
	MESSAGE msg;
	reset_msg(&msg);
	msg.type = GET_TICKS;
	send_recv(BOTH, TASK_SYS, &msg);
	return msg.RETVAL;
}


/*======================================================================*
                               TestA
 *======================================================================*/
void TestA()
{
	int fd;
	int i, n;

	char filename[MAX_FILENAME_LEN+1] = "blah";
	const char bufw[] = "abcde";
	const int rd_bytes = 3;
	char bufr[rd_bytes];

	assert(rd_bytes <= strlen(bufw));

	/* create */
	fd = open(filename, O_CREAT | O_RDWR);
	assert(fd != -1);
	printl("File created: %s (fd %d)\n", filename, fd);

	/* write */
	n = write(fd, bufw, strlen(bufw));
	assert(n == strlen(bufw));

	/* close */
	close(fd);

	/* open */
	fd = open(filename, O_RDWR);
	assert(fd != -1);
	printl("File opened. fd: %d\n", fd);

	/* read */
	n = read(fd, bufr, rd_bytes);
	assert(n == rd_bytes);
	bufr[n] = 0;
	printl("%d bytes read: %s\n", n, bufr);

	/* close */
	close(fd);

	char * filenames[] = {"/foo", "/bar", "/baz"};

	/* create files */
	for (i = 0; i < sizeof(filenames) / sizeof(filenames[0]); i++) {
		fd = open(filenames[i], O_CREAT | O_RDWR);
		assert(fd != -1);
		printl("File created: %s (fd %d)\n", filenames[i], fd);
		close(fd);
	}

	char * rfilenames[] = {"/bar", "/foo", "/baz", "/dev_tty0"};

	/* remove files */
	for (i = 0; i < sizeof(rfilenames) / sizeof(rfilenames[0]); i++) {
		if (unlink(rfilenames[i]) == 0)
			printl("File removed: %s\n", rfilenames[i]);
		else
			printl("Failed to remove file: %s\n", rfilenames[i]);
	}
	
	TestD();

	
	

	char tty_name[] = "/dev_tty0";

	char rdbuf[128];


	int fd_stdin  = open(tty_name, O_RDWR);
	assert(fd_stdin  == 0);
	int fd_stdout = open(tty_name, O_RDWR);
	assert(fd_stdout == 1);

//	char filename[MAX_FILENAME_LEN+1] = "zsp01";
	const char bufwns[80] = {0};
//	const int rd_bytes = 3;
//	char bufr[rd_bytes];
	
	clear();
	printf("                        ==================================\n");
	printf("                                   MyTinix v1.0.2             \n");
	printf("                                 Kernel on Orange's \n\n");
	printf("                                     Welcome !\n");
	printf("                        ==================================\n");
	
	while (1) {
		printl("[root@localhost /] ");
		int r = read(fd_stdin, rdbuf, 70);
		rdbuf[r] = 0;
		//show();
		if (strcmp(rdbuf, "process") == 0)
		{
			//ProcessManage();
		}
		else if (strcmp(rdbuf, "filemng") == 0)
		{
			printf("File Manager is already running on CONSOLE-1 ! \n");
			continue;

		}else if (strcmp(rdbuf, "help") == 0)
		{
			help();
		}
		else if (strcmp(rdbuf, "NewFile") == 0)
		{

			NewFile(fd_stdin, fd_stdout);
		}
		else if(strcmp(rdbuf, "RemoveFile") == 0)
		{
			RemoveFile(fd_stdin, fd_stdout);
		}
		else if(strcmp(rdbuf, "WriteFile") == 0)
		{
			WriteFile(fd_stdin, fd_stdout);
		}
		else if(strcmp(rdbuf, "ReadFile") == 0)
		{
			ReadFile(fd_stdin, fd_stdout);
		}
		else if(strcmp(rdbuf, "Chess") == 0)
		{
			chess(fd_stdin, fd_stdout);
		}
		else if (strcmp(rdbuf, "clear") == 0)
		{
			clear();
			printf("                        ==================================\n");
			printf("                                   Honginix v1.0.2             \n");
			printf("                                 Kernel on Orange's \n\n");
			printf("                                     Welcome !\n");
			printf("                        ==================================\n");
		}
		else
			printf("Command not found, please check!\n");
	}
	

	spin("TestA");
}

/*======================================================================*
                               TestB
 *======================================================================*/
void TestB()
{
	char tty_name[] = "/dev_tty1";

	int fd_stdin  = open(tty_name, O_RDWR);
	assert(fd_stdin  == 0);
	int fd_stdout = open(tty_name, O_RDWR);
	assert(fd_stdout == 1);

	char rdbuf[128];

	while (1) {
		printf("$ ");
		int r = read(fd_stdin, rdbuf, 70);
		rdbuf[r] = 0;

		if (strcmp(rdbuf, "hello") == 0)
			printf("hello world!\n");
		else
			if (rdbuf[0])
				printf("{%s}\n", rdbuf);
	}

	assert(0); /* never arrive here */
}

/*======================================================================*
                               TestB
 *======================================================================*/
void TestC()
{
	spin("TestC");
	/* assert(0); */
}

void TestD()
{
	printl("hello Ninux");
}

void clear()
{
	clear_screen(0,console_table[current_console].cursor);
	console_table[current_console].crtc_start = 0;
	console_table[current_console].cursor = 0;
	
}

void help()
{
	printf("=============================================================================\n");
	printf("Command List     :\n");
	printf("1. process       : A process manage,show you all process-info here\n");
	printf("2. filemng       : Run the file manager\n");
	printf("3. clear         : Clear the screen\n");
	printf("4. help          : Show this help message\n");
//	printf("5. taskmanager   : Run a task manager,you can add or kill a process here\n");
	printf("5. chess         : Run a chess game on this OS\n");
	printf("6. NewFile       : Create a new file on this OS\n");
	printf("7. RemoveFile    : Remove a new file on this OS\n");
	printf("8. WriteFile     : Write into a file on this OS\n");
	printf("9. ReadFile      : Write into a file on this OS\n");
	printf("==============================================================================\n");		
}

void NewFile(int fd_stdin,int fd_stdout)
{
	int fd;
	char buf[80]={0};

	char IsFirst = 0;
	int IsFinish = 0;
	while(!IsFinish)
	{

		printf("Please input the file name..\n");

		int r = read(fd_stdin, buf, 70);
		buf[r] = 0;

		fd = open(buf, O_CREAT | O_RDWR);
		assert(fd != -1);
		printl("File created: %s (fd %d)\n", buf, fd);
		close(fd);		

		IsFinish = 1;
	}
	
}

void RemoveFile(int fd_stdin,int fd_stdout)
{
	int fd;
	char buf[80]={0};

	char IsFirst = 0;
	int IsFinish = 0;
	while(!IsFinish)
	{

		printf("Please input the file name..\n");

		int r = read(fd_stdin, buf, 70);
		buf[r] = 0;

		if (unlink(buf) == 0)
			printl("File removed: %s\n", buf);
		else
			printl("Failed to remove file: %s\n", buf);
		
		IsFinish = 1;
	}
	
}

void WriteFile(int fd_stdin,int fd_stdout)
{
	int fd;
	int n;
	char buf[80]={0};
	char wbuf[200]={0};

	char IsFirst = 0;
	int IsFinish = 0;
	while(!IsFinish)
	{

		printf("Please input the file name..\n");

		int r = read(fd_stdin, buf, 70);
		buf[r] = 0;

		fd = open(buf, O_RDWR);
		assert(fd != -1);
		printl("File opened. fd: %d\n", fd);

		printf("Please input the text..\n");
		int w = read(fd_stdin, wbuf, 200);
		wbuf[w] = 0;
		
		n = write(fd, wbuf, strlen(wbuf));
		assert(n == strlen(wbuf));
		
		close(fd);
		IsFinish = 1;
	}
}

void ReadFile(int fd_stdin,int fd_stdout)
{
	int fd;
	int n;
	char buf[80]={0};
	int rd_bytes = 200;
	char bufr[200]={0};

	char IsFirst = 0;
	int IsFinish = 0;
	while(!IsFinish)
	{

		printf("Please input the file name..\n");

		int r = read(fd_stdin, buf, 70);
		buf[r] = 0;

		fd = open(buf, O_RDWR);
		assert(fd != -1);
		
		n = read(fd, bufr, rd_bytes);
		//assert(n == rd_bytes);
		bufr[n] = 0;
		printl("File read: %s\n", bufr);
		
		close(fd);
		IsFinish = 1;
	}
}

/*****************************************************************************
 *                                chess
 *****************************************************************************/
#define P1 1
#define P2 -1
#define SIZE 3
#define WIN -1
#define UNWIN 0
#define PEACE 1
#define chkAndPutDwnRow(row, col){\
for(col = 0; col < SIZE; col++){\
    if(chsman[row][col] == 0){\
        chsman[row][col] = P2;\
        dsply();\
        return;\
    }\
}\
}
#define chkAndPutDwnCol(row, col){\
for(col = 0; col < SIZE; col++){\
    if(chsman[col][row] == 0){\
        chsman[col][row] = P2;\
        dsply();\
        return;\
    }\
}\
}
#define chkAndPutDwn_Slsh(row, col){\
if(chsman[row][col] == 0){\
    chsman[row][col] = P2;\
    dsply();\
    return;\
}\
}
int chsman[SIZE][SIZE] = {0};

//int enterChsman(int, int);
//void dsply(void);
//void input(void);
//void judge(void);
//int chkWin(void);
//int chkPeace(void);

int stepFlg = 0;

int enterChsman(int row, int col)
{
    
    if(row >= SIZE || col >= SIZE)
        return 0;

    if(chsman[row][col] != 0)
        return 0;
    
    chsman[row][col] = P1;
    return 1;
}

int enterChsman2(int row, int col) //二号玩家使用
{
    
    if(row >= SIZE || col >= SIZE)
        return 0;
    
    if(chsman[row][col] != 0)
        return 0;
    
    chsman[row][col] = P2;
    return 1;
}


//用户输入
void input(int fd_stdin,int fd_stdout)
{
    char buf[80]={0};
    int row = 0;
    int col = 0;
    do{
        printf("Please put in the location you wanted:\n");
       
		int r = read(fd_stdin, buf, 80);
		buf[r] = 0;
        //scanf("%d", &row);
       
        //scanf("%d", &col);

	if(buf[0] == '1'){
		row = 2;
 		col = 0;
	}
	if(buf[0] == '2'){
		row = 2;
 		col = 1;
	}
	if(buf[0] == '3'){
		row = 2;
 		col = 2;
	}
	if(buf[0] == '4'){
		row = 1;
 		col = 0;
	}
	if(buf[0] == '5'){
		row = 1;
 		col = 1;
	}
	if(buf[0] == '6'){
		row = 1;
 		col = 2;
	}
	if(buf[0] == '7'){
		row = 0;
 		col = 0;
	}
	if(buf[0] == '8'){
		row = 0;
 		col = 1;
	}
	if(buf[0] == '9'){
		row = 0;
 		col = 2;
	}
	if(buf[0] == '1'){
		row = 2;
 		col = 0;
	}


        if(enterChsman(row, col) == 1){
            printf("You put on [%d][%d].\n", row, col);
            dsply();
            break;
        }
        else
            printf("Sorry! You input wrong number!\n");
    }while(1);
    return;
}


void input2(int fd_stdin,int fd_stdout)
{
    //int row2,col2;

    char buf[80]={0};
    char buf2[80]={0};
		
   do{
        printf("行号: ");
        int r = read(fd_stdin, buf, 80);
		buf[r] = 0;
        if(enterChsman2((int)buf - 1, (int)buf2 - 1) == 1){
            printl("2号放在了 [%d][%d].\n", (int)buf, (int)buf2);
//            chsman[row2-1][col2-1] = P2;
            dsply();
            
            break;
        }
        else
            printf("Sorry! You input wrong number!！\n");
    }while(1);
    return;
    
}

//系统自动判断并输入函数
void judge(void)
{
    int row, col;
    int i;

    int rskAndAtkLevlRow[SIZE] = {0}, rskAndAtkLevlCol[SIZE] = {0}, rskAndAtkLevlSlsh[2] = {0};

    stepFlg++;  //纪录是第几步了

    for(row = 0; row < SIZE; row++){
        for(col = 0; col < SIZE; col++){
            rskAndAtkLevlRow[row] += chsman[row][col];
        }
    }
    for(col = 0; col < SIZE; col++){
        for(row = 0; row < SIZE; row++){
            rskAndAtkLevlCol[col] += chsman[row][col];
        }
    }
    rskAndAtkLevlSlsh[0] = chsman[0][0] + chsman[1][1] + chsman[2][2];
    rskAndAtkLevlSlsh[1] = chsman[0][2] + chsman[1][1] + chsman[2][0];

    for(i = 0; i < SIZE; i++){
        if(rskAndAtkLevlRow[i] == -2){
            chkAndPutDwnRow(i, col)
        }
    }

    for(i = 0; i< SIZE; i++){
        if(rskAndAtkLevlCol[i] == -2){
            chkAndPutDwnCol(i, col)
        }
    }

    if(rskAndAtkLevlSlsh[0] == -2){
        for(row = 0, col = 0; row < SIZE; row++, col++){
            chkAndPutDwn_Slsh(row, col)
        }
    }

    if(rskAndAtkLevlSlsh[1] == -2){
        for(row = 0, col = 2; row < SIZE; row++, col--){
            chkAndPutDwn_Slsh(row, col)
        }
    }


    for(i = 0; i < SIZE; i++){
        if(rskAndAtkLevlRow[i] == 2){
            chkAndPutDwnRow(i, col)
        }
    }

    for(i = 0; i< SIZE; i++){
        if(rskAndAtkLevlCol[i] == 2){
            chkAndPutDwnCol(i, col)
        }
    }

    if(rskAndAtkLevlSlsh[0] == 2){
        for(row = 0, col = 0; row < SIZE; row++, col++){
            chkAndPutDwn_Slsh(row, col)
        }
    }

    if(rskAndAtkLevlSlsh[1] == 2){
        for(row = 0, col = 2; row < SIZE; row++, col--){
            chkAndPutDwn_Slsh(row, col)
        }
    }

    for(row = 0; row < SIZE; row++){
        for(col = 0; col < SIZE; col++){
            if(chsman[row][col] == 0 && ((row == 0 && col == 0) || (row == 0 && col == 2) ||
                                         (row == 2 && col == 0) || (row == 2 && col == 2))){
                chsman[row][col] = P2;
                dsply();
                return;
            }
        }
    }
}
//显示现在的界面
void dsply(void)
{
    int row, col, i;

    for(i = 0; i < SIZE * 4 + 1; i++)
        printf("-");
    printf("\n");
  //打印函数
    for(row = 0; row < SIZE; row++){
        printf("|");
        for(col = 0; col < SIZE; col++){
            if(chsman[row][col] == P1) printf(" o |");
            else if(chsman[row][col] == P2) printf(" x |");
            else printf("   |");
        }
        printf("\n");
        
        for(i = 0; i < SIZE * 4 + 1; i++)
            printf("-");
        printf("\n");
    }
    return;
}

//检查是否有一方胜出
int chkWin(void)
{
    int i;
    for(i = 0; i < SIZE; i++){
        if(chsman[i][0] + chsman[i][1] + chsman[i][2] == -3 || chsman[0][i] + chsman[1][i] + chsman[2][i] == -3 ||
           chsman[0][0] + chsman[1][1] + chsman[2][2] == -3 || chsman[0][2] + chsman[1][1] + chsman[2][0] == -3){
            return WIN;
        }
    }
    for(i = 0; i < SIZE; i++){
        if(chsman[i][0] + chsman[i][1] + chsman[i][2] == 3 || chsman[0][i] + chsman[1][i] + chsman[2][i] == 3 ||
           chsman[0][0] + chsman[1][1] + chsman[2][2] == 3 || chsman[0][2] + chsman[1][1] + chsman[2][0] == 3){
            return UNWIN;
        }
    }
    
    return 2;
}
//检查是否平局
int chkPeace(void)
{
    int row, col;
    int sum = 0;
    for(row = 0; row < SIZE; row++){
        for(col = 0; col < SIZE; col++){
            if(sum += chsman[row][col] == PEACE){
                return PEACE;
            }
        }
    }
    return 0;
}
void chess(int fd_stdin,int fd_stdout)
{
    dsply();

    printf("Please choose players model 1 or computer model 2 :");
    //int player;
    //scanf("%d", &player);

		char player[80]={0};
		int r = read(fd_stdin, player, 80);
		player[r] = 0;
    
           
    do{
        input(fd_stdin,fd_stdout);
        
        if(player[0] == '1') input2(fd_stdin,fd_stdout);
        else if (player[0] == '2') judge();
        else{
            printf("Sorry! You input wrong number,Please type in again！");
            continue;
        }
        if(chkWin() == WIN) break;
        if(chkWin() == UNWIN) break;
        if(stepFlg == 5 && chkPeace() == PEACE){
            printf("Peace!");
            return 0;
        }
    }while(1);
    
    if(chkWin() == WIN)
        printf("二号玩家胜利！ ");
    if(chkWin() == UNWIN)
        printf("一号玩家胜利！ ");
    return 0;
}



/*****************************************************************************
 *                                panic
 *****************************************************************************/
PUBLIC void panic(const char *fmt, ...)
{
	int i;
	char buf[256];

	/* 4 is the size of fmt in the stack */
	va_list arg = (va_list)((char*)&fmt + 4);

	i = vsprintf(buf, fmt, arg);

	printl("%c !!panic!! %s", MAG_CH_PANIC, buf);

	/* should never arrive here */
	__asm__ __volatile__("ud2");
}

