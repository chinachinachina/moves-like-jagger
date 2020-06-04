/***************************************************************************
 * File name: server.c
 * Description: This file realize the server of the multiplayer online game.
 * Author: Zeyuan Qiu
 * Version: 1.0
 * Date: 2020/01/07
***************************************************************************/

#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

/* 游戏区域内有七个随机产生的飞行轨道，Y1-Y7为相对于原点（游戏区域左上角）的横向偏移量 */
#define Y1 15
#define Y2 25
#define Y3 35
#define Y4 45
#define Y5 55
#define Y6 65
#define Y7 75

/* 虚线方框内为游戏区域，源代码中X为纵轴与Y为横轴
 * 每一个Y为一个飞行轨道， 每个飞行轨道从区域上方边界随机产生向下移动的飞行物，同一时刻同一轨道中至多存在两个飞行物，即0、1或2个
 * 两个玩家的控制体在游戏区域下边界，可左右移动，点击空格可向上发射子弹，子弹会向上方匀速移动，集中飞行块或者上边界，子弹消失
 * 若某一玩家已发射两枚子弹，这两枚子弹若未击中飞行物或上边界，即存在与游戏区域中，该玩家不可发射第3枚子弹，需要等到2枚子弹中其中一枚消失
 * 任意一个飞行物接触到下边界，游戏结束，分数高的玩家获胜

	 O Y1 Y2 Y3 Y4 Y5 Y6 Y7
	 ------------------------			上边界
	x|                      |
	x|                      |
	   .  .  .  .  .  .  .  |
	   .  .  .  .  .  .  .	|			省略部分
	   .  .  .  .  .  .  .  |
	x|      ⬆        ⬆      |			下边界，存在两个玩家控制的控制体

*/

/* 游戏图信息结构体 */
struct map
{
	/* y1为玩家1控制的子弹射出水平坐标 */
	int y1;
	/* y2为玩家2控制的子弹射出水平坐标 */
	int y2;
	/* quit1为玩家1的游戏状态，值为1表示玩家1退出*/
	int quit1;
	/* quit2为玩家2的游戏状态，值为1表示玩家2退出*/
	int quit2;
	/* 玩家1的分数 */
	int y1score;
	/* 玩家2的分数 */
	int y2score;
	/* 玩家1是否赢得该局游戏 */
	int y1winflag;
	/* 玩家2是否赢得该局游戏 */
	int y2winflag;
};

/* 7个飞行轨道内各产生至多两个飞行物，如Y1轨道内两个飞行物（若存在）的坐标为（Y1, x11）及（Y1, X21） */
struct box
{
	int x11, x12, x13, x14, x15, x16, x17;
	int x21, x22, x23, x24, x25, x26, x27;
};

/* 在同一时刻的游戏区域内最多存在2*2颗子弹，如玩家1射出的两枚子弹（若存在）的坐标为（x11, y11）及（x12, y12） */
struct bullet
{
	int x11, y11, x12, y12;
	int x21, y21, x22, y22;
};

/* 向玩家发送游戏准备信息 */
void welcome(int fd, int num, int flag)
{
	char str[1025];
	/* 向序列中增加房间号 */
	sprintf(str, "%d ROOM %d", fd, num);
	char *msg;
	/* 若该连接为玩家1，则需要等待玩家2 */
	if (flag == 1)
		msg = "Please wait another player...";
	/* 若该连接为玩家2，则游戏开始 */
	else
		msg = "Game is beginning...";
	strcat(str, msg);
	/* 将准备信息发送给玩家 */
	if (send(fd, str, strlen(str), 0) == -1)
	{
		perror("[ERROR] Send error");
		exit(1);
	}
}

/* 创建玩家 */
void *player_create(int *arg)
{
	char buf[1024];
	memset(buf, '\0', sizeof(buf));
	int connfd = *arg;
	int n = read(connfd, buf, sizeof(buf));
}

/* 确认玩家是否准备完成，向玩家发送一个字符进行流程确认 */
void *player_ready(int *arg)
{
	int connfd = *arg;
	char *str = ".";
	/* 唤醒客户端阻塞的read函数 */
	if ((send(connfd, str, strlen(str), 0)) == -1)
	{
		perror("[ERROR] Send error");
		exit(1);
	}
}

