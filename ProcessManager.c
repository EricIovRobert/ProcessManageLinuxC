#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>


#define NUCLEE 6
struct MemorieProc{
    long size;
    long res;
};
struct Memorie{
    long total, liber, valabil;
};
struct IOProc{
    unsigned long read_bytes;
    unsigned long write_bytes;
};
struct CpuProc{
    long utime;
    long stime;
};
struct TimpCpu{
    long user, nice, system, idle, iowait, irq, softirq;
    long user_cores[NUCLEE], system_cores[NUCLEE], idle_cores[NUCLEE];
};

struct DiskStats{
    long reads_completed, reads_merged, sectors_read, time_reading, writes_completed, writes_merged, sectors_written, time_writing;
};

int read_process_io_info(const char *pid, struct IOProc *io_info){
    char path[50], line[256];
    snprintf(path, sizeof(path),"/proc/%s/io",pid);
    FILE *file = fopen(path,"r");
    if(!file){
        return -1;
    }
    while (fgets(line,sizeof(line),file)){
        if(sscanf(line,"read_bytes: %lu", &io_info->read_bytes)==1){};
        if(sscanf(line,"write_bytes: %lu", &io_info->write_bytes)==1){};
    }
    fclose(file);
    return 0;
}

int read_process_memory_info(const char *pid, struct MemorieProc *mem_info){
    char path[50];
    snprintf(path, sizeof(path),"/proc/%s/statm",pid);
    FILE *file = fopen(path,"r");
    if(!file){
        return -1;
    }
    if(fscanf(file,"%ld %ld", &mem_info->size, &mem_info->res)!=2){
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}
int read_process_cpu_info_time(const char *pid, struct CpuProc *cpu_time){
    char path[50], line[256];
    snprintf(path,sizeof(path),"/proc/%s/stat",pid);
    FILE *file = fopen(path,"r");
    if(!file){
        return -1;
    }
    if(fgets(line, sizeof(line),file)!=NULL){
        sscanf(line, "%*d %*s %*c %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %ld %ld", &cpu_time->utime, &cpu_time->stime);
    }
    else {
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;

}

int read_disk_stats(const char *device, struct DiskStats *stats){
    char path[256], line[256],dev_name[32];
    sprintf(path,"/proc/diskstats");
    FILE *file = fopen(path,"r");
    if(!file){
        return -1;
    }
    while(fgets(line, sizeof(line), file) != NULL){
        sscanf(line, "%*d %*d %31s %ld %ld %ld %ld %ld %ld %ld %ld",dev_name,&stats->reads_completed,&stats->reads_merged,&stats->sectors_read,&stats->time_reading,&stats->writes_completed,&stats->writes_merged,&stats->sectors_written,&stats->time_writing);
        if(strcmp(dev_name,device)==0){
            fclose(file);
            return 0;
        }

}
    fclose(file);
    return -1;
}

int read_memory_info(struct Memorie *mem_info){
    char line[256];
    FILE *file = fopen("/proc/meminfo","r");
    if(!file){
        return -1;
    }
    while(fgets(line,sizeof(line),file)!=NULL){
        if(strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %ld kB", &mem_info->total);
        }
        else if(strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line, "MemFree: %ld kB", &mem_info->liber);
        }
        else if(strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line, "MemAvailable: %ld kB", &mem_info->valabil);
        }

    }
    fclose(file);
    return 0;

}
int read_cpu_time(struct TimpCpu *cpu_time){
    char line[256];
    int core = -1;
    FILE *file = fopen("/proc/stat","r");
    if(!file){
        return -1;
    }
    while(fgets(line,sizeof(line),file)!=NULL){
        if(core == -1){
            if (sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld", &cpu_time->user, &cpu_time->nice, &cpu_time->system, &cpu_time->idle, &cpu_time->iowait, &cpu_time->irq, &cpu_time->softirq) != 7){
                fclose(file);
                return -1;
        }
        core++;
    }
    else if (core < NUCLEE && sscanf(line, "cpu%*d %ld %*d %ld %ld", &cpu_time->user_cores[core], &cpu_time->system_cores[core], &cpu_time->idle_cores[core]) == 3){
            core++;
        }
    }
    fclose(file);
    return 0;
}

double cpu_usage(struct TimpCpu *prev, struct TimpCpu *curr){
    long prev_idle = prev->idle + prev->iowait;
    long curr_idle = curr->idle + curr->iowait;
    long prev_total = prev->user + prev->nice + prev->system + prev->idle + prev->iowait + prev->irq + prev->softirq;
    long curr_total = curr->user + curr->nice + curr->system + curr->idle + curr->iowait + curr->irq + curr->softirq;
    long total_diff = curr_total - prev_total;
    long idle_diff = curr_idle - prev_idle;

    if(total_diff == 0) return 0.0;
    return ((total_diff - idle_diff) * 100.0) / total_diff;

}
double memory_usage(struct Memorie *mem_info){
    return ((mem_info->total - mem_info->liber) * 100.0) / mem_info->total;
}

