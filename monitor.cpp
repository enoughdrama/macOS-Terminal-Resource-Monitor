#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <map>
#include <ctime>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <libproc.h>
#include <pwd.h>
#include <unistd.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

class TermColors {
public:
    static const std::string Reset;
    static const std::string Bold;
    static const std::string Black;
    static const std::string Red;
    static const std::string Green;
    static const std::string Yellow;
    static const std::string Blue;
    static const std::string Magenta;
    static const std::string Cyan;
    static const std::string White;
    
    static std::string colorizePercent(double percent) {
        if (percent >= 90.0) return Red + std::to_string(static_cast<int>(percent)) + "%" + Reset;
        if (percent >= 70.0) return Yellow + std::to_string(static_cast<int>(percent)) + "%" + Reset;
        return Green + std::to_string(static_cast<int>(percent)) + "%" + Reset;
    }
    
    static std::string getLoadBar(double percent, int width = 20) {
        int numChars = static_cast<int>((percent / 100.0) * width);
        std::string bar = "[";
        
        for (int i = 0; i < width; i++) {
            if (i < numChars) {
                if (percent >= 90.0) bar += Red + "█" + Reset;
                else if (percent >= 70.0) bar += Yellow + "█" + Reset;
                else bar += Green + "█" + Reset;
            } else {
                bar += " ";
            }
        }
        
        bar += "] " + colorizePercent(percent);
        return bar;
    }
};

const std::string TermColors::Reset = "\033[0m";
const std::string TermColors::Bold = "\033[1m";
const std::string TermColors::Black = "\033[30m";
const std::string TermColors::Red = "\033[31m";
const std::string TermColors::Green = "\033[32m";
const std::string TermColors::Yellow = "\033[33m";
const std::string TermColors::Blue = "\033[34m";
const std::string TermColors::Magenta = "\033[35m";
const std::string TermColors::Cyan = "\033[36m";
const std::string TermColors::White = "\033[37m";

class SystemMonitor {
private:
    struct CPUInfo {
        uint64_t user;
        uint64_t system;
        uint64_t idle;
        uint64_t nice;
    };
    
    struct NetworkInfo {
        uint64_t bytes_in;
        uint64_t bytes_out;
    };
    
    struct ProcessInfo {
        pid_t pid;
        std::string name;
        std::string user;
        double cpu_percent;
        uint64_t memory;
    };
    
    std::vector<CPUInfo> prev_cpu_info;
    std::map<std::string, NetworkInfo> prev_net_info;
    time_t prev_net_time;
    
    double calculateCPULoad(const CPUInfo& prev, const CPUInfo& current) {
        uint64_t prev_total = prev.user + prev.system + prev.idle + prev.nice;
        uint64_t current_total = current.user + current.system + current.idle + current.nice;
        
        uint64_t total_diff = current_total - prev_total;
        uint64_t idle_diff = current.idle - prev.idle;
        
        if (total_diff == 0) return 0.0;
        
        return 100.0 * (1.0 - static_cast<double>(idle_diff) / total_diff);
    }
    
    std::string formatBytes(uint64_t bytes) {
        constexpr double KB = 1024;
        constexpr double MB = 1024 * KB;
        constexpr double GB = 1024 * MB;
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        
        if (bytes < KB) {
            ss << bytes << " B";
        } else if (bytes < MB) {
            ss << (bytes / KB) << " KB";
        } else if (bytes < GB) {
            ss << (bytes / MB) << " MB";
        } else {
            ss << (bytes / GB) << " GB";
        }
        
        return ss.str();
    }
    
    std::string formatBytesPerSec(uint64_t bytes) {
        constexpr double KB = 1024;
        constexpr double MB = 1024 * KB;
        constexpr double GB = 1024 * MB;
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        
        if (bytes < KB) {
            ss << bytes << " B/s";
        } else if (bytes < MB) {
            ss << (bytes / KB) << " KB/s";
        } else if (bytes < GB) {
            ss << (bytes / MB) << " MB/s";
        } else {
            ss << (bytes / GB) << " GB/s";
        }
        
        return ss.str();
    }
    
public:
    SystemMonitor() {
        natural_t cpu_count;
        processor_info_array_t cpu_info;
        mach_msg_type_number_t cpu_info_count;
        
        kern_return_t kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                               &cpu_count, &cpu_info, &cpu_info_count);
        