/* 使用自定义协议解析、读取玩家操作信息 */
void *player_op(int *arg)
{
	/* 连接fd */
	int connfd = arg[0];
	/* 玩家号 */
	int num = arg[1];
	/* 玩家的操作行为产生的序列（自定义序列规则/协议） */
	char buf[1024];
	/* 得到共享内存标识符 */
	int seg_id = shmget(50 + num, 1024, 0777);
	/* 存储游戏区域内的信息 */
	struct map *mymap = shmat(seg_id, NULL, 0);
	/* 得到共享内存标识符 */
	int seg_id2 = shmget(150 + num, 1024, 0777);
	/* 存储玩家的子弹坐标 */
	struct bullet *mybullet = shmat(seg_id2, NULL, 0);

	while (1)
	{
		memset(buf, '\0', sizeof(buf));
		/* 读取玩家的操作信息 */
		read(connfd, buf, sizeof(buf));
		/* 协议定义：buf[0]为1时，下面的数值含义为玩家执行的移动操作的指令 */
		if (buf[0] == '1')
		{
			/* 协议定义：buf[2]为1时，为玩家1的操作 */
			if (buf[2] == '1')
			{
				/* 协议定义： buf[4]为l时，表示玩家坐标向左移动1个单位*/
				if (buf[4] == 'l')
				{
					/* 在实际移动前，需要判断是否越过左边界 */
					if (mymap->y1 >= 15)
						mymap->y1 = mymap->y1 - 1;
				}
				/* 协议定义： buf[4]为r时，表示玩家坐标向左移动1个单位*/
				else if (buf[4] == 'r')
				{
					/* 在实际移动前，需要判断是否越过右边界 */
					if (mymap->y1 <= 75)
						mymap->y1 = mymap->y1 + 1;
				}
			}
			/* 协议定义：buf[2]不为1时，为玩家2的操作 */
			else
			{
				/* 协议定义： buf[4]为l时，表示玩家坐标向左移动1个单位*/
				if (buf[4] == 'l')
				{
					/* 在实际移动前，需要判断是否越过左边界 */
					if (mymap->y2 >= 15)
						mymap->y2 = mymap->y2 - 1;
				}
				/* 协议定义： buf[4]为r时，表示玩家坐标向左移动1个单位*/
				else if (buf[4] == 'r')
				{
					/* 在实际移动前，需要判断是否越过右边界 */
					if (mymap->y2 <= 75)
						mymap->y2 = mymap->y2 + 1;
				}
			}
		}
		/* 协议定义：buf[0]为2时，为玩家发射子弹 */
		else if (buf[0] == '2')
		{
			/* 协议定义：buf[2]为1时，为玩家1的操作 */
			if (buf[2] == '1')
			{
				/* 若该玩家第一枚子弹未发射（x轴坐标为43），则发射（x轴坐标为42，向上移动） */
				if (mybullet->x11 == 43)
				{
					mybullet->x11 = 42;
					mybullet->y11 = mymap->y1;
				}
				/* 若该玩家第二枚子弹未发射（x轴坐标为43），则发射（x轴坐标为42，向上移动） */
				else if (mybullet->x12 == 43)
				{
					mybullet->x12 = 42;
					mybullet->y12 = mymap->y1;
				}
			}
			/* 协议定义：buf[2]不为1时，为玩家2的操作 */
			else
			{
				/* 若该玩家第一枚子弹未发射（x轴坐标为43），则发射（x轴坐标为42，向上移动） */
				if (mybullet->x21 == 43)
				{
					mybullet->x21 = 42;
					mybullet->y21 = mymap->y2;
				}
				/* 若该玩家第二枚子弹未发射（x轴坐标为43），则发射（x轴坐标为42，向上移动） */
				else if (mybullet->x22 == 43)
				{
					mybullet->x22 = 42;
					mybullet->y22 = mymap->y2;
				}
			}
		}
		/* 协议定义：buf[0]为3时，后面的数据表示有玩家退出 */
		else if (buf[0] == '3')
		{
			/* 玩家1主动退出 */
			if (buf[2] == '1')
				mymap->quit2 = 1;
			/* 玩家2主动退出 */
			if (buf[2] == '2')
				mymap->quit1 = 1;
			printf("[INFO] A client leaves...\n");
			printf("[INFO] Another client leaves...\n");
		}
	}
}

