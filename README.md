Aim

The aim of the project is to build an interactive process manager.

It has two major segments:

    Creation of a process monitor.
    Custom feature addition - Visual analytics

Introduction

Effective system monitoring is essential for diagnosing performance bottlenecks and ensuring resource‑efficient operation of Linux hosts. While mature tools exist (e.g., htop, glances), a bespoke minimal monitor provides insight into the fundamentals of metric collection, inter‑process communication, and visualization pipelines. This project demonstrates a do‑it‑yourself approach: a C backend gathers and streams system statistics, and a Python frontend animates these metrics for intuitive trend analysis.
Technologies used
Languages :
    C
    Python 3

Tools :
    POSIX I/O (fopen, open, write, mkfifo, popen)
    matplotlib
    collections (deque)
    os, time
    ncurses

Data source :
    /proc/stat
    /proc/meminfo
    /proc/loadavg
    /proc/uptime
    ps command

Interprocess communication

    Named pipe (FIFO) at /tmp/sysmon_pipe

Methodology

Backend Data Collection (C)

    CPU usage: Read /proc/stat, compute deltas of user, nice, system, and idle times to derive percentage utilization .
    Memory usage: Parse /proc/meminfo for total and available memory to calculate used percentage .
    Load average and process count: Read the first field of /proc/loadavg and count numeric entries in /proc to estimate total processes.
    Process listing: Use popen("ps -eo pid,user,ni,pri,pcpu,pmem,args --sort=-%cpu") to capture the top processes by CPU usage, supporting search and scrolling .
    Terminal display: Leverage ncurses to render colorized bars for CPU and RAM, a header/footer with controls, and a paginated, searchable process table updated every 0.25 s .
    Interprocess communication: Create a named FIFO at /tmp/sysmon_pipe; on each update, write a line containing CPU%, mem%, load1, and process count .

Frontend Visualization (Python)

    Pipe reader: Ensure the FIFO exists; then on each 0.5 s interval, read one line, parse the four floats, and append to sliding deques of length 50 for CPU, memory, and load .
    Plotting: Use matplotlib.animation.FuncAnimation to update two subplots (CPU and memory) in real time, setting y‑axes to 0–110% and dynamically titling the CPU plot with current load, pipe process count, total processes, and uptime .
    Uptime and total processes: Independently read /proc/uptime and recount /proc entries each frame for contextual metrics.

Results

    Interactive terminal UI: The C monitor responds promptly to user input—search, scroll, and quit—while continuously updating bars and process lists without flicker.
    Smooth animated plots: The Python visualization displays CPU and memory utilization trends over the last 25 s (50 samples @ 0.5 s intervals), with clear gridlines and legends. Titles accurately reflect instantaneous load averages and process counts.
    Stability and resource footprint: Under typical system loads, the combined monitor consumes negligible CPU (<1%) and memory (<20 MB), demonstrating minimal overhead for continuous monitoring.
    
    Extensibility: The FIFO bridge decouples data collection from rendering, allowing additional frontends (e.g., web dashboards) to subscribe without modifying the C backend.

Conclusions

This minimal system monitor validates a modular approach to real‑time resource tracking: a performant C daemon streams raw metrics, and a Python client offers rich graphical insight. The architecture fosters easy extension—future work may include network traffic charts, historical logging, or a web‑based interface. Overall, the project underscores the core principles of low‑level metric gathering, inter‑process communication via FIFOs, and dynamic visualization.

Github : https://github.com/MysteriousCoder7/IEEE_C03_SysMon
Team
Mentors

    Saksham Kumar Singh
    Dhruv Girish Nayak

Mentees

    Shraddha Kovalli
    Vishal Murugan
    Adithya D
    Ankit Kumar
    Tarun P