        if (kr == KERN_SUCCESS) {
            prev_cpu_info.resize(cpu_count);
            processor_cpu_load_info_t cpu_load_info = (processor_cpu_load_info_t)cpu_info;
            
            for (unsigned i = 0; i < cpu_count; ++i) {
                prev_cpu_info[i].user = cpu_load_info[i].cpu_ticks[CPU_STATE_USER];
                prev_cpu_info[i].system = cpu_load_info[i].cpu_ticks[CPU_STATE_SYSTEM];
                prev_cpu_info[i].idle = cpu_load_info[i].cpu_ticks[CPU_STATE_IDLE];
                prev_cpu_info[i].nice = cpu_load_info[i].cpu_ticks[CPU_STATE_NICE];
            }
            
            vm_deallocate(mach_task_self(), (vm_address_t)cpu_info, cpu_info_count * sizeof(int));
        }
        
        updateNetworkInfo();
        prev_net_time = time(nullptr);
    }
    
    std::vector<double> getCPUUsage() {
        natural_t cpu_count;
        processor_info_array_t cpu_info;
        mach_msg_type_number_t cpu_info_count;
        
        std::vector<double> cpu_usage;
        
        kern_return_t kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                                               &cpu_count, &cpu_info, &cpu_info_count);
        
        if (kr == KERN_SUCCESS) {
            processor_cpu_load_info_t cpu_load_info = (processor_cpu_load_info_t)cpu_info;
            
            cpu_usage.resize(cpu_count);
            for (unsigned i = 0; i < cpu_count; ++i) {
                CPUInfo current;
                current.user = cpu_load_info[i].cpu_ticks[CPU_STATE_USER];
                current.system = cpu_load_info[i].cpu_ticks[CPU_STATE_SYSTEM];
                current.idle = cpu_load_info[i].cpu_ticks[CPU_STATE_IDLE];
                current.nice = cpu_load_info[i].cpu_ticks[CPU_STATE_NICE];
                
                cpu_usage[i] = calculateCPULoad(prev_cpu_info[i], current);
                prev_cpu_info[i] = current;
            }
            
            vm_deallocate(mach_task_self(), (vm_address_t)cpu_info, cpu_info_count * sizeof(int));
        }
        
        return cpu_usage;
    }
    
    double getTotalCPUUsage() {
        std::vector<double> cpu_usage = getCPUUsage();
        if (cpu_usage.empty()) return 0.0;
        
        double total = 0.0;
        for (double usage : cpu_usage) {
            total += usage;
        }
        
        return total / cpu_usage.size();
    }
    
    std::pair<double, double> getMemoryUsage() {
        vm_size_t page_size;
        mach_port_t host_port = mach_host_self();
        vm_statistics64_data_t vm_stats;
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        
        host_page_size(host_port, &page_size);
        
        if (host_statistics64(host_port, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) != KERN_SUCCESS) {
            return {0.0, 0.0};
        }
        
        uint64_t free_memory = vm_stats.free_count * page_size;
        uint64_t used_memory = (vm_stats.active_count + vm_stats.wire_count) * page_size;
        uint64_t total_memory = used_memory + free_memory + (vm_stats.inactive_count * page_size);
        
        double used_gb = static_cast<double>(used_memory) / (1024 * 1024 * 1024);
        double total_gb = static_cast<double>(total_memory) / (1024 * 1024 * 1024);
        
        return {used_gb, total_gb};
    }
    
    std::vector<std::pair<std::string, double>> getDiskUsage() {
        struct statfs *mounts;
        int num_mounts = getmntinfo(&mounts, MNT_WAIT);
        
        std::vector<std::pair<std::string, double>> disk_usage;
        
        for (int i = 0; i < num_mounts; ++i) {
            if (strncmp(mounts[i].f_fstypename, "devfs", sizeof("devfs")) == 0) {
                continue;
            }
            
            uint64_t total = mounts[i].f_blocks * mounts[i].f_bsize;
            uint64_t free = mounts[i].f_bfree * mounts[i].f_bsize;
            uint64_t used = total - free;
            
            double usage_percent = 0.0;
            if (total > 0) {
                usage_percent = 100.0 * static_cast<double>(used) / total;
            }
            
            disk_usage.push_back({mounts[i].f_mntonname, usage_percent});
        }
        
        return disk_usage;
    }
    
    std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> getDiskSizes() {
        struct statfs *mounts;
        int num_mounts = getmntinfo(&mounts, MNT_WAIT);
        
        std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> disk_sizes;
        
        for (int i = 0; i < num_mounts; ++i) {
            if (strncmp(mounts[i].f_fstypename, "devfs", sizeof("devfs")) == 0) {
                continue;
            }
            
            uint64_t total = mounts[i].f_blocks * mounts[i].f_bsize;
            uint64_t free = mounts[i].f_bfree * mounts[i].f_bsize;
            uint64_t used = total - free;
            
            disk_sizes.push_back({mounts[i].f_mntonname, {used, total}});
        }
        
        return disk_sizes;
    }
    
    std::map<std::string, std::pair<double, double>> getNetworkUsage() {
        std::map<std::string, NetworkInfo> current_net_info;
        updateNetworkInfo(&current_net_info);
        
        time_t current_time = time(nullptr);
        double time_diff = difftime(current_time, prev_net_time);
        if (time_diff <= 0) time_diff = 1.0;
        
        std::map<std::string, std::pair<double, double>> net_usage;
        
        for (const auto& [interface, current] : current_net_info) {
            if (prev_net_info.find(interface) != prev_net_info.end()) {
                const auto& prev = prev_net_info[interface];
                
                uint64_t in_diff = current.bytes_in - prev.bytes_in;
                uint64_t out_diff = current.bytes_out - prev.bytes_out;
                
                double in_rate = static_cast<double>(in_diff) / time_diff;
                double out_rate = static_cast<double>(out_diff) / time_diff;
                
                net_usage[interface] = {in_rate, out_rate};
            }
        }
        
        prev_net_info = current_net_info;
        prev_net_time = current_time;
        
        return net_usage;
    }
    
    void updateNetworkInfo(std::map<std::string, NetworkInfo>* net_info = nullptr) {
        std::map<std::string, NetworkInfo>& info = net_info ? *net_info : prev_net_info;
        info.clear();
        
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) == -1) {
            return;
        }
        
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_name == nullptr) {
                continue;
            }
            
            if (ifa->ifa_addr->sa_family == AF_LINK) {
                std::string iface_name = ifa->ifa_name;
                if (iface_name == "lo0") continue;
                
                struct if_data *stats = (struct if_data *)ifa->ifa_data;
                info[iface_name].bytes_in = stats->ifi_ibytes;
                info[iface_name].bytes_out = stats->ifi_obytes;
            }
        }
        
        freeifaddrs(ifaddr);
    }
    
    std::vector<ProcessInfo> getTopProcesses(size_t count = 10) {
        std::vector<ProcessInfo> processes;
        
        int pid_count = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
        if (pid_count <= 0) {
            return processes;
        }
        
        std::vector<pid_t> pids(pid_count);
        pid_count = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), pid_count * sizeof(pid_t));
        pids.resize(pid_count / sizeof(pid_t));
        
        for (pid_t pid : pids) {
            if (pid <= 0) continue;
            
            char name[PROC_PIDPATHINFO_MAXSIZE];
            if (proc_name(pid, name, sizeof(name)) <= 0) {
                continue;
            }
            
            struct proc_taskinfo task_info;
            if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &task_info, sizeof(task_info)) <= 0) {
                continue;
            }
            
            uid_t uid;
            struct proc_bsdinfo bsd_info;
            if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsd_info, sizeof(bsd_info)) > 0) {
                uid = bsd_info.pbi_uid;
            } else {
                continue;
            }
            
            struct passwd *pw = getpwuid(uid);
            std::string username = pw ? pw->pw_name : std::to_string(uid);
            
            double cpu_percent = 100.0 * (static_cast<double>(task_info.pti_total_user) +
                                         static_cast<double>(task_info.pti_total_system)) /
                                        NSEC_PER_SEC / sysconf(_SC_NPROCESSORS_CONF);
            
            ProcessInfo process;
            process.pid = pid;
            process.name = name;
            process.user = username;
            process.cpu_percent = cpu_percent;
            process.memory = task_info.pti_resident_size;
            
            processes.push_back(process);
        }
        
        std::sort(processes.begin(), processes.end(),
                 [](const ProcessInfo& a, const ProcessInfo& b) {
                     return a.cpu_percent > b.cpu_percent;
                 });
        
        if (processes.size() > count) {
            processes.resize(count);
        }
        
        return processes;
    }
    
    std::map<std::string, std::string> getBatteryInfo() {
        std::map<std::string, std::string> battery_info;
        
        CFTypeRef power_sources = IOPSCopyPowerSourcesInfo();
        CFArrayRef power_source_list = IOPSCopyPowerSourcesList(power_sources);
        
        for (CFIndex i = 0; i < CFArrayGetCount(power_source_list); ++i) {
            CFDictionaryRef power_source = IOPSGetPowerSourceDescription(power_sources,
                                                                   CFArrayGetValueAtIndex(power_source_list, i));
            
            if (!power_source) continue;
            
            CFStringRef power_source_state = (CFStringRef)CFDictionaryGetValue(power_source,
                                                                              CFSTR(kIOPSPowerSourceStateKey));
            
            if (!power_source_state) continue;
            
            if (CFStringCompare(power_source_state, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo ||
                CFStringCompare(power_source_state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo) {
                
                CFNumberRef percent_ref = (CFNumberRef)CFDictionaryGetValue(power_source,
                                                                           CFSTR(kIOPSCurrentCapacityKey));
                if (percent_ref) {
                    int percent;
                    CFNumberGetValue(percent_ref, kCFNumberIntType, &percent);
                    battery_info["Percentage"] = std::to_string(percent) + "%";
                }
                
                if (CFStringCompare(power_source_state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo) {
                    battery_info["State"] = "Charging";
                } else {
                    battery_info["State"] = "Discharging";
                }
                
                CFNumberRef time_ref = (CFNumberRef)CFDictionaryGetValue(power_source,
                                                                        CFSTR(kIOPSTimeToEmptyKey));
                if (time_ref) {
                    int minutes;
                    CFNumberGetValue(time_ref, kCFNumberIntType, &minutes);
                    if (minutes > 0) {
                        int hours = minutes / 60;
                        int mins = minutes % 60;
                        battery_info["Time Remaining"] = std::to_string(hours) + "h " +
                                                         std::to_string(mins) + "m";
                    }
                }
                
                // Check if dictionary contains cycle count key
                CFTypeRef cycles_ref = CFDictionaryGetValue(power_source, CFSTR("CycleCount"));
                if (cycles_ref && CFGetTypeID(cycles_ref) == CFNumberGetTypeID()) {
                    int cycles;
                    CFNumberGetValue((CFNumberRef)cycles_ref, kCFNumberIntType, &cycles);
                    battery_info["Cycle Count"] = std::to_string(cycles);
                }
                
                CFBooleanRef isPresent = (CFBooleanRef)CFDictionaryGetValue(power_source,
                                                                           CFSTR(kIOPSIsPresentKey));
                if (isPresent) {
                    battery_info["Is Present"] = CFBooleanGetValue(isPresent) ? "Yes" : "No";
                }
                
                break;
            }
        }
        
        CFRelease(power_source_list);
        CFRelease(power_sources);
        
        return battery_info;
    }
    
    std::map<std::string, std::string> getSystemInfo() {
        std::map<std::string, std::string> sys_info;
        
        char buffer[1024];
        size_t size = sizeof(buffer);
        
        if (sysctlbyname("hw.model", buffer, &size, nullptr, 0) == 0) {
            sys_info["Model"] = buffer;
        }
        
        size = sizeof(buffer);
        if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, nullptr, 0) == 0) {
            sys_info["CPU"] = buffer;
        }
        
        int cores;
        size = sizeof(cores);
        if (sysctlbyname("hw.ncpu", &cores, &size, nullptr, 0) == 0) {
            sys_info["CPU Cores"] = std::to_string(cores);
        }
        
        size = sizeof(buffer);
        if (sysctlbyname("kern.osversion", buffer, &size, nullptr, 0) == 0) {
            sys_info["OS Version"] = "macOS " + std::string(buffer);
        }
        
        int64_t memsize;
        size = sizeof(memsize);
        if (sysctlbyname("hw.memsize", &memsize, &size, nullptr, 0) == 0) {
            int64_t memGiB = memsize / (1024 * 1024 * 1024);
            sys_info["Total Memory"] = std::to_string(memGiB) + " GB";
        }
        
        char hostname[1024];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            sys_info["Hostname"] = hostname;
        }
        
        char username[1024];
        if (getlogin_r(username, sizeof(username)) == 0) {
            sys_info["User"] = username;
        }
        
        return sys_info;
    }
};

