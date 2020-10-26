#include "types.h"
#include "x86.h"
#include "param.h"
#include "defs.h"
#include "msg.h"
#include "spinlock.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

#include "user_gui.h"

#include "window_manager.h"

#define MAX_WIDTH 800
#define MAX_HEIGHT 600

int min(int x, int y) { return x < y ? x : y; }
int max(int x, int y) { return x > y ? x : y; }
int clamp(int x, int l, int r) { return min(r, max(l, x)); }
/*
static int windowlisthead, emptyhead;
static int desktopHandler = -100;
static int focus;
static int frontcnt;

static int clickedOnTitle, clickedOnContent;
*/

/*
static msg_buf globalMessages;

#define MAXWINDOW 20

static window_p windowHead;
static window_p windowTail;
*/

#define MAX_WINDOW_CNT 50

//linked-list of windows
static struct
{
	struct proc *proc;
	windowtemp wnd;
	int next, prev;
} windowlist[MAX_WINDOW_CNT];

static int windowlisthead, windowlisttail;
//static int desktopHandler = -100;
//static int focus;
//static int frontcnt;

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


int findNextAvailable()
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

void addToListTail(int *tail, int idx)
{
	windowlist[idx].prev = *tail;
	windowlist[idx].next = -1;
	if (*tail != -1)
		windowlist[*tail].next = idx;
	*tail = idx;
}

void removeFromList(int *tail, int idx)
{
	if (*tail == idx)
		*tail = windowlist[*tail].prev;
	if (windowlist[idx].prev != -1)
		windowlist[windowlist[idx].prev].next = windowlist[idx].next;
	if (windowlist[idx].next != -1)
		windowlist[windowlist[idx].next].prev = windowlist[idx].prev;
}

static int clickedOnTitle, clickedOnContent;

int next_window_id = 1;

struct spinlock wmlock;

static struct
{
	int x, y;
} wm_mouse_pos, wm_last_mouse_pos;

#define MOUSE_SPEED_X 1.0f
#define MOUSE_SPEED_Y -1.0f;

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
	wm_mouse_pos.x = SCREEN_WIDTH / 2;
	wm_mouse_pos.y = SCREEN_HEIGHT / 2;
	wm_last_mouse_pos = wm_mouse_pos;

	/*
	windowHead=0;
	windowTail=0;
	*/

	windowlisthead = -1;
	windowlisttail = -1;
	int i;
	for (i = 0; i < MAX_WINDOW_CNT; ++i)
	{
		windowlist[i].next = i;
		windowlist[i].prev = i;
	}
	//windowlist[0].prev = -1;
	//windowlist[MAX_WINDOW_CNT - 1].next = -1;

	//focus = -1;

	clickedOnTitle = clickedOnContent = 0;

	//initMessageQueue(&globalMessages);

	initlock(&wmlock, "wmlock");
}
/*
void focusWindow(window_p focus)
{
	window_p p;
	for (p = windowHead; p != 0; p = p->next)
	{
		if (p == focus)
		{
			window_p prevWindow = p->prev;
			window_p nextWindow = p->next;
			if (prevWindow != 0)
			{
				prevWindow->next = nextWindow;
			}
			else
			{
				windowHead = nextWindow;
			}
			if (nextWindow != 0)
			{
				nextWindow->prev = prevWindow;
			}
			break;
		}
	}
	windowTail->next = focus;
	focus->prev = windowTail;
	focus->next = 0;
	windowTail = focus;
}
*/

void moveRect(win_rect *rect, int dx, int dy)
{
	rect->xmin += dx;
	rect->xmax += dx;
	rect->ymin += dy;
	rect->ymax += dy;
}

/*
void moveFocusWindow(int dx, int dy)
{
	moveRect(&windowTail->position, dx, dy);
}

void debugPrintWindowList()
{
	window_p p;
	cprintf("############################\n");
	if (windowHead != 0)
	{
		cprintf("current Head at %x\n", windowHead);
		cprintf("current Tail at %x\n", windowTail);
		cprintf("\n");
		
		for (p = windowHead; p != 0; p = p->next)
		{
			cprintf("current Window at %x\n", p);
			cprintf("prev Window at %x\n", p->prev);
			cprintf("next Window at %x\n", p->next);
			cprintf("current Window width %d\n", p->position.xmax - p->position.xmin);
			cprintf("\n");
		}
		
	}
}
*/

void debugPrintWindowList()
{

	cprintf("############################\n");
	cprintf("current Head at %d\n", windowlisthead);
	cprintf("current Tail at %d\n", windowlisttail);
	cprintf("\n");

	int p;
	for (p = windowlisthead; p != -1; p = windowlist[p].next)
	{

		cprintf("current Window at %d\n", p);
		cprintf("prev Window at %d\n", windowlist[p].prev);
		cprintf("next Window at %d\n", windowlist[p].next);
		//cprintf("current Window width %d\n", windowlist[p].wnd->position.xmax - windowlist[p].wnd->position.xmin);
		cprintf("\n");
	}
}

