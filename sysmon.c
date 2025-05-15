#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_PROCESSES 256
#define PIPE_PATH "/tmp/sysmon_pipe"

typedef struct {
    int pid;
    char user[32];
    float cpu, mem;
    char command[256];
    int nice;
    int priority;
} Process;

Process proc_list[MAX_PROCESSES];
int proc_count = 0;
int scroll_offset = 0;
int matched_proc_count = 0;

char search_term[64] = "";
long long last_user = 0, last_nice = 0, last_system = 0, last_idle = 0;

void init_colors() {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
}

float get_cpu_usage() {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0;

    char buffer[256];
    fgets(buffer, sizeof(buffer), fp);
    fclose(fp);

    long long user, nice, system, idle;
    sscanf(buffer, "cpu %lld %lld %lld %lld", &user, &nice, &system, &idle);

    long long total_diff = (user - last_user) + (nice - last_nice) + (system - last_system);
    long long total_time = total_diff + (idle - last_idle);

    float cpu = (total_time == 0) ? 0 : (100.0 * total_diff / total_time);

    last_user = user;
    last_nice = nice;
    last_system = system;
    last_idle = idle;

    return cpu;
}

float get_memory_usage() {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return 0;

    char label[32];
    long value;
    long total = 0, free = 0, buffers = 0, cached = 0;

    while (fscanf(fp, "%31s %ld", label, &value) == 2) {
        if (strcmp(label, "MemTotal:") == 0) total = value;
        else if (strcmp(label, "MemFree:") == 0) free = value;
        else if (strcmp(label, "Buffers:") == 0) buffers = value;
        else if (strcmp(label, "Cached:") == 0) cached = value;
    }
    fclose(fp);

    long used = total - free - buffers - cached;
    return (float)used / total * 100;
}

void write_usage_to_pipe(float cpu, float mem) {
    FILE *fp = fopen("/proc/loadavg", "r");
    float load1 = 0.0;
    if (fp) {
        fscanf(fp, "%f", &load1);
        fclose(fp);
    }

    int fd = open(PIPE_PATH, O_WRONLY | O_NONBLOCK);
    if (fd != -1) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%.2f %.2f %.2f %d\n", cpu, mem, load1, proc_count);
        write(fd, buffer, strlen(buffer));
        close(fd);
    }
}
void banner() {
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(0, 0, "System Monitor - Press [q] to Quit | [/] Search | [↑/↓] Scroll");
    attroff(COLOR_PAIR(2) | A_BOLD);
}


void draw_bar(int row, const char *label, float percent) {
    attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(row, 0, "%s", label);
    attroff(COLOR_PAIR(6) | A_BOLD);

    int width = COLS - 15;
    if (width < 0) width = 0;

    int filled = (int)(percent / 100.0 * width);
    attron(COLOR_PAIR(5));
    mvprintw(row, 12, "[");
    for (int i = 0; i < width; i++) {
        addch(i < filled ? '=' : ' ');
    }
    printw("] %.1f%%", percent);
    attroff(COLOR_PAIR(5));
}

void head() {
    attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(3, 0, " PID   USER       NI PRI  CPU%%  MEM%%  COMMAND");
    mvhline(4, 0, ACS_HLINE, COLS);
    attroff(COLOR_PAIR(6) | A_BOLD);
}

void process_line(int row, int pid, const char* user, int nice, int pri, float cpu, float mem, const char* command, int alt) {
    char display_cmd[64];  // max 50 chars + room for ellipsis + null-terminator
    int cmd_len = strlen(command);

    if (cmd_len > 50) {
        // Show last 47 characters with ellipsis
        snprintf(display_cmd, sizeof(display_cmd), "...%s", command + cmd_len - 47);
    } else {
        snprintf(display_cmd, sizeof(display_cmd), "%s", command);
    }

    if (alt) attron(COLOR_PAIR(7));
    mvprintw(row, 0, "%5d  %-10s %2d %3d  %5.1f%% %5.1f%%  %-.*s",
             pid, user, nice, pri, cpu, mem, COLS - 35, display_cmd);
    if (alt) attroff(COLOR_PAIR(7));
}



void fetch_processes() {
    proc_count = 0;
    FILE *fp = popen("ps -eo pid,user,ni,pri,pcpu,pmem,args --sort=-%cpu", "r");
    if (!fp) return;

    char buffer[1024];
    fgets(buffer, sizeof(buffer), fp);

    while (fgets(buffer, sizeof(buffer), fp) && proc_count < MAX_PROCESSES) {
        Process p;
        if (sscanf(buffer, "%d %31s %d %d %f %f %255[^\n]",
                   &p.pid, p.user, &p.nice, &p.priority,
                   &p.cpu, &p.mem, p.command) == 7) {
            proc_list[proc_count++] = p;
        }
    }
    pclose(fp);
}

void display_processes() {
    int row = 5;
    int shown = 0;
    int available_rows = LINES - 7;
    matched_proc_count = 0;

    for (int i = 0; i < proc_count; i++) {
        if (strlen(search_term) == 0 || strstr(proc_list[i].command, search_term)) {
            matched_proc_count++;
        }
    }

    int skipped = 0;
    for (int i = 0; i < proc_count && shown < available_rows; i++) {
        if (strlen(search_term) == 0 || strstr(proc_list[i].command, search_term)) {
            if (skipped < scroll_offset) {
                skipped++;
                continue;
            }
            process_line(row++, proc_list[i].pid, proc_list[i].user,
                         proc_list[i].nice, proc_list[i].priority,
                         proc_list[i].cpu, proc_list[i].mem,
                         proc_list[i].command, shown % 2);
            shown++;
        }
    }

    if (strlen(search_term) > 0) {
        attron(COLOR_PAIR(6));
        mvprintw(LINES - 3, 2, "Filter: %s", search_term);
        attroff(COLOR_PAIR(6));
    }
}

void footer() {
    attron(A_DIM);
    mvprintw(LINES - 2, 0, "[q] Quit    [Up/Down] Scroll    [/] Search    [Updated every 0.25s]");
    attroff(A_DIM);
}

int main() {
    mkfifo(PIPE_PATH, 0666);

    initscr();
    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Your terminal does not support color\n");
        exit(1);
    }

    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    curs_set(0);
    halfdelay(1);
    init_colors();

    struct timespec delay = {.tv_sec = 0, .tv_nsec = 250 * 1000000};

    while (1) {
        int ch = getch();
        if (ch == ERR) {
            // No input
        } else if (ch == 'q') {
            break;
        } else if (ch == '/') {
            echo();
            curs_set(1);
            nocbreak();
            timeout(-1);
            mvprintw(LINES - 2, 2, "Search: ");
            getnstr(search_term, sizeof(search_term) - 1);
            noecho();
            curs_set(0);
            cbreak();
            halfdelay(1);
            scroll_offset = 0;
        } else if (ch == KEY_DOWN) {
            int max_scroll = matched_proc_count - (LINES - 7);
            if (scroll_offset < max_scroll) scroll_offset++;
            flushinp();
        } else if (ch == KEY_UP && scroll_offset > 0) {
            scroll_offset--;
            flushinp();
        }

        clear();
        float cpu = get_cpu_usage();
        float mem = get_memory_usage();
        write_usage_to_pipe(cpu, mem);

        draw_bar(0, "CPU Usage", cpu);
        draw_bar(1, "RAM Usage", mem);
        head();
        fetch_processes();
        display_processes();
        footer();
        refresh();

        nanosleep(&delay, NULL);
    }

    unlink(PIPE_PATH);
    endwin();
    return 0;
}