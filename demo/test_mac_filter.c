/*
 * MAC Filter Logic Test Demo
 * 用于验证include_mac_filter函数的算法逻辑
 * 在macOS上编译运行: gcc test_mac_filter.c -o test_mac_filter && ./test_mac_filter
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// 模拟NVRAM数据存储
#define MAX_MAC_ENTRIES 100
#define MAX_MAC_ADDR 64
#define MAX_TIME_RULES 32

typedef struct {
    char mac[18];           // MAC地址 (XX:XX:XX:XX:XX:XX)
    char time_rules[MAX_TIME_RULES][160]; // 时间规则数组
    int time_rule_count;    // 时间规则数量
    int has_drop_rule;      // 是否已添加DROP规则
} mac_entry_t;

// 模拟全局数据
mac_entry_t mac_entries[MAX_MAC_ADDR];
int mac_count = 0;

// 模拟的NVRAM数据
char macfilter_list_x[MAX_MAC_ENTRIES][32];  // 存储MAC地址
char macfilter_date_x[MAX_MAC_ENTRIES][32];  // 存储日期规则
char macfilter_time_x[MAX_MAC_ENTRIES][32];  // 存储时间规则
int macfilter_num_x = 0;  // 总规则数

// 模拟函数
void g_buf_init(void) {
    // 模拟缓冲区初始化
}

char* mac_conv(const char* list_name, int index, char* mac_buf) {
    strcpy(mac_buf, macfilter_list_x[index]);
    return (*mac_buf) ? mac_buf : "";
}

int nvram_get_int(const char* key) {
    if (strcmp(key, "macfilter_num_x") == 0) return macfilter_num_x;
    return 0;
}

char* nvram_safe_get(const char* key) {
    static char temp[32];
    int index = atoi(key + strlen("macfilter_date_x"));
    if (index >= 0 && index < macfilter_num_x) {
        strcpy(temp, macfilter_date_x[index]);
        return temp;
    }
    return "";
}

void timematch_conv(char* output, const char* date, const char* time) {
    // 简单模拟：如果时间和日期都有效，生成时间匹配规则
    if (*date && *time) {
        sprintf(output, " --timestart %s --timestop %s", time, time);
    } else {
        *output = '\0';
    }
}

void logmessage(const char* module, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[%s] ", module);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

// 核心测试函数 - 模拟include_mac_filter的逻辑
int test_include_mac_filter(int mac_filter_mode, const char* logdrop) {
    int i, mac_num;
    char mac_timematch[128], mac_buf[24], nv_date[32], nv_time[32];
    char *filter_mac;
    const char *dtype = "maclist";
    const char *ftype;
    FILE *fp = stdout;  // 输出到控制台

    if (mac_filter_mode > 0) {
        if (mac_filter_mode == 2) {
            // 拒绝模式: 每个MAC对应多条时间规则，DROP规则在最后
            
            mac_count = 0;
            mac_num = 0;
            
            // 第一轮：收集所有MAC及其时间规则
            for (i = 0; i < macfilter_num_x; i++) {
                g_buf_init();
                filter_mac = mac_conv("macfilter_list_x", i, mac_buf);
                
                if (!*filter_mac) continue;
                
                // 查找或创建MAC条目
                mac_entry_t *current_mac_entry = NULL;
                int mac_found = 0;
                
                for (int j = 0; j < mac_count; j++) {
                    if (strcasecmp(mac_entries[j].mac, filter_mac) == 0) {
                        current_mac_entry = &mac_entries[j];
                        mac_found = 1;
                        break;
                    }
                }
                
                if (!mac_found) {
                    // 新MAC地址
                    if (mac_count >= MAX_MAC_ADDR) {
                        logmessage("MAC Filter", "WARNING: MAC limit reached, skipping %s", filter_mac);
                        continue;
                    }
                    
                    current_mac_entry = &mac_entries[mac_count];
                    strcpy(current_mac_entry->mac, filter_mac);
                    current_mac_entry->time_rule_count = 0;
                    current_mac_entry->has_drop_rule = 0;
                    mac_count++;
                }
                
                // 获取时间规则
                sprintf(nv_date, "macfilter_date_x%d", i);
                sprintf(nv_time, "macfilter_time_x%d", i);
                timematch_conv(mac_timematch, nv_date, nv_time);
                
                if (strlen(mac_timematch) > 0 && current_mac_entry->time_rule_count < MAX_TIME_RULES) {
                    // 检查时间规则是否已存在
                    int time_rule_exists = 0;
                    for (int r = 0; r < current_mac_entry->time_rule_count; r++) {
                        if (strcmp(current_mac_entry->time_rules[r], mac_timematch) == 0) {
                            time_rule_exists = 1;
                            break;
                        }
                    }
                    
                    if (!time_rule_exists) {
                        // 添加新的时间规则
                        strcpy(current_mac_entry->time_rules[current_mac_entry->time_rule_count], mac_timematch);
                        current_mac_entry->time_rule_count++;
                        mac_num++;
                    }
                }
            }
            
            // 第二轮：为每个MAC生成规则（先时间规则，最后DROP规则）
            for (int j = 0; j < mac_count; j++) {
                mac_entry_t *entry = &mac_entries[j];
                
                //logmessage("MAC Filter", "DEBUG: Processing MAC %s with %d time rules", 
                        //    entry->mac, entry->time_rule_count);
                
                // 生成所有时间允许规则
                for (int r = 0; r < entry->time_rule_count; r++) {
                    fprintf(fp, "-A %s -m mac --mac-source %s%s -j RETURN\n", 
                            dtype, entry->mac, entry->time_rules[r]);
                    
                    //logmessage("MAC Filter", "DEBUG: Added time rule for MAC %s: %s", 
                            //    entry->mac, entry->time_rules[r]);
                }
                
                // 最后添加DROP规则（确保是该MAC的最后一条规则）
                if (entry->time_rule_count > 0) {
                    fprintf(fp, "-A %s -m mac --mac-source %s -j %s\n", 
                            dtype, entry->mac, logdrop);
                    
                    //logmessage("MAC Filter", "DEBUG: Added DROP rule for MAC %s", entry->mac);
                }
            }
            
            logmessage("MAC Filter", "INFO: Processed %d unique MACs with %d total rules in deny mode", 
                       mac_count, mac_num);
        }
        else {
            // 允许模式: 列表中的设备允许，其他设备拒绝
            ftype = "RETURN";
            mac_num = 0;
            
            for (i = 0; i < macfilter_num_x; i++) {
                g_buf_init();
                filter_mac = mac_conv("macfilter_list_x", i, mac_buf);
                
                if (!*filter_mac) continue;
                
                sprintf(nv_date, "macfilter_date_x%d", i);
                sprintf(nv_time, "macfilter_time_x%d", i);
                timematch_conv(mac_timematch, nv_date, nv_time);
                
                fprintf(fp, "-A %s -m mac --mac-source %s%s -j %s\n", 
                        dtype, filter_mac, mac_timematch, ftype);
                
                mac_num++;
                logmessage("MAC Filter", "DEBUG: Added allow rule for MAC %s: %s", 
                           filter_mac, mac_timematch);
            }
            
            if (mac_num > 0) {
                fprintf(fp, "-A %s -j %s\n", dtype, logdrop);
                //logmessage("MAC Filter", "INFO: Processed %d MAC entries in allow mode", mac_num);
            }
        }
        
        if (mac_num < 1)
            mac_filter_mode = 0;
    }

    return mac_filter_mode;
}

// 测试数据初始化
void init_test_data() {
    // 测试场景1：拒绝模式，多个MAC，有重复时间规则
    printf("=== 测试场景1：拒绝模式，多个MAC，有重复时间规则 ===\n");
    
    macfilter_num_x = 8;
    
    // MAC1: 11:22:33:44:55:66  有两条相同时间规则
    strcpy(macfilter_list_x[0], "11:22:33:44:55:66");
    strcpy(macfilter_date_x[0], "1111111");
    strcpy(macfilter_time_x[0], "08001200");
    
    strcpy(macfilter_list_x[1], "11:22:33:44:55:66");
    strcpy(macfilter_date_x[1], "1111111");
    strcpy(macfilter_time_x[1], "08001200");  // 相同时间规则
    
    // MAC1: 不同时间规则
    strcpy(macfilter_list_x[2], "11:22:33:44:55:66");
    strcpy(macfilter_date_x[2], "1111111");
    strcpy(macfilter_time_x[2], "14001800");
    
    // MAC2: aa:bb:cc:dd:ee:ff
    strcpy(macfilter_list_x[3], "aa:bb:cc:dd:ee:ff");
    strcpy(macfilter_date_x[3], "1111111");
    strcpy(macfilter_time_x[3], "09001700");
    
    // MAC3: 22:33:44:55:66:77  没有时间规则
    strcpy(macfilter_list_x[4], "22:33:44:55:66:77");
    strcpy(macfilter_date_x[4], "");
    strcpy(macfilter_time_x[4], "");
    
    // MAC1: 又一个不同时间规则
    strcpy(macfilter_list_x[5], "11:22:33:44:55:66");
    strcpy(macfilter_date_x[5], "1111111");
    strcpy(macfilter_time_x[5], "20002200");
    
    // MAC2: 又一个规则
    strcpy(macfilter_list_x[6], "aa:bb:cc:dd:ee:ff");
    strcpy(macfilter_date_x[6], "1111111");
    strcpy(macfilter_time_x[6], "19002100");
    
    // 空MAC（应该被跳过）
    strcpy(macfilter_list_x[7], "");
    strcpy(macfilter_date_x[7], "1111111");
    strcpy(macfilter_time_x[7], "10001600");
    
    printf("\n输入数据:\n");
    for (int i = 0; i < macfilter_num_x; i++) {
        printf("规则%d: MAC=%s, 日期=%s, 时间=%s\n", 
               i, macfilter_list_x[i], macfilter_date_x[i], macfilter_time_x[i]);
    }
    printf("\n");
}

// 测试允许模式
void test_allow_mode() {
    printf("\n=== 测试场景2：允许模式 ===\n");
    
    macfilter_num_x = 3;
    
    strcpy(macfilter_list_x[0], "11:22:33:44:55:66");
    strcpy(macfilter_date_x[0], "1111111");
    strcpy(macfilter_time_x[0], "08001200");
    
    strcpy(macfilter_list_x[1], "aa:bb:cc:dd:ee:ff");
    strcpy(macfilter_date_x[1], "1111111");
    strcpy(macfilter_time_x[1], "14001800");
    
    strcpy(macfilter_list_x[2], "22:33:44:55:66:77");
    strcpy(macfilter_date_x[2], "");
    strcpy(macfilter_time_x[2], "");
    
    printf("输入数据:\n");
    for (int i = 0; i < macfilter_num_x; i++) {
        printf("规则%d: MAC=%s, 日期=%s, 时间=%s\n", 
               i, macfilter_list_x[i], macfilter_date_x[i], macfilter_time_x[i]);
    }
    printf("\n");
    
    test_include_mac_filter(1, "DROP");  // 允许模式
}

int main() {
    printf("MAC过滤规则生成逻辑测试程序\n");
    printf("===========================================\n\n");
    
    // 测试拒绝模式
    init_test_data();
    int result = test_include_mac_filter(2, "DROP");  // 拒绝模式
    printf("\n拒绝模式测试结果: %s\n", result > 0 ? "成功" : "失败");
    
    // 测试允许模式
    test_allow_mode();
    
    printf("\n测试完成！\n");
    return 0;
}