void focusWindow(int winId)
{
	cprintf("focus window %d\n", winId);
	if (winId == -1 || winId==windowlisttail)
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

	addToListTail(&windowlisttail, winId);

	//debugPrintWindowList();
}

void wmHandleMessage(message *msg)
{
	acquire(&wmlock);

	//memset(screen, SCREEN_WIDTH * SCREEN_HEIGHT * 3, 0);
	//memset(screen_buf1, SCREEN_WIDTH * SCREEN_HEIGHT * 3, 0);
	//message newmsg;
	switch (msg->msg_type)
	{
	case M_MOUSE_MOVE:
		//cprintf("mouse moved %d, %d\n", msg->params[0], msg->params[1]);
		wm_last_mouse_pos = wm_mouse_pos;
		wm_mouse_pos.x += msg->params[0] * MOUSE_SPEED_X;
		wm_mouse_pos.y += msg->params[1] * MOUSE_SPEED_Y;
		if (wm_mouse_pos.x > SCREEN_WIDTH)
			wm_mouse_pos.x = SCREEN_WIDTH;
		if (wm_mouse_pos.y > SCREEN_HEIGHT)
			wm_mouse_pos.y = SCREEN_HEIGHT;
		if (wm_mouse_pos.x < 0)
			wm_mouse_pos.x = 0;
		if (wm_mouse_pos.y < 0)
			wm_mouse_pos.y = 0;
		//redraw mouse cursor
		//clearMouse(screen, screen_buf1, wm_last_mouse_pos.x, wm_last_mouse_pos.y);
		//drawMouse(screen, 0, wm_mouse_pos.x, wm_mouse_pos.y);
		//drag

		if (clickedOnTitle)
		{
			//moveFocusWindow(wm_mouse_pos.x - wm_last_mouse_pos.x, wm_mouse_pos.y - wm_last_mouse_pos.y);
		}
		break;
	case M_MOUSE_DOWN:
		debugPrintWindowList();
		cprintf("mouse at %d, %d\n", wm_mouse_pos.x, wm_mouse_pos.y);
		//handle focus changes
		int p;
		for (p = windowlisttail; p != -1; p = windowlist[p].prev)
		{

			windowtemp* win = &windowlist[p].wnd;
			if (isInRect(win->position.xmin, win->position.ymin - TITLE_HEIGHT, win->position.xmax, win->position.ymax, wm_mouse_pos.x, wm_mouse_pos.y))
			{
				focusWindow(p);
				break;
			}
		}

		/*
		if (windowTail != 0 && isInRect(windowTail->position.xmin, windowTail->position.ymin, windowTail->position.xmax, windowTail->position.ymax, wm_mouse_pos.x, wm_mouse_pos.y))
		{
			clickedOnContent = 1;
			//newmsg = *msg;
			//coordinate transformation (from screen to window)
			//newmsg.params[0] = wm_mouse_pos.x - windowlist[focus].wnd.contents.xmin;
			//newmsg.params[1] = wm_mouse_pos.y - windowlist[focus].wnd.contents.ymin;
			//newmsg.params[2] = msg->params[0];
			//dispatchMessage(focus, &newmsg);
		}
		//else if (p!=0 && wm_mouse_pos.x + 30 > p->position.xmax) //close
		//{
		//newmsg.msg_type = WM_WINDOW_CLOSE;
		//dispatchMessage(focus, &newmsg);
		//}
		else // titlebar
		{
			clickedOnTitle = 1;
		}
		*/

		break;
	case M_MOUSE_LEFT_CLICK:
		/*
		if (clickedOnContent)
		{
			clickedOnContent = 0;
			
			newmsg = *msg;
			newmsg.params[0] = wm_mouse_pos.x - windowlist[focus].wnd.contents.xmin;
			newmsg.params[1] = wm_mouse_pos.y - windowlist[focus].wnd.contents.ymin;
			newmsg.params[2] = msg->params[0];
			dispatchMessage(focus, &newmsg);
			
		}
		*/
		break;
	case M_MOUSE_RIGHT_CLICK:
		/*
		if (clickedOnContent)
		{
			
			clickedOnContent = 0;
			newmsg = *msg;
			newmsg.params[0] = wm_mouse_pos.x - windowlist[focus].wnd.contents.xmin;
			newmsg.params[1] = wm_mouse_pos.y - windowlist[focus].wnd.contents.ymin;
			newmsg.params[2] = msg->params[0];
			dispatchMessage(focus, &newmsg);
			
		}
		*/
		break;
	case M_MOUSE_DBCLICK:
		/*
		if (clickedOnContent)
		{
			
			clickedOnContent = 0;
			newmsg = *msg;
			newmsg.params[0] = wm_mouse_pos.x - windowlist[focus].wnd.contents.xmin;
			newmsg.params[1] = wm_mouse_pos.y - windowlist[focus].wnd.contents.ymin;
			newmsg.params[2] = msg->params[0];
			dispatchMessage(focus, &newmsg);
			
		}
		*/
		break;
	case M_MOUSE_UP:
		/*
		if (clickedOnContent)
		{ 
			clickedOnContent = 0;
			newmsg = *msg;
			newmsg.params[0] = wm_mouse_pos.x - windowlist[focus].wnd.contents.xmin;
			newmsg.params[1] = wm_mouse_pos.y - windowlist[focus].wnd.contents.ymin;
			newmsg.params[2] = msg->params[0];
			dispatchMessage(focus, &newmsg);
			
		}
		else if (clickedOnTitle)
		{
			clickedOnTitle = 0;
		}
		*/
		break;
	case M_KEY_DOWN:
		//dispatchMessage(focus, msg);
		break;
	case M_KEY_UP:
		//dispatchMessage(focus, msg);
		break;

	default:
		break;
	}
	//memmove(screen, screen_buf1, SCREEN_WIDTH * SCREEN_HEIGHT * 3);
	release(&wmlock);
}

