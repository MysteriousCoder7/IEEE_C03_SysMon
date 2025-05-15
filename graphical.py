import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import os
import time

PIPE_PATH = '/tmp/sysmon_pipe'

# Ensure the named pipe exists
if not os.path.exists(PIPE_PATH):
    os.mkfifo(PIPE_PATH)

# Store the latest 50 data points
cpu_data = deque([0] * 50, maxlen=50)
mem_data = deque([0] * 50, maxlen=50)
load_data = deque([0] * 50, maxlen=50)

proc_count = 0  # From pipe
start_time = time.time()

# Setup plot
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 6))

line1, = ax1.plot(cpu_data, label='CPU %', color='blue')
line2, = ax2.plot(mem_data, label='Memory %', color='green')

# Ensure memory line is visible even at 100%
ax1.set_ylim(0, 110)
ax2.set_ylim(0, 110)

# Axis labels
ax1.set_ylabel("CPU Usage (%)")
ax2.set_ylabel("Memory Usage (%)")
ax2.set_xlabel("Time (samples)")

# Add grid and legend
for ax in (ax1, ax2):
    ax.grid(True)
    ax.legend(loc="upper right")

def get_system_uptime():
    try:
        with open("/proc/uptime", "r") as f:
            uptime_seconds = float(f.readline().split()[0])
            hrs, rem = divmod(int(uptime_seconds), 3600)
            mins, secs = divmod(rem, 60)
            return f"{hrs}h {mins}m {secs}s"
    except:
        return "unknown"

def get_total_processes():
    try:
        return sum(1 for pid in os.listdir('/proc') if pid.isdigit())
    except:
        return 0

def update(frame):
    global proc_count

    try:
        with open(PIPE_PATH, 'r') as pipe:
            line = pipe.readline().strip()
            cpu, mem, load, proc = map(float, line.split())
            cpu_data.append(cpu)
            mem_data.append(mem)
            load_data.append(load)
            proc_count = int(proc)
    except Exception as e:
        print("Error reading pipe:", e)
        return line1, line2

    uptime_str = get_system_uptime()
    total_proc = get_total_processes()

    # Update title with load, pipe process count, total process count, and uptime
    ax1.set_title(
        f"CPU Usage  |  Load Avg: {load_data[-1]:.2f}  |  From Pipe: {proc_count} procs  |  Total: {total_proc} procs  |  Uptime: {uptime_str}"
    )

    # Update plots
    line1.set_ydata(cpu_data)
    line1.set_xdata(range(len(cpu_data)))
    line2.set_ydata(mem_data)
    line2.set_xdata(range(len(mem_data)))

    return line1, line2

ani = animation.FuncAnimation(fig, update, interval=500)
plt.tight_layout()
plt.show()
