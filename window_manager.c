#include "types.h"
#include "x86.h"
#include "param.h"
#include "defs.h"
#include "msg.h"
#include "spinlock.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "gui.h"

#include "user_gui.h"

#include "window_manager.h"

#define MAX_WIDTH 800
#define MAX_HEIGHT 600

int min(int x, int y) { return x < y ? x : y; }
int max(int x, int y) { return x > y ? x : y; }
int clamp(int x, int l, int r) { return min(r, max(l, x)); }

struct RGBA titleBarColor;

#define MAX_WINDOW_CNT 50

//linked-list of windows
static struct
{
	struct proc *proc;
	kernel_window wnd;
	int next, prev;
} windowlist[MAX_WINDOW_CNT];

kernel_window desktopWindow;

static int windowlisthead, windowlisttail;
static int desktopId = -1;

void createRectByCoord(win_rect *rect, int xmin, int ymin, int xmax, int ymax)
{
	rect->xmin = xmin;
	rect->xmax = xmax;
	rect->ymin = ymin;
	rect->ymax = ymax;
}

void createRectBySize(win_rect *rect, int xmin, int ymin, int width, int height)
{
	rect->xmin = xmin;
	rect->xmax = xmin + width;
	rect->ymin = ymin;
	rect->ymax = ymin + height;
}

int findNextAvailableWindowId()
{
	for (int i = 0; i < MAX_WINDOW_CNT; i++)
	{
		if (windowlist[i].prev == i && windowlist[i].next == i)
		{
			return i;
		}
	}
	return -1;
}

void addToWindowList(int idx)
{
	windowlist[idx].prev = windowlisttail;
	windowlist[idx].next = -1;
	if (windowlisttail != -1)
		windowlist[windowlisttail].next = idx;
	windowlisttail = idx;
}

void removeFromWindowList(int idx)
{
	if (windowlisttail == idx)
		windowlisttail = windowlist[windowlisttail].prev;
	if (windowlist[idx].prev != -1)
		windowlist[windowlist[idx].prev].next = windowlist[idx].next;
	if (windowlist[idx].next != -1)
		windowlist[windowlist[idx].next].prev = windowlist[idx].prev;
}

static int clickedOnTitle, clickedOnContent;

struct spinlock wmlock;

static struct
{
	int x, y;
} wm_mouse_pos, wm_last_mouse_pos;

#define MOUSE_SPEED_X 1
#define MOUSE_SPEED_Y -1

int isInRect(int xmin, int ymin, int xmax, int ymax, int x, int y)
{
	return (x >= xmin && x <= xmax && y >= ymin && y <= ymax);
}

int dispatchMessage(msg_buf *buf, message *msg)
{
	if (buf->cnt >= MSG_BUF_SIZE)
		return 1;
	++buf->cnt;
	buf->data[buf->rear] = *msg;
	if ((++buf->rear) >= MSG_BUF_SIZE)
		buf->rear = 0;
	return 0;
}

//return non-zero if buf is empty
int getMessage(msg_buf *buf, message *result)
{
	if (buf->cnt == 0)
		return 1;
	--buf->cnt;
	*result = buf->data[buf->front];
	if ((++buf->front) >= MSG_BUF_SIZE)
		buf->front = 0;
	return 0;
}

void initMessageQueue(msg_buf *buf)
{
	buf->front = buf->rear = buf->cnt = 0;
}

void wmInit()
{
	titleBarColor.R = 244;
	titleBarColor.G = 160;
	titleBarColor.B = 5;
	titleBarColor.A = 255;

	wm_mouse_pos.x = SCREEN_WIDTH / 2;
	wm_mouse_pos.y = SCREEN_HEIGHT / 2;
	wm_last_mouse_pos = wm_mouse_pos;

	windowlisthead = -1;
	windowlisttail = -1;
	int i;
	for (i = 0; i < MAX_WINDOW_CNT; ++i)
	{
		windowlist[i].next = i;
		windowlist[i].prev = i;
	}

	clickedOnTitle = clickedOnContent = 0;

	initlock(&wmlock, "wmlock");
}

void moveRect(win_rect *rect, int dx, int dy)
{
	rect->xmin += dx;
	rect->xmax += dx;
	rect->ymin += dy;
	rect->ymax += dy;
}

void debugPrintWindowList()
{

	cprintf("############################\n");
	cprintf("current Proc at %d\n", myproc());
	cprintf("current Head at %d\n", windowlisthead);
	cprintf("current Tail at %d\n", windowlisttail);
	cprintf("\n");

	int p;
	for (p = windowlisthead; p != -1; p = windowlist[p].next)
	{

		cprintf("current Window at %d\n", p);
		cprintf("current Window proc %d\n", windowlist[p].proc);
		cprintf("prev Window at %d\n", windowlist[p].prev);
		cprintf("next Window at %d\n", windowlist[p].next);
		//cprintf("current Window width %d\n", windowlist[p].wnd->position.xmax - windowlist[p].wnd->position.xmin);
		cprintf("\n");
	}
}

