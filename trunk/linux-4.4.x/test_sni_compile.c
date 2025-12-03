// 简化的SNI模块编译测试
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_sni.h>
#include <linux/textsearch.h>

MODULE_AUTHOR("tekintian <tekintian@gmail.com>");
MODULE_DESCRIPTION("Xtables: SNI-based matching (refactored for stability)");
MODULE_LICENSE("GPL");

static bool test_function(void) {
    return true;
}

static int __init test_init(void) {
    printk(KERN_INFO "SNI test module loaded\n");
    return 0;
}

static void __exit test_exit(void) {
    printk(KERN_INFO "SNI test module unloaded\n");
}

module_init(test_init);
module_exit(test_exit);
