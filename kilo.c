
//(c-set-offset 'case-label 4)
//https://vt100.net/docs/vt100-ug/chapter3.html#ED
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>

void editorMoveCursor(int key);

/* DEFINES */
/* 0x00011111 & key */
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}
#define KILO_VERSION "0.0.1"



enum editorkey
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
};

    
/* DATA */
struct editorConfig
{
    int cx,cy;
    int screenrows;
    int screencols;
    termios orig_termios;
};

struct abuf
{
    char *b;
    int len;
};

editorConfig E;


/* TERMINAL */
void die(const char *s)
{
    // clears screen in case of error as wellx
    write(STDOUT_FILENO, "\x1b[H",3);
    write(STDOUT_FILENO, "\x1b[2J",4);
    
    perror(s);
    printf("\r");
    exit(1);
}
void disableRawMode()
{
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    {
        die("tcgetattr");
    }
}

void enableRawMode()
{

    if (tcgetattr(STDIN_FILENO,&E.orig_termios) == -1)
    {
        die("tcgetaddr");
    }
    atexit(disableRawMode);

    termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON |IEXTEN| ISIG);
    raw.c_cc[VMIN] = 0;           // min bytes to read prior to return
    raw.c_cc[VTIME] = 1;      // the max amount of time to wait for bytes 10ths of second
  
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        die("tcsetattr");
    }
}

int editorReadKey()
{
    int nread;
    char c;
    while ( (nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if(nread == -1 && errno != EAGAIN)
        {
            die("read");
        }
    }

    if (c == '\x1b')
    {
	char seq[3] = {};

	if (read(STDIN_FILENO, &seq[0] , 1) != 1)
	{
	    return '\x1b';
	}

	if (read(STDIN_FILENO, &seq[1] , 1) != 1)
	{
	    return '\x1b';
	}

	if (seq[0] == '[')
	{
	    switch (seq[1])
	    {
		case 'A': return  ARROW_UP;
		case 'B': return  ARROW_DOWN;
		case 'C': return  ARROW_RIGHT;
		case 'D': return  ARROW_LEFT;
	    }
	}
	
	return '\x1b';	
    }
    else
    {
	return c;	
    }
}

int getCursorPostion(int *rows, int *cols)
{
    char buf[32] = {};
    unsigned int i = 0;

    // queries for cursor postion n command
    if (write (STDOUT_FILENO, "\x1b[6n", 4) != 4)
    {
        return -1;
    }

    while (i < (sizeof(buf) - 1))
    {
        if(read (STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i]  == 'R') break;
        i++;
    }
    /* replace th R with NULL string terminator */
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d",rows,cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
	if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
	{
	    return -1;
	}
	return getCursorPostion(rows, cols);
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/* APPEND BUFFER */


void abAppend(abuf *ab, const char *s, int len)
{
    char *n =  (char *)realloc(ab->b, ab->len + len);

    if (n == NULL) return;
    memcpy(&n[ab->len],s, len);
    ab->b = n;
    ab->len += len;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}


/* OUTPUT */
void editorDrawRows(abuf *ab)
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
	if(y == E.screenrows / 3)
	{
	    char welcome[80];
	    int welcomelen = snprintf(welcome, sizeof(welcome),
				      "kilo editor --version %s", KILO_VERSION);
	    if (welcomelen > E.screencols)
		{
		welcomelen = E.screencols;
		}
	    
	    int padding = (E.screencols - welcomelen) / 2;

	    if(padding)
	    {
		abAppend(ab, "~", 1);
		padding--;
	    }
	    
	    while(padding--) 
	    {
		abAppend(ab," ",1);
	    }
	    abAppend(ab,welcome, welcomelen);
	}
	    else
	{
	    abAppend(ab, "~", 1);
	}
	
	abAppend(ab, "\x1b[K", 3); // clears rest of line
	// 2 erases whole line
	// 1 erases to the left of the cursor
	// 0 erases to the right this is defualt

	if(y < E.screenrows - 1)
	{
	    abAppend(ab,"\r\n", 2);
	}
    }
}

void editorRefreshScreen()
{
    abuf ab = ABUF_INIT;

    abAppend(&ab,"\x1b[?25l", 6); // hide cursor/not in vterm
    // abAppend(&ab,"\x1b[2J", 4); //clear screen
    abAppend(&ab,"\x1b[H", 3); // set curson pos

    editorDrawRows(&ab);

    char buf[32] = {};
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab,"\x1b[?25h", 6); // show cursor/not in vterm

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/* INPUT */
void editorProcessKeypress()
{
    int c = editorReadKey();

    switch(c)
    {
	case CTRL_KEY('q'):
        {
            write(STDOUT_FILENO, "\x1b[H",3);
            write(STDOUT_FILENO, "\x1b[2J", 4);
            exit(0);
        } break;

	case ARROW_UP:
	case ARROW_DOWN:
	case ARROW_LEFT:
	case ARROW_RIGHT:
	{
	    editorMoveCursor(c);
	} break;
    }
}

void editorMoveCursor(int key)
{
    switch (key)
    {
	case ARROW_LEFT:
	{
	    E.cx--;
	} break;
	case ARROW_RIGHT:
	{
	    E.cx++;
	} break;
	case ARROW_UP:
	{
	    E.cy--;
	} break;

	case ARROW_DOWN:
	{
	    E.cy++;
	} break;
    }
}

/* INIT */
void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    
    if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    {
        die("getWindowsSize");
    }
 
}

int main()
{
    enableRawMode();
    initEditor();

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
} 