void focusWindow(int winId)
{
	//cprintf("focus window %d\n", winId);
	if (winId == -1 || winId == windowlisttail)
		return;
	if (winId == windowlisthead)
	{
		int newhead = windowlist[winId].next;
		windowlist[newhead].prev = -1;
		windowlisthead = newhead;
	}
	else
	{
		int prevWin = windowlist[winId].prev;
		int nextWin = windowlist[winId].next;
		windowlist[prevWin].next = nextWin;
		windowlist[nextWin].prev = prevWin;
	}

	addToWindowList(winId);
}

void moveFocusWindow(int dx, int dy)
{
	windowlist[windowlisttail].wnd.position.xmin += dx;
	windowlist[windowlisttail].wnd.position.xmax += dx;
	windowlist[windowlisttail].wnd.position.ymin += dy;
	windowlist[windowlisttail].wnd.position.ymax += dy;
}

void handleDesktopDockClick(int mouse_x)
{
	int p;
	int windowCount = 0;
	for (p = windowlisthead; p != -1; p = windowlist[p].next)
	{
		if (p != desktopId)
		{
			windowCount++;
		}
	}
	if (windowCount > 0)
	{
		int xStart = 5;
		int barWidth = SCREEN_WIDTH / (windowCount + 1);
		for (p = windowlisthead; p != -1; p = windowlist[p].next)
		{
			if (p != desktopId)
			{
				if (mouse_x > xStart + 1 && mouse_x <= xStart + barWidth - 1)
				{
					if (windowlist[p].wnd.minimized)
					{
						message newmsg;
						newmsg.msg_type = WM_WINDOW_MAXIMIZE;
						dispatchMessage(&windowlist[p].wnd.msg_buf, &newmsg);
					}
					else
					{
						focusWindow(p);
					}
					break;
				}
				xStart += barWidth;
			}
		}
	}
}

void wmHandleMessage(message *msg)
{
	acquire(&wmlock);

	message newmsg;
	switch (msg->msg_type)
	{
	case M_MOUSE_MOVE:
		wm_last_mouse_pos = wm_mouse_pos;
		if (msg->params[0] > -SCREEN_WIDTH && msg->params[0] < SCREEN_WIDTH)
		{
			if (msg->params[1] > -SCREEN_HEIGHT && msg->params[0] < SCREEN_HEIGHT)
			{
				wm_mouse_pos.x += msg->params[0] * MOUSE_SPEED_X;
				wm_mouse_pos.y += msg->params[1] * MOUSE_SPEED_Y;
			}
		}

		if (wm_mouse_pos.x > SCREEN_WIDTH)
			wm_mouse_pos.x = SCREEN_WIDTH;
		if (wm_mouse_pos.y > SCREEN_HEIGHT)
			wm_mouse_pos.y = SCREEN_HEIGHT;
		if (wm_mouse_pos.x < 0)
			wm_mouse_pos.x = 0;
		if (wm_mouse_pos.y < 0)
			wm_mouse_pos.y = 0;

		if (clickedOnTitle)
		{
			moveFocusWindow(wm_mouse_pos.x - wm_last_mouse_pos.x, wm_mouse_pos.y - wm_last_mouse_pos.y);
		}
		break;
	case M_MOUSE_DOWN:;
		int p = windowlisttail;
		for (; p != -1; p = windowlist[p].prev)
		{

			kernel_window *win = &windowlist[p].wnd;
			if (p != desktopId && isInRect(win->position.xmin, win->position.ymin - TITLE_HEIGHT, win->position.xmax, win->position.ymax, wm_mouse_pos.x, wm_mouse_pos.y))
			{
				focusWindow(p);
				break;
			}
		}
		if (p == -1)
			focusWindow(desktopId);

		kernel_window *win = &windowlist[windowlisttail].wnd;
		if (isInRect(win->position.xmin, win->position.ymin - TITLE_HEIGHT, win->position.xmax, win->position.ymin, wm_mouse_pos.x, wm_mouse_pos.y))
		{
			if (wm_mouse_pos.x + 30 > win->position.xmax)
			{
				newmsg.msg_type = WM_WINDOW_CLOSE;
				dispatchMessage(&windowlist[windowlisttail].wnd.msg_buf, &newmsg);
			}
			else if (wm_mouse_pos.x + 60 > win->position.xmax && wm_mouse_pos.x + 30 <= win->position.xmax)
			{
				//not nessasary for current design, we could just minimize the window here
				//but we may want minimized window to halt updating
				newmsg.msg_type = WM_WINDOW_MINIMIZE;
				dispatchMessage(&windowlist[windowlisttail].wnd.msg_buf, &newmsg);
			}
			else
			{
				clickedOnTitle = 1;
			}
		}
		if (isInRect(win->position.xmin, win->position.ymin, win->position.xmax, win->position.ymax, wm_mouse_pos.x, wm_mouse_pos.y))
		{
			clickedOnContent = 1;
			if (windowlisttail == desktopId)
			{
				if (wm_mouse_pos.y >= SCREEN_HEIGHT - 1.2 * TITLE_HEIGHT && wm_mouse_pos.y <= SCREEN_HEIGHT)
				{
					handleDesktopDockClick(wm_mouse_pos.x);
				}
			}
		}

		break;

	//now window content only responds to mouse clicks and keyboard, does not respond to mouse drag
	case M_MOUSE_LEFT_CLICK:
	case M_MOUSE_RIGHT_CLICK:
	case M_MOUSE_DBCLICK:

		if (clickedOnContent)
		{
			clickedOnContent = 0;
			newmsg.msg_type = msg->msg_type;
			newmsg.params[0] = wm_mouse_pos.x - windowlist[windowlisttail].wnd.position.xmin;
			newmsg.params[1] = wm_mouse_pos.y - windowlist[windowlisttail].wnd.position.ymin;
			//cprintf("message type %d, dispatch to %d\n", msg->msg_type, windowlisttail);
			dispatchMessage(&windowlist[windowlisttail].wnd.msg_buf, &newmsg);
		}
	case M_MOUSE_UP:
		if (clickedOnContent)
		{
			clickedOnContent = 0;
		}
		if (clickedOnTitle)
		{
			clickedOnTitle = 0;
		}
		break;
	case M_KEY_DOWN:
	case M_KEY_UP:
		dispatchMessage(&windowlist[windowlisttail].wnd.msg_buf, msg);
		break;

	default:
		break;
	}
	release(&wmlock);
}

