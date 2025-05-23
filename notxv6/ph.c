#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
#define NKEYS 100000

pthread_mutex_t lock[NBUCKET];//共享数据锁

struct entry {
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];//每个桶是一个链表
int keys[NKEYS];//存储所有键的数组
int nthread = 1;

double//返回当前时间：秒级精度，用于性能测量
now()
{
 struct timeval tv;
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void //头插法加入新节点，p是指向链表头指针的指针
insert(int key, int value, struct entry **p, struct entry *n)
{
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;
}

static //put函数
void put(int key, int value)
{
  int i = key % NBUCKET;//计算哈希值确定节点位于哪个桶中

  //单独给桶上锁
  pthread_mutex_lock(&lock[i]);

  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {//遍历该桶检查该键是否存在
    if (e->key == key)
      break;
  }
  if(e){//若原本就存在
    // update the existing key.
    e->value = value;
  } else {//不存在就头插法插入新节点
    // the new is new.
    insert(key, value, &table[i], table[i]);
  }
  pthread_mutex_unlock(&lock[i]);
}

static struct entry*
get(int key)
{
  int i = key % NBUCKET;


  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }

  return e;//返回条目指针或null
}

static void *
put_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int b = NKEYS/nthread;//每个线程处理的键数量

  for (int i = 0; i < b; i++) {//插入其负责的所键值对
    put(keys[b*n + i], n);
  }

  return NULL;
}

static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;//未找到的键计数

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;//查找所有键，统计并打印未找到的键数量
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);//获取线程数参数
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);
  assert(NKEYS % nthread == 0);//确保键能够被平分给各线程
  for (int i = 0; i < NKEYS; i++) {
    keys[i] = random();
  }

  //初始化所有桶的锁
  for(int i = 0; i < NBUCKET; i++) {
    pthread_mutex_init(&lock[i], NULL);
  }

  //
  // first the puts
  //
  t0 = now();//创建多个线程执行put操作并测量性能
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();//创建多个线程执行get操作并测量性能
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
