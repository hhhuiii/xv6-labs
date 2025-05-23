#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static int nthread = 1;
static int round = 0;//全局轮次计数器

struct barrier {
  pthread_mutex_t barrier_mutex;//互斥锁，保护屏障状态
  pthread_cond_t barrier_cond;//条件变量，用于线程等待
  int nthread;      // Number of threads that have reached this round of the barrier
  int round;     // Barrier round
} bstate;

static void
barrier_init(void)//初始化互斥锁和条件变量，将到达线程计数初始化为0
{
  assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
  assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
  bstate.nthread = 0;
}

static void 
barrier()//获取互斥锁并检查所有线程是否都已到达
{
  pthread_mutex_lock(&bstate.barrier_mutex);
  if(++bstate.nthread < nthread)
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  else {
    bstate.nthread = 0;
    bstate.round++;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

static void *
thread(void *xa)
{
  long n = (long) xa;
  long delay;
  int i;

  for (i = 0; i < 20000; i++) {//每次循环检查当前轮次是否与循环轮次计数一致（确保同步正确）
    int t = bstate.round;
    assert (i == t);
    barrier();//调用barrier等待其他线程
    usleep(random() % 100);//执行随机延迟
  }

  return 0;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "%s: %s nthread\n", argv[0], argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);//获取线程数
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);

  barrier_init();

  for(i = 0; i < nthread; i++) {//创建线程
    assert(pthread_create(&tha[i], NULL, thread, (void *) i) == 0);
  }
  for(i = 0; i < nthread; i++) {//等待线程结束
    assert(pthread_join(tha[i], &value) == 0);
  }
  printf("OK; passed\n");
}