/* 循环读取共享内存中的数据，按照协议格式将其转换为字符串，分别发送至两个玩家的客户端 */
void *player_update(int *arg)
{
	/* 两个玩家客户端fd */
	int connfd1 = arg[0];
	int connfd2 = arg[1];
	int num = arg[2];
	int seg_id = shmget(50 + num, 1024, 0777);
	/* 存储地图中坐标信息 */
	struct map *mymap = shmat(seg_id, NULL, 0);
	int seg_id1 = shmget(100 + num, 1024, 0777);
	/* 存储移动飞行块坐标信息 */
	struct box *mybox = shmat(seg_id1, NULL, 0);
	int seg_id2 = shmget(150 + num, 1024, 0777);
	/* 存储玩家的子弹坐标 */
	struct bullet *mybullet = shmat(seg_id2, NULL, 0);
	char str[1024];
	while (1)
	{
		memset(str, '\0', sizeof(str));
		/* 格式化两个玩家的控制体的Y轴坐标、两个玩家是否退出游戏信息、
		2*7个移动方块的X轴坐标、2*2个子弹的坐标、两个玩家的分数和比赛结果 */
		sprintf(str, "%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d",
				/* 两个玩家的控制体的Y轴坐标 */
				mymap->y1, mymap->y2,
				/* 两个玩家是否退出游戏信息 */
				mymap->quit1, mymap->quit2,
				/* 2*7个移动方块的X轴坐标 */
				mybox->x11, mybox->x12, mybox->x13, mybox->x14, mybox->x15, mybox->x16, mybox->x17,
				mybox->x21, mybox->x22, mybox->x23, mybox->x24, mybox->x25, mybox->x26, mybox->x27,
				/* 2*2个子弹的坐标 */
				mybullet->x11, mybullet->y11, mybullet->x12, mybullet->y12,
				mybullet->x21, mybullet->y21, mybullet->x22, mybullet->y22,
				/* 两个玩家的分数 */
				mymap->y1score, mymap->y2score,
				/* 两个玩家的比赛结果 */
				mymap->y1winflag, mymap->y2winflag);

		/* 向第一个玩家发送格式化信息 */
		if ((send(connfd1, str, sizeof(str), 0)) == -1)
		{
			perror("send error");
			exit(1);
		}
		/* 向第二个玩家发送格式化信息 */
		if ((send(connfd2, str, sizeof(str), 0)) == -1)
		{
			perror("send error");
			exit(1);
		}
		usleep(100000);
	}
}

