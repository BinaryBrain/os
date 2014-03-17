#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/linkage.h>

asmlinkage long sys_get_unique_id(int *uuid);
atomic_t id = ATOMIC_INIT(0);

asmlinkage long sys_get_unique_id(int *uuid) {
	return put_user(atomic_add_return(1,&id),uuid);
}
