/**************************************************************************
 * File name: client.c
 * Description:This file realize the client of the multiplayer online game.
 * Author: Zeyuan Qiu
 * Version: 1.0
 * Date: 2020/01/07
**************************************************************************/

#include <curses.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
/* 72-2，上边界 */
#define LINE1 "-----------------------------------------------------------------------"
#define LINE2 "|"

/* 下边界 */
#define X 32
/* 游戏区域内有七个随机产生的飞行轨道，Y1-Y7为相对于原点（游戏区域左上角）的横向偏移量 */
#define Y1 15
#define Y2 25
#define Y3 35
#define Y4 45
#define Y5 55
#define Y6 65
#define Y7 75

/* 玩家号，为1或2 */
int player;
/* 游戏运行状态 */
int run;

/* 实时更新分数（擦除原分数，重写新分数）
   i为我方分数，j为对手分数*/
void erase_drawScore(int i, int j)
{
	char temp1[8];
	char temp2[8];
	memset(temp1, '\0', sizeof(temp1));
	sprintf(temp1, "%d", i);
	move(41, 22);
	addstr("  ");
	move(41, 22);
	addstr(temp1);
	memset(temp2, '\0', sizeof(temp2));
	sprintf(temp2, "%d", j);
	move(41, 78);
	addstr("  ");
	move(41, 78);
	addstr(temp2);
	move(35, 80);
	refresh();
}

/* 在指定坐标绘制子弹 */
void drawBullet(int i, int j)
{
	mvaddstr(i, j, "o");
	move(35, 80);
	refresh();
}

/* 擦除原位置的子弹 */
void eraseBullet(int i, int j)
{
	mvaddstr(i, j, " ");
	move(35, 80);
	refresh();
}

/* 在指定位置绘制飞行物 */
void drawBox(int i, int j)
{
	j = j - 2;
	mvaddstr(i++, j, " -----");
	mvaddstr(i++, j, "|     |");
	mvaddstr(i++, j, " -----");
	move(35, 80);
	refresh();
}

/* 擦除指定位置的飞行物 */
void eraseBox(int i, int j)
{
	j = j - 2;
	mvaddstr(i++, j, "      ");
	mvaddstr(i++, j, "       ");
	mvaddstr(i++, j, "      ");
	move(35, 80);
	refresh();
}

/* 在指定位置绘制控制物 */
void drawPlane(int i, int j)
{
	j = j - 3;
	mvaddstr(i++, j, "   |");
	mvaddstr(i++, j, "--| |--");
	mvaddstr(i++, j, "  \\ /");
	move(35, 80);
	refresh();
}

/* 擦除指定位置的控制物 */
void erasePlane(int i, int j)
{
	j = j - 3;
	mvaddstr(i++, j, "    ");
	mvaddstr(i++, j, "       ");
	mvaddstr(i++, j, "      ");
	move(35, 80);
	refresh();
}

/* 游戏开始前准备 */
void pregame(char *room, char *welcome)
{
	int i;
	/* 初始化屏幕 */
	initscr();
	/* 用户输入字符不回显 */
	noecho();
	clear();

	move(0, 30);
	/* 屏幕上绘制游戏名及版权 */
	addstr("MOVES LIKE JAGGER (copyright©Zeyuan Qiu)");
	/* 判断当前玩家是该房间的第一个玩家还是第二个玩家，设置玩家号 */
	if (strcmp(welcome, "Please wait another player...") == 0)
		player = 1;
	else
		player = 2;

	move(2, 5);
	/* 绘制房间号 */
	addstr(room);
	move(3, 5);
	/* 绘制欢迎信息 */
	addstr(welcome);

	move(5, 10);
	/* 绘制上边界 */
	addstr(LINE1);
	move(35, 10);
	/* 绘制下边界 */
	addstr(LINE1);

	for (i = 6; i < 35; i++)
	{
		move(i, 10);
		/* 循环绘制左边界 */
		addstr(LINE2);
		move(i, 80);
		/* 循环绘制右边界 */
		addstr(LINE2);
	}

	/* 初始化当前终端支持的所有颜色 */
	start_color();

	/* 刷新屏幕得以显示 */
	refresh();
}