void drawWindowBar(struct RGB *dst, kernel_window *win, struct RGBA barcolor)
{
	struct RGBA closecolor, minimizecolor, txtcolor;
	closecolor.R = 219;
	closecolor.G = 68;
	closecolor.B = 55;
	closecolor.A = 255;
	minimizecolor.R = minimizecolor.G = minimizecolor.B = 120;
	minimizecolor.A = 255;
	txtcolor.R = txtcolor.G = txtcolor.B = txtcolor.A = 255;
	int xmin = win->position.xmin;
	int xmax = win->position.xmax;
	int ymin = win->position.ymin - TITLE_HEIGHT;
	int ymax = win->position.ymin;
	drawRectByCoord(dst, xmin, ymin, xmax - 2 * TITLE_HEIGHT, ymax, barcolor);
	drawRectByCoord(dst, xmax - 2 * TITLE_HEIGHT, ymin, xmax - TITLE_HEIGHT, ymax, minimizecolor);
	drawRectByCoord(dst, xmax - TITLE_HEIGHT, ymin, xmax, ymax, closecolor);
	drawString(dst, xmin + 5, ymin + 3, win->title, txtcolor);
}

void drawWindow(kernel_window *win, int drawTitleBar)
{
	int width = win->position.xmax - win->position.xmin;
	int height = win->position.ymax - win->position.ymin;

	draw24ImagePart(screen_buf1, win->window_buf, win->position.xmin, win->position.ymin,
					width, height, 0, 0, width, height);

	if (drawTitleBar == 1)
	{
		drawWindowBar(screen_buf1, win, titleBarColor);
	}
}

void drawDesktopDock(struct RGB *dst)
{
	struct RGBA dockColor;
	dockColor.R = 15;
	dockColor.G = 157;
	dockColor.B = 88;
	dockColor.A = 255;
	drawRectByCoord(dst, 0, SCREEN_HEIGHT - TITLE_HEIGHT * 1.2, SCREEN_WIDTH, SCREEN_HEIGHT, dockColor);
	struct RGBA txtcolor;
	txtcolor.R = txtcolor.G = txtcolor.B = txtcolor.A = 255;
	int p;
	int windowCount = 0;
	for (p = windowlisthead; p != -1; p = windowlist[p].next)
	{
		if (p != desktopId)
		{
			windowCount++;
		}
	}
	if (windowCount > 0)
	{
		int xStart = 5;
		int barWidth = SCREEN_WIDTH / (windowCount + 1);
		for (p = windowlisthead; p != -1; p = windowlist[p].next)
		{
			if (p != desktopId)
			{
				drawStringWithMaxWidth(dst, xStart + 5, SCREEN_HEIGHT - TITLE_HEIGHT * 0.9, barWidth - 2, windowlist[p].wnd.title, txtcolor);
				drawRectByCoord(dst, xStart + barWidth - 2, SCREEN_HEIGHT - TITLE_HEIGHT * 1.2, xStart + barWidth, SCREEN_HEIGHT, txtcolor);
				xStart += barWidth;
			}
		}
	}
}

