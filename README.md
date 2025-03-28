# 🖥️ macOS System Resource Monitor

## 📋 What I've Accomplished

I've successfully enhanced the macOS System Resource Monitor with several powerful new features and fixed compilation errors to ensure the code runs properly on macOS. The monitor now provides comprehensive system information with an intuitive, color-coded interface.

## ✨ Key Enhancements

### 🧠 Added Functionality
- **Network monitoring** with real-time download/upload speeds
- **Process tracking** for identifying resource-intensive applications
- **Enhanced battery information** including charging status, time remaining, and cycle count
- **System information** display showing hardware model, CPU details, and OS version
- **Total CPU load** in addition to per-core metrics

### 🎨 Visual Improvements
- **Color-coded indicators** that change from green to yellow to red based on usage
- **Visual load bars** for quick assessment of resource utilization
- **Better formatted output** with clear section headers and consistent spacing
- **Terminal colors** for improved readability and visual alerts

### 🛠️ Technical Improvements
- **Fixed string concatenation issues** that were causing compilation errors
- **Improved memory handling** throughout the application
- **More robust error checking** in system information gathering functions
- **Better data formatting** with proper use of stringstream
- **Code organization** with dedicated helper methods and classes

## 📊 Code Structure

The enhanced monitor is organized into three main components:

1. **TermColors Class** - Handles terminal color codes and visual formatting
2. **SystemMonitor Class** - Core functionality for gathering system metrics
3. **Main Loop** - Displays information in the terminal with a 2-second refresh rate

## 🔧 Compilation Instructions

```bash
# Compile with C++17 standard and required frameworks
g++ -std=c++17 monitor.cpp -o monitor -framework IOKit -framework CoreFoundation

# Run the monitor
./monitor
```

## 📸 Preview

When running, the monitor displays:

```
System Information:
  Model: MacBook Pro (16-inch, 2021)
  CPU: Apple M1 Pro
  CPU Cores: 10
  OS Version: macOS 12.5.1
  Total Memory: 16 GB
  Hostname: MacBook-Pro
  User: username

CPU Usage:
  Total: [██████████          ] 52%
  Core 0: [███████████         ] 58%
  Core 1: [████████            ] 42%
  ...

Memory Usage: [████████████        ] 62%
  10.15 GB / 16.00 GB

Disk Usage:
  /: [███████               ] 36%
    112.45 GB / 500.00 GB
  ...

Network Usage:
  en0:
    ↓ 1.25 MB/s
    ↑ 258.42 KB/s
  ...

Battery:
  Level: [███████████████     ] 78%
  State: Discharging
  Time Remaining: 3h 45m
  Cycle Count: 124
  Is Present: Yes

Top Processes:
  PID    | USER     | CPU%     | MEMORY     | NAME
  -------------------------------------------------
  1234   | username | 12%      | 1.2G       | Firefox
  ...

Press Ctrl+C to exit
```

The monitor refreshes every 2 seconds to provide real-time system information.