void playgame(int tcp_socket, int id, char *buf)
{
	char msg[10];
	char c;
	/* 循环获取用户的输入 */
	while (1)
	{
		memset(msg, '\0', sizeof(msg));
		c = getch();
		/* run为0表示游戏退出，界面也退出 */
		if (0 == run)
		{
			usleep(500000);
			break;
		}
		/* 用户输入为a表示向左移动 */
		if (c == 'a')
		{
			/* 根据自定义协议拼接字符串，1表示移动信息 */
			sprintf(msg, "1-%d-l", player);
			if ((send(tcp_socket, msg, sizeof(msg), 0)) == -1)
			{
				perror("[ERROR] Send error");
				exit(0);
			}
			refresh();
		}
		/* 用户输入为d表示向右移动 */
		else if (c == 'd')
		{
			/* 根据自定义协议拼接字符串，1表示移动信息 */
			sprintf(msg, "1-%d-r", player);
			if ((send(tcp_socket, msg, sizeof(msg), 0)) == -1)
			{
				perror("[ERROR] Send error");
				exit(0);
			}
		}
		/* 用户输入为空格表示发射子弹 */
		else if (c == ' ')
		{
			/* 根据自定义协议拼接字符串，2表示子弹信息 */
			sprintf(msg, "2-%d-b", player);
			if ((send(tcp_socket, msg, sizeof(msg), 0)) == -1)
			{
				perror("[ERROR] Send error");
				exit(0);
			}
		}
		/* 用户输入为q表示退出游戏 */
		else if (c == 'q')
		{
			/* 根据自定义协议拼接字符串，3表示退出游戏信息 */
			sprintf(msg, "3-%d-q", player);
			if ((send(tcp_socket, msg, sizeof(msg), 0)) == -1)
			{
				perror("[ERROR] Send error");
				exit(0);
			}
			break;
		}
		refresh();
	}
}

