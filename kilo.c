//(setq c-basic-offset 4)
//(c-set-offset 'case-label 4)
//https://vt100.net/docs/vt100-ug/chapter3.html#ED
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>

/* DEFINES */
/* 0x00011111 & key */
#define CTRL_KEY(k) ((k) & 0x1f)

/* DATA */
struct editorConfig
{
    int screenrows;
    int screencols;
    termios orig_termios;
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

char editorReadKey()
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
    return c;
}

int getCursorPostion(int *rows, int *cols)
{
    char buf[32] = {};
    unsigned int i = 0;

    // queries for cursor postion n command
    if (write (STDOUT_FILENO, "x1b[6n", 4) != 4)
    {
        return -1;
    }

    while (i < (sizeof(buf) - 1))
    {
        if(read (STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i]  == 'R') break;
        i++;
    }
    buf[i] = '\0';
    
    editorReadKey();
    return -1;
}

int getWindowSize(int *rows, int *cols)
{
    struct winsize ws;

    if(1)
    {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        {
            return -1;
        }
        return getCursorPostion(rows, cols);
    }
    #ifdef stop
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        return -1;
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
    #endif
}

/* OUTPUT */
void editorDrawRows()
{
    int y;
    for (y = 0; y < E.screenrows; y++)
    {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen()
{
    write(STDOUT_FILENO,"\x1b[H", 3);
    write(STDOUT_FILENO,"\x1b[2J", 4);
    editorDrawRows();
    write(STDOUT_FILENO,"\x1b[H", 3);

}


/* INPUT */

void editorProcessKeypress()
{
    char c = editorReadKey();

    switch(c)
    {
        case CTRL_KEY('q'):
        {
            write(STDOUT_FILENO, "\x1b[H",3);
            write(STDOUT_FILENO, "\x1b[2J", 4);
            exit(0);
        } break;
    }
}

/* INIT */
void initEditor()
{
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
