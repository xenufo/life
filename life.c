#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <ctype.h>

int HPAD = 0;
int VPAD = 0;

typedef struct {
    int h;
    int w;
    bool *state;
    bool *next_state;
} Life;

void init_empty(Life *life) {
    life->state = malloc(life->h*life->w*sizeof(bool));
    life->next_state = malloc(life->h*life->w*sizeof(bool));
}

void init_random(Life *life) {
    for (int i=0; i < life->h*life->w; i++) {
        life->state[i] = !(rand() % 8);
    }
}

void init_line(Life *life) {
    for (int i=0; i < life->w-1; i++) {
        life->state[i] = 1;
    }
}

void init_glider(Life *life) {
    life->state[0*life->w+1] = 1;
    life->state[1*life->w+2] = 1;
    life->state[2*life->w+0] = 1;
    life->state[2*life->w+1] = 1;
    life->state[2*life->w+2] = 1;
}

void update(Life *life) {
    for (int i=0; i < life->h*life->w; i++) {
        life->next_state[i] = life->state[i];
    }
    for (int y=0; y < life->h; y++) {
        for (int x=0; x < life->w; x++) {
            int i = y*life->w+x;
            int neighbors = 0;
            for (int dy=-1; dy <= 1; dy++) 
                for (int dx=-1; dx <= 1; dx++) 
                    if (dy || dx) {
                        int row = (y+dy)%life->h;
                        if (row == -1)
                            row = life->h-1;
                        int col = (x+dx)%life->w;
                        if (col == -1)
                            col = life->w-1;
                        neighbors += life->state[row*life->w + col];
                        // neighbors += state[((y+dy)%h+h)%h*w + ((x+dx)%w+w)%w];
                    }
            if (neighbors < 2 || neighbors > 3)
                life->next_state[i] = 0;
            if (neighbors == 3)
                life->next_state[i] = 1;
        }
    }
    for (int i=0; i < life->h*life->w; i++) {
        life->state[i] = life->next_state[i];
    }
}

void display_frame(Life *life) {
    clear();
    // printf("\033[2J\033[H");
    for (int y=0; y < life->h-1; y+=2) {
        for (int x=0; x < life->w; x++) {
            int upper = life->state[y*life->w+x];
            int lower = life->state[(y+1)*life->w+x];
            char *cell = " ";
            if (upper && lower) 
                cell = "\u2588";
            else if (upper) 
                cell = "\u2580";
            else if (lower) 
                cell = "\u2584";
            mvprintw(VPAD+y/2, HPAD+x, "%s", cell);
            // printf("%s", cell);
        }
        // printf("\n");
    }
    // for (int y=-1; y<life->h/2; y++) {
        // mvprintw(VPAD+y, HPAD-1, "%s", ":");
        // mvprintw(VPAD+y, HPAD+life->w, "%s", ":");
    // }
    // for (int x=-1; x<life->w; x++) {
        // mvprintw(VPAD-1, HPAD+x, "%s", ".");
        // mvprintw(VPAD+life->h/2, HPAD+x, "%s", ".");
    // }
    refresh();
}

void set_padding(Life *life) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    HPAD = (cols - life->w)/2;
    VPAD = (rows - life->h/2)/2;

}

static const char* skip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

bool rle_parse_header(const char *line, int *x, int *y) {
    int h = -1, w = -1;
    const char *p = line;

    // find x
    p = strstr(p, "x");
    if (!p) return false;
    p++;
    p = skip_ws(p);
    if (*p!='=') return false;
    p++;
    p = skip_ws(p);
    if (!isdigit((unsigned char)*p)) return 0;
    w = (int)strtol(p, (char**)&p, 10);

    // find y
    p = strstr(p, "y");
    if (!p) return false;
    p++;
    p = skip_ws(p);
    if (*p!='=') return false;
    p++;
    p = skip_ws(p);
    if (!isdigit((unsigned char)*p)) return 0;
    h = (int)strtol(p, (char**)&p, 10);

    *x = w*2;
    *y = h*2;
    return true;
}

Life rle_load(char *file_name) {
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL) {
        printf("could not read file \"%s\"\n", file_name);
        exit(1);
    }
    char line[1024];
    int h = -1;
    int w = -1;

    // skip everything before the header
    while (fgets(line, sizeof(line), fp)) {
        const char *s = skip_ws(line);
        if (*s == '\0') continue;
        if (*s == '#') continue;
        if (rle_parse_header(s, &w, &h)) break;
    }
    if (h<0 || w<0) {
        printf("no header found in \"%s\"\n", file_name);
        exit(1);
    }
    Life life = {.h = h, .w = w};
    init_empty(&life);

    // parse rle
    int x=w/4,y=h/4;
    int run=0;
    int c;
    while ((c=fgetc(fp))!=EOF) {
        if (isspace((unsigned char)c) || c==',') continue;
        if (isdigit((unsigned char)c)) {
            run = run * 10+(c-'0');
            continue;
        }
        int pos_offset = 1;
        if (run) pos_offset = run;
        run = 0;
        switch (c) {
            case 'b':
                x += pos_offset;
                break;
            case 'o':
                for (int i=0; i<pos_offset; i++)
                    life.state[y*life.w+(x+i)] = 1;
                x += pos_offset;
                break;
            case '$':
                y += pos_offset;
                x = w/4;
                break;
            case '!':
                return life;
            default:
                printf("unknown token %c\n", c);
                exit(1);
        }
    }
    printf("error: EOF before \"!\"\n");
    exit(1);
}

int main (int argc, char** argv) {
    setlocale(LC_ALL, "");
    initscr();              // start curses mode
    cbreak();               // disable line buffering
    noecho();               // don't echo typed chars
    keypad(stdscr, TRUE);   // enable function keys, arrows, etc.
    nodelay(stdscr, TRUE);  // non-blocking getch()
    curs_set(0);            // 0 hide, 1 normal, 2 very visible
    set_escdelay(0);

    int msdelay = 100;
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    Life life;
    if (argc>1) {
        life = rle_load(argv[1]);
    } else {
        life.h = rows*2,
        life.w = cols,
        init_empty(&life);
        // init_random(&life);
        // init_glider(&life);
        init_line(&life);
    }
    if (argc>2) {
        msdelay = strtol(argv[2], NULL, 10);
    }

    set_padding(&life);
    display_frame(&life);
    for (;;) {
        int ch = getch();
        if (ch=='q') break;
        if (ch==27) break;
        if (ch=='r') init_random(&life);
        if (ch=='l') {
            if (argc>1)
                life = rle_load(argv[1]);
        }
        if (ch == KEY_RESIZE) {
            set_padding(&life);
        }
        if (ch=='g') init_glider(&life);
        update(&life);
        display_frame(&life);
        usleep(msdelay*1000);
    }

    endwin();
    return 0;
}
