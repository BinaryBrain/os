#include <stdio.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#if defined(__LP64__) || defined(_LP64) || __x86_64__
#define IS_32_BIT 0
#else
#define IS_32_BIT 1
#endif

#if IS_32_BIT
#define __NR_get_unique_id 351
#else
#define __NR_get_unique_id 543
#endif
#define MAX_THREAD 10
#define THREAD_LOOP 10

long get_unique_id(int* uuid) {
return syscall(__NR_get_unique_id , uuid) ? errno : 0;
}
void *print_unique_id(void *thread_id){
	int j;
	int res;
	for(j=0; j<THREAD_LOOP; j++){
		int* uuid=malloc(sizeof(int));
		res = get_unique_id(uuid);
		printf("Syscall returned %d, uuid is %d, thread_id %ld\n", res, *uuid, (long)thread_id);
		usleep(j%6);
		}
	pthread_exit(NULL);
}
int main() {
long i;

pthread_t tab_thread[MAX_THREAD];
for(i=0; i<MAX_THREAD; ++i){
	pthread_create(&tab_thread[i], NULL, print_unique_id, (void *) i);
}
pthread_exit(NULL);
return 0;
}