void updateScreen()
{

	acquire(&wmlock);

	memset(screen_buf1, 255, screen_size);
	drawWindow(&windowlist[desktopId].wnd, 0);
	drawDesktopDock(screen_buf1);

	int p;
	for (p = windowlisthead; p != -1; p = windowlist[p].next)
	{
		if (p != desktopId && windowlist[p].wnd.minimized==0)
		{

			switchuvm(windowlist[p].proc);
			drawWindow(&windowlist[p].wnd, 1);
		}
	}
	if (myproc() == 0)
		switchkvm();
	else
		switchuvm(windowlist[desktopId].proc);

	drawMouse(screen_buf1, 0, wm_mouse_pos.x, wm_mouse_pos.y);
	memmove(screen, screen_buf1, screen_size);

	release(&wmlock);
}

//return window handler on succuss, -1 if unsuccessful
int createWindow(window_p window, char *title)
{

	acquire(&wmlock);

	int winId = findNextAvailableWindowId();
	if (winId == -1)
		return 1;

	if (windowlisthead == -1)
	{
		windowlisthead = winId;
	}

	addToWindowList(winId);

	if (desktopId == -1)
	{
		desktopId = winId;
		createRectByCoord(&windowlist[winId].wnd.position, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	}
	else
	{
		createRectByCoord(&windowlist[winId].wnd.position, SCREEN_WIDTH / 2 - window->width / 2, SCREEN_HEIGHT / 2 - window->height / 2, SCREEN_WIDTH / 2 + window->width / 2, SCREEN_HEIGHT / 2 + window->height / 2);
	}

	windowlist[winId].wnd.window_buf = window->window_buf;
	window->handler = winId;
	windowlist[winId].proc = myproc();
	windowlist[winId].wnd.minimized = 0;
	//cprintf("current proc %d\n", windowlist[winId].proc);
	initMessageQueue(&windowlist[winId].wnd.msg_buf);

	uint len = strlen(title);
	if (len >= MAX_TITLE_LEN)
		return -1;
	memmove(windowlist[winId].wnd.title, title, len);

	//debugPrintWindowList();

	release(&wmlock);

	return 0;
}

int closeWindow(window_p window)
{

	acquire(&wmlock);

	int winId = window->handler;

	removeFromWindowList(winId);
	windowlist[winId].prev = winId;
	windowlist[winId].next = winId;
	initMessageQueue(&windowlist[winId].wnd.msg_buf);

	if (winId == windowlisttail && windowlisttail >= 1)
	{
		focusWindow(winId - 1);
	}

	//debugPrintWindowList();

	release(&wmlock);

	return 0;
}

int minimizeWindow(window_p window)
{

	acquire(&wmlock);

	int winId = window->handler;

	windowlist[winId].wnd.minimized = 1;

	release(&wmlock);

	return 0;
}

int maximizeWindow(window_p window)
{

	acquire(&wmlock);

	int winId = window->handler;

	windowlist[winId].wnd.minimized = 0;
	focusWindow(winId);

	release(&wmlock);

	return 0;
}

int sys_GUI_createWindow()
{
	window *wnd;
	char *title;
	argptr(0, (char **)&wnd, sizeof(window));
	argstr(1, &title);
	return createWindow(wnd, title);
}

int sys_GUI_closeWindow()
{
	window *wnd;
	argptr(0, (char **)&wnd, sizeof(window));
	return closeWindow(wnd);
}

int sys_GUI_minimizeWindow()
{
	window *wnd;
	argptr(0, (char **)&wnd, sizeof(window));
	return minimizeWindow(wnd);
}

int sys_GUI_maximizeWindow()
{
	window *wnd;
	argptr(0, (char **)&wnd, sizeof(window));
	return maximizeWindow(wnd);
}

int sys_GUI_getMessage()
{
	int h;
	message *res;
	argint(0, &h);
	argptr(1, (char **)(&res), sizeof(message));
	if (myproc() != windowlist[h].proc)
	{
		return 0;
	}
	return getMessage(&windowlist[h].wnd.msg_buf, res);
}

int sys_GUI_updateScreen()
{
	updateScreen();
	return 0;
}