int main() {
    SystemMonitor monitor;
    
    while (true) {
        std::system("clear");
        std::cout << TermColors::Bold + TermColors::Cyan + "=== macOS System Resource Monitor ===" + TermColors::Reset << std::endl << std::endl;
        
        time_t now = time(nullptr);
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));
        std::cout << "Time: " << time_str << std::endl << std::endl;
        
        auto sys_info = monitor.getSystemInfo();
        std::cout << TermColors::Bold + TermColors::Blue + "System Information:" + TermColors::Reset << std::endl;
        for (const auto& [key, value] : sys_info) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
        std::cout << std::endl;
        
        double total_cpu = monitor.getTotalCPUUsage();
        std::cout << TermColors::Bold + TermColors::Blue + "CPU Usage:" + TermColors::Reset << std::endl;
        std::cout << "  Total: " << TermColors::getLoadBar(total_cpu) << std::endl;
        
        std::vector<double> cpu_usage = monitor.getCPUUsage();
        for (size_t i = 0; i < cpu_usage.size(); ++i) {
            std::cout << "  Core " << i << ": " << TermColors::getLoadBar(cpu_usage[i]) << std::endl;
        }
        std::cout << std::endl;
        
        auto [used_memory, total_memory] = monitor.getMemoryUsage();
        double memory_percent = (used_memory / total_memory) * 100.0;
        std::cout << TermColors::Bold + TermColors::Blue + "Memory Usage:" + TermColors::Reset << " "
                  << TermColors::getLoadBar(memory_percent) << std::endl;
        std::cout << "  " << std::fixed << std::setprecision(2) << used_memory
                  << " GB / " << total_memory << " GB" << std::endl << std::endl;
        
        auto disk_sizes = monitor.getDiskSizes();
        std::cout << TermColors::Bold + TermColors::Blue + "Disk Usage:" + TermColors::Reset << std::endl;
        for (const auto& [mount_point, sizes] : disk_sizes) {
            uint64_t used = sizes.first;
            uint64_t total = sizes.second;
            double usage_percent = 0.0;
            if (total > 0) {
                usage_percent = 100.0 * static_cast<double>(used) / total;
            }
            
            double used_gb = static_cast<double>(used) / (1024 * 1024 * 1024);
            double total_gb = static_cast<double>(total) / (1024 * 1024 * 1024);
            
            std::cout << "  " << mount_point << ": " << TermColors::getLoadBar(usage_percent) << std::endl;
            std::cout << "    " << std::fixed << std::setprecision(2) << used_gb
                      << " GB / " << total_gb << " GB" << std::endl;
        }
        std::cout << std::endl;
        
        auto net_usage = monitor.getNetworkUsage();
        std::cout << TermColors::Bold + TermColors::Blue + "Network Usage:" + TermColors::Reset << std::endl;
        for (const auto& [interface, rates] : net_usage) {
            double in_rate = rates.first;
            double out_rate = rates.second;
            
            std::stringstream in_ss, out_ss;
            in_ss << std::fixed << std::setprecision(2);
            out_ss << std::fixed << std::setprecision(2);
            
            if (in_rate < 1024) in_ss << in_rate << " B/s";
            else if (in_rate < 1024 * 1024) in_ss << (in_rate / 1024) << " KB/s";
            else if (in_rate < 1024 * 1024 * 1024) in_ss << (in_rate / (1024 * 1024)) << " MB/s";
            else in_ss << (in_rate / (1024 * 1024 * 1024)) << " GB/s";
            
            if (out_rate < 1024) out_ss << out_rate << " B/s";
            else if (out_rate < 1024 * 1024) out_ss << (out_rate / 1024) << " KB/s";
            else if (out_rate < 1024 * 1024 * 1024) out_ss << (out_rate / (1024 * 1024)) << " MB/s";
            else out_ss << (out_rate / (1024 * 1024 * 1024)) << " GB/s";
            
            std::cout << "  " << interface << ":" << std::endl;
            std::cout << "    ↓ " << in_ss.str() << std::endl;
            std::cout << "    ↑ " << out_ss.str() << std::endl;
        }
        std::cout << std::endl;
        
        auto battery_info = monitor.getBatteryInfo();
        if (!battery_info.empty()) {
            std::cout << TermColors::Bold + TermColors::Blue + "Battery:" + TermColors::Reset << std::endl;
            double battery_percent = -1.0;
            if (battery_info.find("Percentage") != battery_info.end()) {
                battery_percent = std::stod(battery_info["Percentage"]);
            }
            
            if (battery_percent >= 0) {
                std::cout << "  Level: " << TermColors::getLoadBar(battery_percent) << std::endl;
            }
            
            for (const auto& [key, value] : battery_info) {
                if (key != "Percentage") {
                    std::cout << "  " << key << ": " << value << std::endl;
                }
            }
            std::cout << std::endl;
        }
        
        auto processes = monitor.getTopProcesses(5);
        std::cout << TermColors::Bold + TermColors::Blue + "Top Processes:" + TermColors::Reset << std::endl;
        std::cout << "  " << std::setw(6) << "PID" << " | "
                  << std::setw(8) << "USER" << " | "
                  << std::setw(8) << "CPU%" << " | "
                  << std::setw(10) << "MEMORY" << " | "
                  << "NAME" << std::endl;
        
        std::cout << "  " << std::string(50, '-') << std::endl;
        for (const auto& proc : processes) {
            double mem_mb = static_cast<double>(proc.memory) / (1024 * 1024);
            std::stringstream mem_ss;
            mem_ss << std::fixed << std::setprecision(1);
            
            if (mem_mb < 1024) mem_ss << mem_mb << "M";
            else mem_ss << (mem_mb / 1024) << "G";
            
            std::cout << "  " << std::setw(6) << proc.pid << " | "
                      << std::setw(8) << proc.user << " | ";
            
            std::string cpu_str = std::to_string(static_cast<int>(proc.cpu_percent)) + "%";
            if (proc.cpu_percent >= 50.0) {
                std::cout << std::setw(8) << (TermColors::Red + cpu_str + TermColors::Reset) << " | ";
            } else if (proc.cpu_percent >= 20.0) {
                std::cout << std::setw(8) << (TermColors::Yellow + cpu_str + TermColors::Reset) << " | ";
            } else {
                std::cout << std::setw(8) << (TermColors::Green + cpu_str + TermColors::Reset) << " | ";
            }
            
            std::cout << std::setw(10) << mem_ss.str() << " | "
                      << proc.name << std::endl;
        }
        std::cout << std::endl;
        
        std::cout << TermColors::Bold + "Press Ctrl+C to exit" + TermColors::Reset << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    return 0;
}