void updateScreen()
{

	acquire(&wmlock);

	memset(screen_buf1, 255, screen_size);

	int p;
	for (p = windowlisthead; p != -1; p = windowlist[p].next)
	{
		windowtemp* win = &windowlist[p].wnd;
		int width = win->position.xmax - win->position.xmin;
		int height = win->position.ymax - win->position.ymin;
		draw24ImagePart(screen_buf1, win->window_buf, win->position.xmin, win->position.ymin,
						width, height, 0, 0, width, height);
		draw24ImagePart(screen_buf1, win->title_buf, win->position.xmin, win->position.ymin - TITLE_HEIGHT,
						width, TITLE_HEIGHT, 0, 0, width, TITLE_HEIGHT);
	}
	/*
	if (windowHead != 0)
	{
		window_p p = windowHead;
		int width = p->position.xmax - p->position.xmin;
		int height = p->position.ymax - p->position.ymin;
		draw24ImagePart(screen_buf1, p->window_buf, p->position.xmin, p->position.ymin,
						width, height, 0, 0, width, height);
		while (p->next != 0)
		{
			p = p->next;
			int width = p->position.xmax - p->position.xmin;
			int height = p->position.ymax - p->position.ymin;
			draw24ImagePart(screen_buf1, p->window_buf, p->position.xmin, p->position.ymin,
							width, height, 0, 0, width, height);
			draw24ImagePart(screen_buf1, p->title_buf, p->position.xmin, p->position.ymin - TITLE_HEIGHT,
							width, TITLE_HEIGHT, 0, 0, width, TITLE_HEIGHT);
		}
	}
	*/

	drawMouse(screen_buf1, 0, wm_mouse_pos.x, wm_mouse_pos.y);
	memmove(screen, screen_buf1, screen_size);

	release(&wmlock);
}

//return window handler on succuss, -1 if unsuccessful
int createWindow(window_p window)
{

	acquire(&wmlock);

	/*
	if (windowHead == 0)
	{
		windowHead = window;
	}
	if (windowTail != 0)
	{
		windowTail->next = window;
	}

	if (windowHead == window)
	{
		window->prev = 0;
	}
	else
	{
		window->prev = windowTail;
	}

	windowTail = window;
	window->next = 0;
	//initMessageQueue(&window->messages);
*/
	/*
	if (emptyhead == -1)
		return -1;

	int idx = emptyhead;
	removeFromList(&emptyhead, idx);
	addToListTail(&windowlisttail, idx);
*/
	int winId = findNextAvailable();
	if (winId == -1)
		return 1;

	if (windowlisthead == -1)
	{
		windowlisthead = winId;
	}

	addToListTail(&windowlisttail, winId);

	createRectByCoord(&windowlist[winId].wnd.position, window->position.xmin, window->position.ymin, window->position.xmax, window->position.ymax);

	windowlist[winId].wnd.window_buf = window->window_buf;
	windowlist[winId].wnd.title_buf = window->title_buf;

	debugPrintWindowList();

	release(&wmlock);

	return 0;
}

int sys_createWindow()
{
	window *wnd;
	argptr(0, (char **)&wnd, sizeof(window));
	return createWindow(wnd);
}

int sys_updateScreen()
{
	updateScreen();
	return 0;
}