/* 控制障碍物生成，子弹生成，障碍物移动，子弹移动，子弹与障碍物碰撞判断 */
void *controller(int *arg)
{
	int num = *arg;
	int seg_id = shmget(100 + num, 1024, 0777);
	/* 飞行物坐标 */
	struct box *mybox = shmat(seg_id, NULL, 0);
	int seg_id1 = shmget(150 + num, 1024, 0777);
	/* 子弹坐标 */
	struct bullet *mybullet = shmat(seg_id1, NULL, 0);
	int seg_id2 = shmget(50 + num, 1024, 0777);
	/* 地图信息 */
	struct map *mymap = shmat(seg_id2, NULL, 0);
	int i;
	/* 14个飞行物的X轴坐标 */
	int x11 = mybox->x11, x12 = mybox->x12, x13 = mybox->x13, x14 = mybox->x14, x15 = mybox->x15, x16 = mybox->x16, x17 = mybox->x17;
	int x21 = mybox->x21, x22 = mybox->x22, x23 = mybox->x23, x24 = mybox->x24, x25 = mybox->x25, x26 = mybox->x26, x27 = mybox->x27;
	/* arr数组为14个物块的X轴坐标 */
	int arr[] = {x11, x12, x13, x14, x15, x16, x17, x21, x22, x23, x24, x25, x26, x27};
	/* 循环计数标志，初始值设置为9，在下面的循环中每个循环+1，若j==10（表示过去五秒），则执行随机生成3个飞行物的操作 */
	int j = 9;
	/* 4个子弹坐标 */
	int bx11, by11, bx12, by12;
	int bx21, by21, bx22, by22;
	while (1)
	{
		/* 子弹x11（玩家1的第1发子弹）坐标控制，若x轴坐标非43，说明子弹已经发射并向上运动，需要将子弹坐标-1 */
		if (mybullet->x11 != 43)
		{
			mybullet->x11 = mybullet->x11 - 1;
			/* 若子弹坐标到达16，则说明到达游戏区域上届，将其坐标重置回43 */
			if (mybullet->x11 == 16)
				mybullet->x11 = 43;
		}
		/* 子弹x12（玩家1的第2发子弹）坐标控制，若x轴坐标非43，说明子弹已经发射并向上运动，需要将子弹坐标-1 */
		if (mybullet->x12 != 43)
		{
			mybullet->x12 = mybullet->x12 - 1;
			if (mybullet->x12 == 16)
				mybullet->x12 = 43;
		}
		/* 子弹x21（玩家2的第1发子弹）坐标控制，若x轴坐标非43，说明子弹已经发射并向上运动，需要将子弹坐标-1 */
		if (mybullet->x21 != 43)
		{
			mybullet->x21 = mybullet->x21 - 1;
			if (mybullet->x21 == 16)
				mybullet->x21 = 43;
		}
		/* 子弹x22（玩家2的第2发子弹）坐标控制，若x轴坐标非43，说明子弹已经发射并向上运动，需要将子弹坐标-1 */
		if (mybullet->x22 != 43)
		{
			mybullet->x22 = mybullet->x22 - 1;
			if (mybullet->x22 == 16)
				mybullet->x22 = 43;
		}

		/* 子弹坐标存储 */
		bx11 = mybullet->x11;
		by11 = mybullet->y11;
		bx12 = mybullet->x12;
		by12 = mybullet->y12;
		bx21 = mybullet->x21;
		by21 = mybullet->y21;
		bx22 = mybullet->x22;
		by22 = mybullet->y22;

		/* 对14个飞行物（若存在）依次进行与子弹是否碰撞的判断 */
		for (i = 0; i < 14; i++)
		{
			/* arr[i] != 15 表示该飞行物不在X轴为15的点，即不在游戏区域上届，即物块已经存在于游戏区域内并运动 */
			if (arr[i] != 15)
			{
				/* 物块在X轴方向坐标+1，即向下运动一个单位 */
				arr[i] = arr[i] + 1;
				/* i%7 == 0 表示该物块X轴坐标为arr[i]，Y轴坐标为Y1，即Y1轨道上的飞行物 */
				if (i % 7 == 0)
				{
					/* 将该飞行物的X轴坐标与第1枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					if (abs(arr[i] - bx11) <= 3)
					{
						/* 该飞行物的Y轴坐标与第1枚子弹进行距离判断，若子弹Y轴距离Y1偏差小于3，则表示子弹与飞行物碰撞 */
						if (by11 <= Y1 + 3 && by11 >= Y1 - 3)
						{
							/* 重置该飞行物X轴坐标为15，即上边界，即在游戏区域内消失 */
							arr[i] = 15;
							/* 重置该子弹X轴坐标为43，即下边界，即在游戏区域内消失 */
							mybullet->x11 = 43;
							/* 该子弹与物块碰撞为玩家1操作导致，因此玩家1的分数+1 */
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第2枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx12) <= 3)
					{
						/* 该飞行物的Y轴坐标与第2枚子弹进行距离判断，若子弹Y轴距离Y1偏差小于3，则表示子弹与飞行物碰撞 */
						if (by12 <= Y1 + 3 && by12 >= Y1 - 3)
						{
							arr[i] = 15;
							mybullet->x12 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第3枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx21) <= 3)
					{
						/* 该飞行物的Y轴坐标与第3枚子弹进行距离判断，若子弹Y轴距离Y1偏差小于3，则表示子弹与飞行物碰撞 */
						if (by21 <= Y1 + 3 && by21 >= Y1 - 3)
						{
							arr[i] = 15;
							mybullet->x21 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第4枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx22) <= 3)
					{
						/* 该飞行物的Y轴坐标与第4枚子弹进行距离判断，若子弹Y轴距离Y1偏差小于3，则表示子弹与飞行物碰撞 */
						if (by22 <= Y1 + 3 && by22 >= Y1 - 3)
						{
							arr[i] = 15;
							mybullet->x22 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
				}
				/* i%7 == 1 表示该物块X轴坐标为arr[i]，Y轴坐标为Y2，即Y2轨道上的飞行物 */
				else if (i % 7 == 1)
				{
					/* 将该飞行物的X轴坐标与第1枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					if (abs(arr[i] - bx11) <= 3)
					{
						/* 该飞行物的Y轴坐标与第1枚子弹进行距离判断，若子弹Y轴距离Y2偏差小于3，则表示子弹与飞行物碰撞 */
						if (by11 <= Y2 + 3 && by11 >= Y2 - 3)
						{
							arr[i] = 15;
							mybullet->x11 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第2枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx12) <= 3)
					{
						/* 该飞行物的Y轴坐标与第2枚子弹进行距离判断，若子弹Y轴距离Y2偏差小于3，则表示子弹与飞行物碰撞 */
						if (by12 <= Y2 + 3 && by12 >= Y2 - 3)
						{
							arr[i] = 15;
							mybullet->x12 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第3枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx21) <= 3)
					{
						/* 该飞行物的Y轴坐标与第3枚子弹进行距离判断，若子弹Y轴距离Y2偏差小于3，则表示子弹与飞行物碰撞 */
						if (by21 <= Y2 + 3 && by21 >= Y2 - 3)
						{
							arr[i] = 15;
							mybullet->x21 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第4枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx22) <= 3)
					{
						/* 该飞行物的Y轴坐标与第4枚子弹进行距离判断，若子弹Y轴距离Y2偏差小于3，则表示子弹与飞行物碰撞 */
						if (by22 <= Y2 + 3 && by22 >= Y2 - 3)
						{
							arr[i] = 15;
							mybullet->x22 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
				}
				/* i%7 == 2 表示该物块X轴坐标为arr[i]，Y轴坐标为Y3，即Y3轨道上的飞行物 */
				else if (i % 7 == 2)
				{
					/* 将该飞行物的X轴坐标与第1枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					if (abs(arr[i] - bx11) <= 3)
					{
						/* 该飞行物的Y轴坐标与第1枚子弹进行距离判断，若子弹Y轴距离Y3偏差小于3，则表示子弹与飞行物碰撞 */
						if (by11 <= Y3 + 3 && by11 >= Y3 - 3)
						{
							arr[i] = 15;
							mybullet->x11 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第2枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx12) <= 3)
					{
						/* 该飞行物的Y轴坐标与第2枚子弹进行距离判断，若子弹Y轴距离Y3偏差小于3，则表示子弹与飞行物碰撞 */
						if (by12 <= Y3 + 3 && by12 >= Y3 - 3)
						{
							arr[i] = 15;
							mybullet->x12 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第3枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx21) <= 3)
					{
						/* 该飞行物的Y轴坐标与第3枚子弹进行距离判断，若子弹Y轴距离Y3偏差小于3，则表示子弹与飞行物碰撞 */
						if (by21 <= Y3 + 3 && by21 >= Y3 - 3)
						{
							arr[i] = 15;
							mybullet->x21 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第4枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx22) <= 3)
					{
						/* 该飞行物的Y轴坐标与第4枚子弹进行距离判断，若子弹Y轴距离Y3偏差小于3，则表示子弹与飞行物碰撞 */
						if (by22 <= Y3 + 3 && by22 >= Y3 - 3)
						{
							arr[i] = 15;
							mybullet->x22 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
				}
				/* i%7 == 3 表示该物块X轴坐标为arr[i]，Y轴坐标为Y4，即Y4轨道上的飞行物 */
				else if (i % 7 == 3)
				{
					/* 将该飞行物的X轴坐标与第1枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					if (abs(arr[i] - bx11) <= 3)
					{
						/* 该飞行物的Y轴坐标与第1枚子弹进行距离判断，若子弹Y轴距离Y4偏差小于3，则表示子弹与飞行物碰撞 */
						if (by11 <= Y4 + 3 && by11 >= Y4 - 3)
						{
							arr[i] = 15;
							mybullet->x11 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第2枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx12) <= 3)
					{
						/* 该飞行物的Y轴坐标与第2枚子弹进行距离判断，若子弹Y轴距离Y4偏差小于3，则表示子弹与飞行物碰撞 */
						if (by12 <= Y4 + 3 && by12 >= Y4 - 3)
						{
							arr[i] = 15;
							mybullet->x12 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第3枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx21) <= 3)
					{
						/* 该飞行物的Y轴坐标与第3枚子弹进行距离判断，若子弹Y轴距离Y4偏差小于3，则表示子弹与飞行物碰撞 */
						if (by21 <= Y4 + 3 && by21 >= Y4 - 3)
						{
							arr[i] = 15;
							mybullet->x21 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第4枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx22) <= 3)
					{
						/* 该飞行物的Y轴坐标与第4枚子弹进行距离判断，若子弹Y轴距离Y4偏差小于3，则表示子弹与飞行物碰撞 */
						if (by22 <= Y4 + 3 && by22 >= Y4 - 3)
						{
							arr[i] = 15;
							mybullet->x22 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
				}
				/* i%7 == 4 表示该物块X轴坐标为arr[i]，Y轴坐标为Y5，即Y5轨道上的飞行物 */
				else if (i % 7 == 4)
				{
					/* 将该飞行物的X轴坐标与第1枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					if (abs(arr[i] - bx11) <= 3)
					{
						/* 该飞行物的Y轴坐标与第1枚子弹进行距离判断，若子弹Y轴距离Y5偏差小于3，则表示子弹与飞行物碰撞 */
						if (by11 <= Y5 + 3 && by11 >= Y5 - 3)
						{
							arr[i] = 15;
							mybullet->x11 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第2枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx12) <= 3)
					{
						/* 该飞行物的Y轴坐标与第2枚子弹进行距离判断，若子弹Y轴距离Y5偏差小于3，则表示子弹与飞行物碰撞 */
						if (by12 <= Y5 + 3 && by12 >= Y5 - 3)
						{
							arr[i] = 15;
							mybullet->x12 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第3枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx21) <= 3)
					{
						/* 该飞行物的Y轴坐标与第3枚子弹进行距离判断，若子弹Y轴距离Y5偏差小于3，则表示子弹与飞行物碰撞 */
						if (by21 <= Y5 + 3 && by21 >= Y5 - 3)
						{
							arr[i] = 15;
							mybullet->x21 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第4枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx22) <= 3)
					{
						/* 该飞行物的Y轴坐标与第4枚子弹进行距离判断，若子弹Y轴距离Y5偏差小于3，则表示子弹与飞行物碰撞 */
						if (by22 <= Y5 + 3 && by22 >= Y5 - 3)
						{
							arr[i] = 15;
							mybullet->x22 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
				}
				/* i%7 == 5 表示该物块X轴坐标为arr[i]，Y轴坐标为Y6，即Y6轨道上的飞行物 */
				else if (i % 7 == 5)
				{
					/* 将该飞行物的X轴坐标与第1枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					if (abs(arr[i] - bx11) <= 3)
					{
						/* 该飞行物的Y轴坐标与第1枚子弹进行距离判断，若子弹Y轴距离Y6偏差小于3，则表示子弹与飞行物碰撞 */
						if (by11 <= Y6 + 3 && by11 >= Y6 - 3)
						{
							arr[i] = 15;
							mybullet->x11 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第2枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx12) <= 3)
					{
						/* 该飞行物的Y轴坐标与第2枚子弹进行距离判断，若子弹Y轴距离Y6偏差小于3，则表示子弹与飞行物碰撞 */
						if (by12 <= Y6 + 3 && by12 >= Y6 - 3)
						{
							arr[i] = 15;
							mybullet->x12 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第3枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx21) <= 3)
					{
						/* 该飞行物的Y轴坐标与第3枚子弹进行距离判断，若子弹Y轴距离Y6偏差小于3，则表示子弹与飞行物碰撞 */
						if (by21 <= Y6 + 3 && by21 >= Y6 - 3)
						{
							arr[i] = 15;
							mybullet->x21 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第4枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx22) <= 3)
					{
						/* 该飞行物的Y轴坐标与第4枚子弹进行距离判断，若子弹Y轴距离Y6偏差小于3，则表示子弹与飞行物碰撞 */
						if (by22 <= Y6 + 3 && by22 >= Y6 - 3)
						{
							arr[i] = 15;
							mybullet->x22 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
				}
				/* i%7 == 6 表示该物块X轴坐标为arr[i]，Y轴坐标为Y7，即Y7轨道上的飞行物 */
				else
				{
					/* 将该飞行物的X轴坐标与第1枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					if (abs(arr[i] - bx11) <= 3)
					{
						/* 该飞行物的Y轴坐标与第1枚子弹进行距离判断，若子弹Y轴距离Y7偏差小于3，则表示子弹与飞行物碰撞 */
						if (by11 <= Y7 + 3 && by11 >= Y7 - 3)
						{
							arr[i] = 15;
							mybullet->x11 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第2枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx12) <= 3)
					{
						/* 该飞行物的Y轴坐标与第2枚子弹进行距离判断，若子弹Y轴距离Y7偏差小于3，则表示子弹与飞行物碰撞 */
						if (by12 <= Y7 + 3 && by12 >= Y7 - 3)
						{
							arr[i] = 15;
							mybullet->x12 = 43;
							mymap->y1score = mymap->y1score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第3枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx21) <= 3)
					{
						/* 该飞行物的Y轴坐标与第3枚子弹进行距离判断，若子弹Y轴距离Y7偏差小于3，则表示子弹与飞行物碰撞 */
						if (by21 <= Y7 + 3 && by21 >= Y7 - 3)
						{
							arr[i] = 15;
							mybullet->x21 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
					/* 将该飞行物的X轴坐标与第4枚子弹进行距离判断，若小于3则表示子弹与飞行物下边界在同一水平线 */
					else if (abs(arr[i] - bx22) <= 3)
					{
						/* 该飞行物的Y轴坐标与第4枚子弹进行距离判断，若子弹Y轴距离Y7偏差小于3，则表示子弹与飞行物碰撞 */
						if (by22 <= Y7 + 3 && by22 >= Y7 - 3)
						{
							arr[i] = 15;
							mybullet->x22 = 43;
							mymap->y2score = mymap->y2score + 1;
						}
					}
				}
			}

			/* arr[i]==40 表示该飞行物已经接触到下边界，游戏结束 */
			if (arr[i] == 40)
			{
				/* 将两个玩家退出状态设置为2，即因游戏设置游戏结束，非玩家主动退出 */
				mymap->quit1 = 2;
				mymap->quit2 = 2;
				/* 判断哪个玩家分数高，其winflag设置为1 */
				if (mymap->y1score > mymap->y2score)
				{
					mymap->y1winflag = 1;
				}
				else if (mymap->y1score < mymap->y2score)
				{
					mymap->y2winflag = 1;
				}
				/* 该情况为两个玩家分数相等，平局 */
				else
				{
					mymap->y1winflag = 2;
					mymap->y2winflag = 2;
				}

				printf("[INFO] A client leaves...\n");
				printf("[INFO] Another client leaves...\n");
			}
		}

		/* 产生随机数，用于飞行物块的随机生成 */
		srand(time(NULL));
		/* 若j==10（已过去0.5*10=5秒）则执行增加1、2或3个随机飞行物的操作 */
		if (j == 10)
		{
			/* 重置j值为0 */
			j = 0;
			/* 产生3个0-13的随机数 */
			int rand1 = rand() % 14;
			int rand2 = rand() % 14;
			int rand3 = rand() % 14;
			/* 若该飞行物的X轴值为15，说明该飞行物不在游戏区域内，则将其X轴值+1，使其出现在游戏区域中 */
			if (arr[rand1] == 15)
			{
				arr[rand1] = arr[rand1] + 1;
			}
			/* 若该飞行物的X轴值为15，说明该飞行物不在游戏区域内，rand1!=rand2则保证不会再同一个时刻同一个地方产生两个飞行物 */
			if ((arr[rand2] == 15) && (rand1 != rand2))
				arr[rand2] = arr[rand2] + 1;
			/* 若该飞行物的X轴值为15，说明该飞行物不在游戏区域内
			(rand1 != rand3) && (rand2 != rand3)则保证不会再同一个时刻同一个地方产生两个飞行物 */
			if ((arr[rand3] == 15) && (rand1 != rand3) && (rand2 != rand3))
				arr[rand3] = arr[rand3] + 1;
		}

		/* 更新14个飞行物的X轴坐标 */
		mybox->x11 = arr[0];
		mybox->x12 = arr[1];
		mybox->x13 = arr[2];
		mybox->x14 = arr[3];
		mybox->x15 = arr[4];
		mybox->x16 = arr[5];
		mybox->x17 = arr[6];
		mybox->x21 = arr[7];
		mybox->x22 = arr[8];
		mybox->x23 = arr[9];
		mybox->x24 = arr[10];
		mybox->x25 = arr[11];
		mybox->x26 = arr[12];
		mybox->x27 = arr[13];

		/* 更新计数值j */
		j++;

		/* 睡眠0.5秒 */
		usleep(500000);
	}
}

int main(int argc, char *argv[])
{
	/* 判断用户输入参数数量是否正确 */
	if (argc <= 1)
	{
		printf("usage: %s port_number \n", basename(argv[0]));
		return 1;
	}

	/* 子进程id集合 */
	int cpid[1000];

	struct sockaddr_in addr;
	int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);

	/* 使用SO_REUSEADDR选项实现TIME_WAIT状态下已绑定的socket地址可以立即被重用 */
	int reuse = 1;
	setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	/* 创建专用socket地址用于接收连接 */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = INADDR_ANY;
	/* 命名socket，与具体的地址address绑定 */
	if (bind(tcp_socket, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
	{
		perror("[ERROR] Cannot bind");
		exit(1);
	}
	/* 监听socket，backlog参数为监听队列长度 */
	listen(tcp_socket, 50);

	/* 当前房间号，每个房间有两个玩家 */
	int num = 0;
	while (1)
	{
		/* 房间号+1 */
		num++;
		/* 房间号超过1000则从1重新计数 */
		num = num % 1000;
		/* 接受玩家1的连接 */
		int connfd1 = accept(tcp_socket, NULL, NULL);
		printf("[INFO] A client arrives...\n");
		/* 向玩家1发送游戏准备信息 */
		welcome(connfd1, num, 1);
		/* 接受玩家2的连接 */
		int connfd2 = accept(tcp_socket, NULL, NULL);
		printf("[INFO] Another client arrives...\n");
		/* 向玩家2发送游戏准备信息 */
		welcome(connfd2, num, 2);
		/* 创建子进程用于处理该房间内事务 */
		if ((cpid[num] = fork()) == 0)
		{
			/* 玩家1的连接fd和房间号 */
			int arr1[2];
			arr1[0] = connfd1;
			arr1[1] = num;
			/* 玩家2的连接fd和房间号 */
			int arr2[2];
			arr2[0] = connfd2;
			arr2[1] = num;
			/* 玩家1、2的连接和房间号 */
			int connfdarr[3];
			connfdarr[0] = connfd1;
			connfdarr[1] = connfd2;
			connfdarr[2] = num;

			/* 初始化游戏区域内的属性值 */
			struct map *mymap;
			/* 分配结构体内存 */
			int seg_id = shmget(50 + num, 1024, IPC_CREAT | 0777);
			mymap = shmat(seg_id, NULL, 0);
			/* 两个玩家的控制体Y轴分别设置为25和65 */
			mymap->y1 = 25;
			mymap->y2 = 65;
			/* 两个玩家的退出状态设置为0 */
			mymap->quit1 = 0;
			mymap->quit2 = 0;
			/* 连哥哥玩家的分数初始值设置为10，后续会减去10 */
			mymap->y1score = 10;
			mymap->y2score = 10;
			/* 两个玩家的胜利标志设置为0 */
			mymap->y1winflag = 0;
			mymap->y2winflag = 0;

			/* 飞行物 */
			int seg_id1 = shmget(100 + num, 1024, IPC_CREAT | 0777);
			struct box *mybox;
			mybox = shmat(seg_id1, NULL, 0);

			/* 初始化14个飞行物的X轴坐标为15，即游戏区域上边界 */
			mybox->x11 = 15;
			mybox->x12 = 15;
			mybox->x13 = 15;
			mybox->x14 = 15;
			mybox->x15 = 15;
			mybox->x16 = 15;
			mybox->x17 = 15;
			mybox->x21 = 15;
			mybox->x22 = 15;
			mybox->x23 = 15;
			mybox->x24 = 15;
			mybox->x25 = 15;
			mybox->x26 = 15;
			mybox->x27 = 15;

			/* 子弹 */
			int seg_id2 = shmget(150 + num, 1024, IPC_CREAT | 0777);
			struct bullet *mybullet;
			mybullet = shmat(seg_id2, NULL, 0);

			/* 初始化4枚子弹的X、Y轴坐标为43，即在游戏区域外，不显示 */
			mybullet->x11 = 43;
			mybullet->y11 = 43;
			mybullet->x12 = 43;
			mybullet->y12 = 43;
			mybullet->x21 = 43;
			mybullet->y21 = 43;
			mybullet->x22 = 43;
			mybullet->y22 = 43;

			pthread_t t1, t2, t3, t4, t5;

			/* 创建两个线程用于用户连接的创建 */
			pthread_create(&t1, NULL, (void *)player_create, &arr1);
			pthread_create(&t2, NULL, (void *)player_create, &arr2);
			pthread_join(t1, NULL);
			pthread_join(t2, NULL);
			/* 该线程运行结束自动释放所有资源 */
			pthread_detach(t1);
			pthread_detach(t2);

			/* 创建两个线程用于游戏前的准备和确认工作 */
			pthread_create(&t1, NULL, (void *)player_ready, &connfd1);
			pthread_create(&t2, NULL, (void *)player_ready, &connfd2);
			pthread_join(t1, NULL);
			pthread_join(t2, NULL);
			/* 该线程运行结束自动释放所有资源 */
			pthread_detach(t1);
			pthread_detach(t2);

			/* 创建两个线程用于接受两个玩家的具体操作（按照协议对字符串进行解析） */
			pthread_create(&t1, NULL, (void *)player_op, arr1);
			pthread_create(&t2, NULL, (void *)player_op, arr2);
			/* 创建线程负责向两个玩家客户端发送最新的游戏区域内所有物体坐标信息的字符串序列 */
			pthread_create(&t3, NULL, (void *)player_update, connfdarr);
			/* 创建线程负责判断内存区域内地图中飞行物、子弹、控制物坐标之间的相对关系，若发生子弹击中飞行物等事件则进行相应处理（主要为坐标的更新） */
			pthread_create(&t4, NULL, (void *)controller, &num);
			pthread_join(t1, NULL);
			pthread_join(t2, NULL);
			pthread_join(t3, NULL);
			pthread_join(t4, NULL);
		}
	}

	/* 关闭负责接收连接的socket */
	close(tcp_socket);

	return 0;
}