/* 根据获取的字符串序列进行解析，刷新屏幕 */
void *refresh_screen(int *arg)
{
	/* 存储从服务器读取的序列 */
	char msg[1024];
	char temp[1024];
	int tcp_socket = *((int *)arg);
	/* 两个控制物的Y轴坐标 */
	int y1 = 25, y2 = 65;
	char *dest_str;
	dest_str = (char *)malloc(sizeof(char) * 6);
	/* 两个玩家的退出状态 */
	int quit1, quit2;
	/* 14个飞行物的X轴坐标 */
	int x11 = 37, x12 = 37, x13 = 37, x14 = 37, x15 = 37, x16 = 37, x17 = 37;
	int x21 = 37, x22 = 37, x23 = 37, x24 = 37, x25 = 37, x26 = 37, x27 = 37;
	/* 七组飞行物的X轴坐标 */
	int arr1[] = {x11, x21};
	int arr2[] = {x12, x22};
	int arr3[] = {x13, x23};
	int arr4[] = {x14, x24};
	int arr5[] = {x15, x25};
	int arr6[] = {x16, x26};
	int arr7[] = {x17, x27};
	/* 四枚子弹的坐标 */
	int bx11, by11, bx12, by12;
	int bx21, by21, bx22, by22;
	/* 两个玩家的分数 */
	int y1score, y2score;
	/* 两个玩家的输赢标志 */
	int y1winflag, y2winflag;

	while (1)
	{
		memset(msg, '\0', sizeof(msg));
		memset(temp, '\0', sizeof(temp));
		/* 擦除原位置的控制物 */
		erasePlane(X, y1);
		erasePlane(X, y2);
		/* 擦除原位置的飞行物 */
		eraseBox(x11, Y1);
		eraseBox(x12, Y2);
		eraseBox(x13, Y3);
		eraseBox(x14, Y4);
		eraseBox(x15, Y5);
		eraseBox(x16, Y6);
		eraseBox(x17, Y7);
		eraseBox(x21, Y1);
		eraseBox(x22, Y2);
		eraseBox(x23, Y3);
		eraseBox(x24, Y4);
		eraseBox(x25, Y5);
		eraseBox(x26, Y6);
		eraseBox(x27, Y7);
		/* 擦除原位置的子弹 */
		eraseBullet(bx11, by11);
		eraseBullet(bx12, by12);
		eraseBullet(bx21, by21);
		eraseBullet(bx22, by22);

		/* 从服务器读取字符串序列 */
		int n = read(tcp_socket, msg, sizeof(msg));
		strncpy(dest_str, msg, sizeof(msg));

		/* 解析字符串序列 */
		sscanf(dest_str, "%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d\
			-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d",
			/* 两个控制物的Y轴坐标 */
			&y1, &y2,
			/* 两个玩家的退出状态 */
			&quit1, &quit2,
			/* 14个飞行物的X轴坐标 */
			&x11, &x12, &x13, &x14, &x15, &x16, &x17,
			&x21, &x22, &x23, &x24, &x25, &x26, &x27,
			/* 四枚子弹的坐标 */
			&bx11, &by11, &bx12, &by12,
			&bx21, &by21, &bx22, &by22,
			/* 两个玩家的分数 */
			&y1score, &y2score,
			/* 两个玩家的输赢标志 */
			&y1winflag, &y2winflag);

		/* 将14个飞行物的X轴坐标放入数组中 */
		arr1[0] = x11;
		arr1[1] = x21;
		arr2[0] = x12;
		arr2[1] = x22;
		arr3[0] = x13;
		arr3[1] = x23;
		arr4[0] = x14;
		arr4[1] = x24;
		arr5[0] = x15;
		arr5[1] = x25;
		arr6[0] = x16;
		arr6[1] = x26;
		arr7[0] = x17;
		arr7[1] = x27;

		int k;
		/* 两个循环，分别为同一飞行轨道内的第一个（若存在）或第二个（若存在）的飞行物的绘制 */
		for (k = 0; k < 2; k++)
		{
			/* 飞行物不存在 */
			if (arr1[k] == 15)
				arr1[k] = 37;
			else
			{
				/* 飞行物存在，绘制飞行物 */
				arr1[k] = arr1[k] - 10;
				drawBox(arr1[k], Y1);
			}
			/* 飞行物不存在 */
			if (arr2[k] == 15)
				arr2[k] = 37;
			else
			{
				/* 飞行物存在，绘制飞行物 */
				arr2[k] = arr2[k] - 10;
				drawBox(arr2[k], Y2);
			}
			/* 飞行物不存在 */
			if (arr3[k] == 15)
				arr3[k] = 37;
			else
			{
				/* 飞行物存在，绘制飞行物 */
				arr3[k] = arr3[k] - 10;
				drawBox(arr3[k], Y3);
			}
			/* 飞行物不存在 */
			if (arr4[k] == 15)
				arr4[k] = 37;
			else
			{
				/* 飞行物存在，绘制飞行物 */
				arr4[k] = arr4[k] - 10;
				drawBox(arr4[k], Y4);
			}
			/* 飞行物不存在 */
			if (arr5[k] == 15)
				arr5[k] = 37;
			else
			{
				/* 飞行物存在，绘制飞行物 */
				arr5[k] = arr5[k] - 10;
				drawBox(arr5[k], Y5);
			}
			/* 飞行物不存在 */
			if (arr6[k] == 15)
				arr6[k] = 37;
			else
			{
				/* 飞行物存在，绘制飞行物 */
				arr6[k] = arr6[k] - 10;
				drawBox(arr6[k], Y6);
			}
			/* 飞行物不存在 */
			if (arr7[k] == 15)
				arr7[k] = 37;
			else
			{
				/* 飞行物存在，绘制飞行物 */
				arr7[k] = arr7[k] - 10;
				drawBox(arr7[k], Y7);
			}
		}

		/* 更新14个飞行物的X轴坐标 */
		x11 = arr1[0];
		x12 = arr2[1];
		x13 = arr3[0];
		x14 = arr4[1];
		x15 = arr5[0];
		x16 = arr6[1];
		x17 = arr7[0];
		x21 = arr1[1];
		x22 = arr2[0];
		x23 = arr3[1];
		x24 = arr4[0];
		x25 = arr5[1];
		x26 = arr6[0];
		x27 = arr7[1];

		/* 绘制两个玩家的控制物 */
		drawPlane(X, y1);
		drawPlane(X, y2);

		/* 绘制子弹 */
		/* 子弹存在 */
		if (bx11 != 43)
		{
			/* 绘制子弹 */
			bx11 = bx11 - 10;
			drawBullet(bx11, by11);
		}
		/* 子弹不存在 */
		else
		{
			/* 子弹更新到指定位置，不显示 */
			bx11 = 41;
			by11 = 0;
		}
		if (bx12 != 43)
		{
			bx12 = bx12 - 10;
			drawBullet(bx12, by12);
		}
		else
		{
			bx12 = 41;
			by12 = 0;
		}
		if (bx21 != 43)
		{
			bx21 = bx21 - 10;
			drawBullet(bx21, by21);
		}
		else
		{
			bx21 = 41;
			by21 = 0;
		}
		if (bx22 != 43)
		{
			bx22 = bx22 - 10;
			drawBullet(bx22, by22);
		}
		else
		{
			bx22 = 41;
			by22 = 0;
		}

		/* 更新分数 */
		if (1 == player)
		{
			/* 更新玩家1界面的分数，第一个参数是自己的分数，第二个参数是对手的分数 */
			erase_drawScore(y1score - 10, y2score - 10);
		}
		else
		{
			/* 更新玩家2界面的分数，第一个参数是自己的分数，第二个参数是对手的分数 */
			erase_drawScore(y2score - 10, y1score - 10);
		}

		/* 查看是否有玩家退出 */
		if (1 == player)
		{
			/* 对方退出 */
			if (1 == quit1)
			{
				run = 0;
				move(36, 30);
				addstr("Your opponent has leaved...");
				refresh();
				break;
			}
		}
		else if (2 == player)
		{
			/* 对方退出 */
			if (1 == quit2)
			{
				run = 0;
				move(36, 30);
				addstr("Your opponent has leaved...");
				refresh();
				break;
			}
		}

		/* 有飞行物撞击底线，游戏结束 */
		if (2 == quit2)
		{
			run = 0;
			move(36, 40);
			addstr("GAME OVER!");
			/* 玩家1处判断游戏输赢 */
			if (1 == player)
			{
				/* 玩家1输掉游戏 */
				if (0 == y1winflag)
				{
					move(37, 39);
					addstr("YOU LOSE THE GAME...");
				}
				/* 玩家1赢得游戏 */
				else if (1 == y1winflag)
				{
					move(37, 38);
					addstr("YOU WIN THE GAME!");
				}
				/* 平局 */
				else
				{
					move(37, 33);
					addstr("YOU DRAW WITH EACH OTHER...");
				}
			}
			/* 玩家2处判断游戏输赢 */
			else
			{
				/* 玩家2输掉游戏 */
				if (0 == y2winflag)
				{
					move(37, 39);
					addstr("YOU LOSE THE GAME...");
				}
				/* 玩家2赢得游戏 */
				else if (1 == y2winflag)
				{
					move(37, 38);
					addstr("YOU WIN THE GAME!");
				}
				/* 平局 */
				else
				{
					move(37, 39);
					addstr("YOU DRAW WITH EACH OTHER...");
				}
			}

			refresh();
			break;
		}
		refresh();

		/* 不可写sleep(0.5) */
		usleep(100000);
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

	run = 1;

	/* 忽略进程终止和退出信号 */
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	/* 创建专用socket地址用于建立连接 */
	struct sockaddr_in addr;
	int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_port = htons(atoi(argv[2]));

	if (connect(tcp_socket, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
	{
		perror("[ERROR] Cannot connect");
		exit(1);
	}

	char buf[1024];
	char room[1024];
	char welcome[1024];
	char c_id[1024];
	char player[1024];
	char string[1024];

	memset(buf, '\0', sizeof(buf));
	memset(c_id, '\0', sizeof(c_id));
	memset(room, '\0', sizeof(room));
	memset(welcome, '\0', sizeof(welcome));

	/* 从服务器读取字符串序列 */
	int n = read(tcp_socket, buf, sizeof(buf));

	strncpy(c_id, buf, 1);
	strncpy(room, buf + 2, 6);
	strncpy(welcome, buf + 8, 30);
	int id = atoi(c_id);

	/* 游戏开始前准备 */
	pregame(room, welcome);
	
	/* 服务端的player_create阻塞，该部分为向其发送准备确认数据 */
	char *ok = "ok";
	if ((send(tcp_socket, ok, strlen(ok), 0)) == -1)
	{
		perror("[ERROR] Send error...");
		exit(1);
	}

	memset(buf, '\0', sizeof(buf));
	n = read(tcp_socket, buf, sizeof(buf));

	move(3, 5);
	/* 清空准备信息 */
	addstr("                              ");
	move(3, 5);
	/* 绘制游戏开始信息 */
	addstr("Game is beginning...");
	sprintf(player, "      You are the player %d", id - 3);
	addstr(player);
	move(41, 10);
	/* 绘制分数信息 */
	addstr("Your score:");
	move(41, 22);
	addstr("0");
	move(41, 55);
	addstr("Your opponent's score:");
	move(41, 78);
	addstr("0");
	/* 绘制初始位置的控制物 */
	drawPlane(32, 65);
	refresh();

	strcpy(string, buf);
	pthread_t t1;
	/* 创建线程以定时刷新屏幕 */
	pthread_create(&t1, NULL, (void *)refresh_screen, &tcp_socket);
	/* 开始游戏 */
	playgame(tcp_socket, id, string);
	/* 关闭游戏界面 */
	endwin();
	/* 关闭连接 */
	close(tcp_socket);

	return 0;
}