const char* get_username(uid_t uid) {
	    struct passwd *pw = getpwuid(uid);
	    return pw ? pw->pw_name : "";
	}

void afis_process_info(const char*pid, struct TimpCpu *curr, struct Memorie *mem_info){
    char path[50], line[256], nume[256];
    uid_t uid;
    snprintf(path,sizeof(path),"/proc/%s/status",pid);
    FILE* file = fopen(path, "r");
    if(!file){
        return;
    }
    uid = -1;
    while(fgets(line,sizeof(line),file)){
        if(strncmp(line,"Name:",5)==0){
            sscanf(line,"Name:\t%255s",nume);
        }
        else if (strncmp(line,"Uid:",4)==0){
            sscanf(line,"Uid:\t%u",&uid);
            break;
        }
    }
    fclose(file);
    struct CpuProc cpu_proc;
    struct MemorieProc mem_proc;
    double cpu_usage1 = 0.0, mem_usage1 = 0.0;

    if(read_process_cpu_info_time(pid,&cpu_proc)!=-1 && read_process_memory_info(pid,&mem_proc)!= -1){
        long timp_total = cpu_proc.utime + cpu_proc.stime;
        long total_cpu_core = 0;
        for(int i = 0; i < NUCLEE;i++){
            total_cpu_core += curr->user_cores[i] + curr->system_cores[i] + curr->idle_cores[i];
        }
        cpu_usage1 = 100.0 * timp_total / total_cpu_core;
        mem_usage1 = 100.0 * mem_proc.res * sysconf(_SC_PAGESIZE)/(mem_info->total *1024.0);
    }
    const char *tip = (uid == 0) ? "System" : "User";
    struct IOProc io_info;
    if(read_process_io_info(pid,&io_info)==0){
        char data[19];
        snprintf(data,sizeof(data), "%9lu/%-8lu", io_info.read_bytes, io_info.write_bytes);
        printf("| %-39s | %-18s | %-8s | %10.2f%% | %14.2f%% | %-9s | %-18s |\n", nume, get_username(uid),pid,cpu_usage1,mem_usage1,tip,data);

    }
    else {
       printf("| %-39s | %-18s | %-8s | %10.2f%% | %14.2f%% | %-9s | %-18s |\n", nume, get_username(uid),pid,cpu_usage1,mem_usage1,tip,"N/A         ");
    }

}

void print_header() {
	printf("--------------------------------------------------------------------------------------------------------------------------------------------\n");
   	 printf("| %-39s | %-18s | %-8s | %-11s | %-15s | %-9s | %-18s |\n",
	"Process Name", "User", "ID", "CPU Usage", "Memory Usage", "Type", "I/O (Read/Write)");
	printf("--------------------------------------------------------------------------------------------------------------------------------------------\n");
}


int main()
{
    struct TimpCpu prev_cpu, curr_cpu;
    struct Memorie mem_info;
    struct DiskStats disk_stats;
    double cpu_usage1, mem_usage1;
    read_cpu_time(&prev_cpu);
    sleep(2);
    while(1){
        system("clear");
        read_cpu_time(&curr_cpu);
        cpu_usage1 = cpu_usage(&prev_cpu,&curr_cpu);
        prev_cpu = curr_cpu;
        read_memory_info(&mem_info);
        mem_usage1 = memory_usage(&mem_info);
        printf("CPU Usage: %.2f%%\n", cpu_usage1);
        printf("Memory Usage: %.2f%%\n", mem_usage1);
        if (read_disk_stats("sda", &disk_stats) != -1) {
          printf("Disk Reads: %ld\n", disk_stats.reads_completed);
          printf("Disk Writes: %ld\n", disk_stats.writes_completed);
          printf("Data Read: %ld\n", disk_stats.sectors_read);
          printf("Data Written: %ld\n", disk_stats.sectors_written);
        }
        print_header();
        DIR* proc = opendir("/proc");
        struct dirent* ent;
        char* endptr;
         while ((ent = readdir(proc)) != NULL) {
            long lpid = strtol(ent->d_name, &endptr, 10);
            if(*endptr =='\0'){
                afis_process_info(ent->d_name,&curr_cpu,&mem_info);
            }
         }
         closedir(proc);
printf("--------------------------------------------------------------------------------------------------------------------------------------------\n");

        sleep(2);

    }

    return 0;
}
