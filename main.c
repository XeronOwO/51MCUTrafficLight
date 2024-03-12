/*******************************
*                              *
*       51单片机课程设计       *
*         倒计时红绿灯         *
*                              *
*******************************/

#include <reg51.h>

// 常用类型定义
typedef unsigned int uint;
typedef unsigned char bool;
typedef unsigned char byte;
#define false 0
#define true 1
#define NULL ((void *)0)

// 声明
void on_light_ready();
void on_light_running();
void on_light_editing();
void on_light_timer();
void on_light_second();
void start_light();
void stop_light();
void edit_light();

/*******************************
*                              *
*           显示功能           *
*                              *
********************************
*                              *
*       P0 连接 LED数码管阵列  *
*  P20~P22 连接 74LS138译码器  *
*                              *
*******************************/

// LED数码管字模
byte Number2LED[16] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};

// LED数码管显示
byte LEDDisplay[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// 循环延时
void delay(uint x)
{
	while (x--);
}

// 设置LED数码管数值
// 参数：
//     index 表示数码管索引，0~7
//     value 表示要显示的数值，可显示范围为0~15，范围以外不显示
void set_display_value(byte index, byte value)
{
	if (0 <= value && value < 16)
	{
		LEDDisplay[index] = Number2LED[value];
	}
	else
	{
		LEDDisplay[index] = 0x00;
	}
}

// 74LS138
sbit LS138A = P2 ^ 0;
sbit LS138B = P2 ^ 1;
sbit LS138C = P2 ^ 2;

// 动态数码管显示
void display()
{
	int i;
	for (i = 0; i < 8; i++) // 从左到右，一共8位
	{
		LS138A = i & 0x01;
		LS138B = (i >> 1) & 0x01;
		LS138C = (i >> 2) & 0x01;
		P0 = LEDDisplay[i]; // 显示数字
		delay(100);
		P0 = 0x00; // 消除影子
	}
}

/*************************
*                        *
*      按键检测功能      *
*                        *
**************************
*                        *
*  P3 连接 独立按键模块  *
*                        *
**************************/

// 上一次的按键
byte LastKey = 0xff;

// 当前按键
byte CurrentKey = 0xff;

// 更新按键，用于按键检测之前
void update_key()
{
	LastKey = CurrentKey;
	CurrentKey = P3;
}

// 按键按下检测
// 参数：
//     i 表示第几个按键，例如 i=1 表示按钮 K1
// 返回：
//     当按下按键时，返回 true ，否则返回 false
bool is_key_pressed(byte i)
{
	byte x = 1 << (i - 1);
	return (((LastKey & x) != 0) && ((CurrentKey & x) == 0));
}

// 按键松开检测
// 参数：
//     i 表示第几个按键，例如 i=1 表示按钮 K1
// 返回：
//     当松开按键时，返回 true ，否则返回 false
bool is_key_released(byte i)
{
	byte x = 1 << (i - 1);
	return (((LastKey & x) == 0) && ((CurrentKey & x) != 0));
}

/***********
*          *
*  定时器  *
*          *
***********/

// 计时器回调函数，会在计时完成后调用
typedef void (*TIMERCALLBACK)();
TIMERCALLBACK TimerCallback;

// 启动计时器
// 参数：
//     interval 时间间隔，范围0~65536
//     callback 回调函数
void start_timer(int interval, TIMERCALLBACK callback)
{
	TMOD = 0x10; //工作于方式1
	EA = 1; // 中断允许
    ET1 = 1; // 中断1打开
	TH1 = (65536 - interval) / 256;
	TL1 = (65536 - interval) % 256;      
	TR1 = 1;
	TimerCallback = callback;
}

// 停止计时器
void stop_timer()
{
	TR1 = 0;
}

// 定时器中断处理
void on_timer() interrupt 3
{
	if (TimerCallback != NULL)
	{
		TimerCallback(); // 计时器回调
	}
}

/*************************
*                        *
*         红绿灯         *
*                        *
**************************
*                        *
*   P1 连接 红绿灯D1~D8  *
*  P25 连接 蜂鸣器       *
*  P26 连接 红绿灯D9     *
*  P27 连接 红绿灯D10    *
*                        *
*************************/

// 按钮设定
// K1 启动/停止红绿灯
#define KEY_START_STOP 1
// K2 切换显示模式
#define KEY_SWITCH_DISPLAY 2
// K3 编辑模式
#define KEY_EDIT 3
// K4 启用/禁用蜂鸣器，蜂鸣器会在绿灯5秒倒计时时响
#define KEY_BUZZER 4
// K5 （启动红绿灯后）东西方向行人按钮
#define KEY_PEDESTRIANS1 5
// K6 （启动红绿灯后）南北方向行人按钮
#define KEY_PEDESTRIANS2 6
// K5 （编辑模式下）增加绿灯时间
#define KEY_ADD 5
// K6 （编辑模式下）减小绿灯时间
#define KEY_SUB 6

// 状态设定
// 就绪
#define LIGHT_READY 0
// 正在运行
#define LIGHT_RUNNING 1
// 编辑模式
#define LIGHT_EDITING 2

// 方向设定
// 东西方向
#define DIR_EAST_WEST 1
// 南北方向
#define DIR_SOUTH_NORTH 2

// 红绿灯状态
byte LightStatus = LIGHT_READY;

// 完整的红绿灯显示功能
#define DISPLAY_FULL 0
// 仅任务书要求的显示功能
#define DISPLAY_SIMPLE 1

// 显示模式
byte DisplayMode = DISPLAY_FULL;

// 蜂鸣器
bool EnableBuzzer = false;
sbit Beep = P2 ^ 5;

// 黄灯时间
const uint YellowTime = 3;
// 最小绿灯时间
const uint GreenTimeMin = 5;
// 最大绿灯时间
const uint GreenTimeMax = 99;
// 绿灯时间，1为东西方向，2为南北方向，0作保留
uint GreenTime[3] = { 0, 10, 10 };

// 循环周期，由4部分组成：绿灯1时间+黄灯时间+绿灯2时间+黄灯时间，然后根据 实际红绿灯时间 进行计算和显示
int LightPeriod;
// 实际红绿灯时间
int LightTime;

// 红绿灯事件处理
void handle_light()
{
	// 事件处理
	switch (LightStatus)
	{
		case LIGHT_READY:
			on_light_ready();
			break;
		case LIGHT_RUNNING:
			on_light_running();
			break;
		case LIGHT_EDITING:
			on_light_editing();
			break;
		default:
			break;
	}
	
	// 其它按键处理
	if (is_key_released(KEY_SWITCH_DISPLAY))
	{
		DisplayMode = 1 - DisplayMode; // 切换显示模式
	}
	if (is_key_released(KEY_BUZZER))
	{
		EnableBuzzer = 1 - EnableBuzzer;
	}
}

// LightTimerCounter 结合 Interval50000 实现1秒钟的计时器
byte LightTimerCounter;
const int Interval50000 = 50000;

// 启动红绿灯
void start_light()
{
	LightStatus = LIGHT_RUNNING;
	LightPeriod = GreenTime[DIR_EAST_WEST] + GreenTime[DIR_SOUTH_NORTH] + YellowTime * 2;
	LightTime = 0;
	LightTimerCounter = 0;
	start_timer(Interval50000, on_light_timer);
}

// 计时器触发
void on_light_timer()
{
	++LightTimerCounter;
	if (LightTimerCounter == 20)
	{
		LightTimerCounter = 0;
		on_light_second();
	}
	start_timer(Interval50000, on_light_timer);
}

// 一秒触发
void on_light_second()
{
	LightTime = (LightTime + 1) % LightPeriod;
}

// 在左4个数码管（表示南北方向）显示时间
// 参数：
//     x 显示的值，0~99，0以下不显示，99以上显示为99
void display_left_time(int x)
{
	int i = 3;
	if (x < 0)
	{
		set_display_value(3, -1);
		set_display_value(2, -1);
		return;
	}
	if (x > 99)
	{
		x = 99; // 常见的红绿灯都是显示2位
	}
	set_display_value(3, x % 10);
	set_display_value(2, x / 10 % 10);
}

// 在右4个数码管（表示东西方向）显示时间
// 参数：
//     x 显示的值，0~99，0以下不显示，99以上显示为99
void display_right_time(int x)
{
	int i = 3;
	if (x < 0)
	{
		set_display_value(7, -1);
		set_display_value(6, -1);
		return;
	}
	if (x > 99)
	{
		x = 99; // 常见的红绿灯都是显示2位
	}
	set_display_value(7, x % 10);
	set_display_value(6, x / 10 % 10);
}

// D9~D10两个灯
sbit D9 = P2 ^ 6;
sbit D10 = P2 ^ 7;

// 停止红绿灯
void stop_light()
{
	LightStatus = LIGHT_READY;
	P1 = 0xff;
	D9 = 1;
	D10 = 1;
	stop_timer();
}

// 显示设定的时间
// 参数：
//     ew 显示东西时间
//     sn 显示南北时间
void display_set_time(bool ew, bool sn)
{
	if (ew)
	{
		display_right_time(GreenTime[DIR_EAST_WEST]);
	}
	else
	{
		display_right_time(-1);
	}
	if (sn)
	{
		display_left_time(GreenTime[DIR_SOUTH_NORTH]);
	}
	else
	{
		display_left_time(-1);
	}
}

// 就绪状态
void on_light_ready()
{
	display_set_time(true, true);
	
	// 按键处理
	if (is_key_released(KEY_START_STOP))
	{
		start_light();
	}
	if (is_key_released(KEY_EDIT))
	{
		edit_light();
	}
}

// 启用黄灯闪烁，行人按下切换键后需要黄灯闪烁
bool EnableYellowFlash = false;

// 完整的红绿灯功能显示
void display_full()
{
	byte light = 0xff;
	uint tmp;
	D9 = 1;
	D10 = 1;
	
	// 东西方向
	if (LightTime < GreenTime[DIR_EAST_WEST])
	{
		// 绿灯
		EnableYellowFlash = false; // 关闭闪黄灯功能
		tmp = GreenTime[DIR_EAST_WEST] - LightTime - 1;
		if (tmp >= 5) // 绿灯剩余5秒以上
		{
			display_right_time(tmp);
			light &= 0xdf;
			D9 = 0;
			if (EnableBuzzer)
			{
				Beep = 0;
			}
		}
		else // 绿灯剩余不到5秒，绿灯闪烁并蜂鸣器报警
		{
			if (LightTimerCounter < 2 || LightTimerCounter >= 10)
			{
				display_right_time(tmp);
				light &= 0xdf;
				D9 = 0;
				if (EnableBuzzer)
				{
					Beep = ~Beep;
				}
			}
			else
			{
				display_right_time(-1);
				if (EnableBuzzer)
				{
					Beep = 0;
				}
			}
		}
	}
	else if (LightTime < GreenTime[DIR_EAST_WEST] + YellowTime)
	{
		// 黄灯
		tmp = GreenTime[DIR_EAST_WEST] + YellowTime - LightTime - 1;
		if (!EnableYellowFlash || (LightTimerCounter < 2 || LightTimerCounter >= 10)) // 行人切换闪烁效果
		{
			display_right_time(tmp);
			light &= 0xbf;
		}
		else
		{
			display_right_time(-1);
		}
		D10 = 0;
	}
	else
	{
		// 红灯
		tmp = LightPeriod - LightTime - 1;
		display_right_time(tmp);
		light &= 0x7f;
		D10 = 0;
	}
	
	// 南北方向
	if (LightTime < GreenTime[DIR_EAST_WEST] + YellowTime)
	{
		// 红灯
		tmp = GreenTime[DIR_EAST_WEST] + YellowTime - LightTime - 1;
		display_left_time(tmp);
		light &= 0xed;
	}
	else if (LightTime < GreenTime[DIR_EAST_WEST] + YellowTime + GreenTime[DIR_SOUTH_NORTH])
	{
		// 绿灯
		EnableYellowFlash = false; // 关闭闪黄灯功能
		tmp = GreenTime[DIR_EAST_WEST] + YellowTime + GreenTime[DIR_SOUTH_NORTH] - LightTime - 1;
		if (tmp >= 5) // 绿灯剩余5秒以上
		{
			display_left_time(tmp);
			light &= 0xfa;
		}
		else // 绿灯剩余不到5秒，绿灯闪烁并蜂鸣器报警
		{
			if (LightTimerCounter < 2 || LightTimerCounter >= 10)
			{
				display_left_time(tmp);
				light &= 0xfa;
				if (EnableBuzzer)
				{
					Beep = ~Beep;
				}
			}
			else
			{
				display_left_time(-1);
				if (EnableBuzzer)
				{
					Beep = 0;
				}
			}
		}
	}
	else
	{
		// 黄灯
		tmp = LightPeriod - LightTime - 1;
		if (!EnableYellowFlash || (LightTimerCounter < 2 || LightTimerCounter >= 10)) // 行人切换闪烁效果
		{
			display_left_time(tmp);
			light &= 0xf7;
		}
		else
		{
			display_left_time(-1);
		}
		light &= 0xfd;
	}
	
	// 设置红绿灯
	P1 = light;
}

// 仅任务书上的功能显示
void display_simple()
{
	byte light = 0xff;
	uint tmp;
	D9 = 1;
	D10 = 1;
	
	// 东西方向
	if (LightTime < GreenTime[DIR_EAST_WEST])
	{
		// 绿灯
		EnableYellowFlash = false; // 关闭闪黄灯功能
		tmp = GreenTime[DIR_EAST_WEST] - LightTime - 1;
		if (tmp >= 5) // 绿灯剩余5秒以上
		{
			light &= 0xdf;
			D9 = 0;
		}
		else // 绿灯剩余不到5秒，绿灯闪烁并蜂鸣器报警
		{
			if (LightTimerCounter < 2 || LightTimerCounter >= 10)
			{
				light &= 0xdf;
				D9 = 0;
				if (EnableBuzzer)
				{
					Beep = ~Beep;
				}
			}
			else
			{
				if (EnableBuzzer)
				{
					Beep = 0;
				}
			}
		}
		display_right_time(-1);
	}
	else if (LightTime < GreenTime[DIR_EAST_WEST] + YellowTime)
	{
		// 黄灯
		if (!EnableYellowFlash || (LightTimerCounter < 2 || LightTimerCounter >= 10)) // 行人切换闪烁效果
		{
			light &= 0xbf;
		}
		D10 = 0;
		display_right_time(-1);
	}
	else
	{
		// 红灯
		tmp = LightPeriod - LightTime - 1;
		if (tmp <= 10)
		{
			display_right_time(tmp);
		}
		else
		{
			display_right_time(-1);
		}
		light &= 0x7f;
		D10 = 0;
	}
	
	// 南北方向
	if (LightTime < GreenTime[DIR_EAST_WEST] + YellowTime)
	{
		// 红灯
		tmp = GreenTime[DIR_EAST_WEST] + YellowTime - LightTime - 1;
		if (tmp <= 10)
		{
			display_left_time(tmp);
		}
		else
		{
			display_right_time(-1);
		}
		light &= 0xed;
	}
	else if (LightTime < GreenTime[DIR_EAST_WEST] + YellowTime + GreenTime[DIR_SOUTH_NORTH])
	{
		// 绿灯
		EnableYellowFlash = false; // 关闭闪黄灯功能
		tmp = GreenTime[DIR_EAST_WEST] + YellowTime + GreenTime[DIR_SOUTH_NORTH] - LightTime - 1;
		if (tmp >= 5) // 绿灯剩余5秒以上
		{
			light &= 0xfa;
		}
		else // 绿灯剩余不到5秒，绿灯闪烁并蜂鸣器报警
		{
			if (LightTimerCounter < 2 || LightTimerCounter >= 10)
			{
				light &= 0xfa;
				if (EnableBuzzer)
				{
					Beep = ~Beep;
				}
			}
			else
			{
				if (EnableBuzzer)
				{
					Beep = 0;
				}
			}
		}
		display_left_time(-1);
	}
	else
	{
		// 黄灯
		if (!EnableYellowFlash || (LightTimerCounter < 2 || LightTimerCounter >= 10)) // 行人切换闪烁效果
		{
			light &= 0xf7;
		}
		light &= 0xfd;
		display_left_time(-1);
	}
	
	// 设置红绿灯
	P1 = light;
}

// 行人按钮切换
// 参数：
//     x 1为东西，2为南北
void switch_light_manually(byte x)
{
	switch (x)
	{
		case DIR_EAST_WEST: // 东西方向行人切换红绿灯
			stop_timer();
			LightTime = GreenTime[DIR_EAST_WEST] + YellowTime + GreenTime[DIR_SOUTH_NORTH];
			LightTimerCounter = 0;
			EnableYellowFlash = true;
			start_timer(Interval50000, on_light_timer);
			break;
		case DIR_SOUTH_NORTH: // 南北方向行人切换红绿灯
			stop_timer();
			LightTime = GreenTime[DIR_EAST_WEST];
			LightTimerCounter = 0;
			EnableYellowFlash = true;
			start_timer(Interval50000, on_light_timer);
			break;
		default:
			break;
	}
}

// 红绿灯运行中
void on_light_running()
{
	// 数码管显示
	switch (DisplayMode)
	{
		case DISPLAY_FULL:
			display_full();
			break;
		case DISPLAY_SIMPLE:
			display_simple();
			break;
		default:
			break;
	}
	
	// 按键处理
	if (is_key_released(KEY_START_STOP))
	{
		stop_light();
	}
	if (is_key_released(KEY_PEDESTRIANS1))
	{
		if (GreenTime[DIR_EAST_WEST] + YellowTime <= LightTime && LightTime < GreenTime[DIR_EAST_WEST] + YellowTime + GreenTime[DIR_SOUTH_NORTH]) // 只有当前方向是红灯的情况下按了才有用
		{
			switch_light_manually(DIR_EAST_WEST);
		}
	}
	if (is_key_released(KEY_PEDESTRIANS2))
	{
		if (LightTime < GreenTime[DIR_EAST_WEST]) // 只有当前方向是红灯的情况下按了才有用
		{
			switch_light_manually(DIR_SOUTH_NORTH);
		}
	}
}

// 编辑第几个红绿灯，0为禁用编辑功能，1为东西，2为南北
byte EditIndex = 0;

byte LightEditTimerCounter;
bool EditVisible;

// 红绿灯编辑状态的计时器
void on_light_edit_timer()
{
	++LightEditTimerCounter;
	if (LightEditTimerCounter == 10)
	{
		LightEditTimerCounter = 0;
		EditVisible = 1 - EditVisible;
		if (EditIndex == 1)
		{
			display_set_time(EditVisible, true);
		}
		else
		{
			display_set_time(true, EditVisible);
		}
	}

	start_timer(Interval50000, on_light_edit_timer);
}

// 启动编辑状态的闪烁效果计时器
void start_light_edit_flash_timer()
{
	LightEditTimerCounter = 0;
	EditVisible = true;
	display_set_time(true, true);
	start_timer(Interval50000, on_light_edit_timer);
}

// 停止编辑状态的闪烁效果计时器
void stop_light_edit_flash_timer()
{
	stop_timer();
	display_set_time(true, true);
}

// 编辑红绿灯
void edit_light()
{
	EditIndex = (EditIndex + 1) % 3;
	stop_light_edit_flash_timer();
	switch (EditIndex)
	{
	case 0:
		LightStatus = LIGHT_READY;
		break;
	case 1:
	case 2:
		LightStatus = LIGHT_EDITING;
		start_light_edit_flash_timer();
		break;
	default:
		break;
	}
}

byte EditAddTimerCounter;

// 绿灯时间增加1
void inc_green_time()
{
	if (GreenTime[EditIndex] < GreenTimeMax)
	{
		++GreenTime[EditIndex];
	}
	display_set_time(true, true);
}

// 按住增加键，第1秒后，每秒增加10
void on_edit_add_timer2()
{
	++EditAddTimerCounter;
	if (EditAddTimerCounter == 2)
	{
		EditAddTimerCounter = 0;
		inc_green_time();
	}
	
	start_timer(Interval50000, on_edit_add_timer2);
}

// 按住增加键，第1秒只增加1
void on_edit_add_timer()
{
	++EditAddTimerCounter;
	if (EditAddTimerCounter == 20)
	{
		EditAddTimerCounter = 0;
		inc_green_time();
		start_timer(Interval50000, on_edit_add_timer2);
		return;
	}
	
	start_timer(Interval50000, on_edit_add_timer);
}

// 开始增加时间
void edit_add_begin()
{
	inc_green_time();
	stop_light_edit_flash_timer();

	EditAddTimerCounter = 0;
	start_timer(Interval50000, on_edit_add_timer);
}

// 停止增加时间
void edit_add_stop()
{
	stop_timer();
	start_light_edit_flash_timer();
}

// 绿灯时间减少1
void dec_green_time()
{
	if (GreenTime[EditIndex] > GreenTimeMin)
	{
		--GreenTime[EditIndex];
	}
	display_set_time(true, true);
}

// 按住减少键，第1秒后，每秒减少10
void on_edit_sub_timer2()
{
	++EditAddTimerCounter;
	if (EditAddTimerCounter == 2)
	{
		EditAddTimerCounter = 0;
		dec_green_time();
	}
	
	start_timer(Interval50000, on_edit_sub_timer2);
}

// 按住减少键，第1秒只减小1
void on_edit_sub_timer()
{
	++EditAddTimerCounter;
	if (EditAddTimerCounter == 20)
	{
		EditAddTimerCounter = 0;
		dec_green_time();
		start_timer(Interval50000, on_edit_sub_timer2);
		return;
	}
	
	start_timer(Interval50000, on_edit_sub_timer);
}

// 开始减少时间
void edit_sub_begin()
{
	dec_green_time();
	stop_light_edit_flash_timer();

	EditAddTimerCounter = 0;
	start_timer(Interval50000, on_edit_sub_timer);
}

// 停止减少时间
void edit_sub_stop()
{
	stop_timer();
	start_light_edit_flash_timer();
}

// 编辑状态
void on_light_editing()
{
	if (is_key_pressed(KEY_ADD))
	{
		edit_add_begin();
	}
	if (is_key_released(KEY_ADD))
	{
		edit_add_stop();
	}
	if (is_key_pressed(KEY_SUB))
	{
		edit_sub_begin();
	}
	if (is_key_released(KEY_SUB))
	{
		edit_sub_stop();
	}
	if (is_key_released(KEY_EDIT))
	{
		edit_light();
	}
}

/***********
*          *
*  主函数  *
*          *
***********/

void main()
{
	while (true)
	{
		// 按键更新
		update_key();
		
		// 事件处理
		handle_light();
		
		// LED数码管显示
		display();
	}
}
