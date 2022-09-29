#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "util.h"

typedef struct irq_entry irq_entry;

struct irq_entry {
  irq_entry *next;
  unsigned int irq; // 割り込み番号
  int (*handler)(
      unsigned int irq,
      void *
          dev); // 割り込みハンドラ（割り込みが発生した時に実行したい関数のポインタ）
  int flags; // フラグ（INTR_IRQ_SHAREDが指定された場合はIRQ番号を共有可能）
  char name[16]; // デバッグ時に使う識別名前
  void *dev; // 割り込み発生元のデバイス（net_device以外にも対応できるように）
};

/*
    NOTE: if you want to add/delete the entries after intr_run(), you need to
   protect these lists with a mutex.
*/
static irq_entry *irqs;

// シグナル集合（シグナルマスク用）
static sigset_t sigmask;

// 割り込み処理スレッドのスレッドID
static pthread_t tid;

// スレッド間の同期のためのバリア
static pthread_barrier_t barrier;

// 割り込みハンドラーを登録
int intr_request_irq(unsigned int irq,
                     int (*handler)(unsigned int irq, void *dev), int flags,
                     const char *name, void *dev) {
  debugf("irq=%u, flags=%d, name=%s", irq, flags, name);
  // 重複しているかチェック
  for (irq_entry *entry = irqs; entry != NULL; entry = entry->next) {
    // IRQ番号が既に登録されているかチェック
    if (entry->irq == irq) {
      // IRQ番号の共有が許可されているかチェック
      if (entry->flags ^ INTR_IRQ_SHARED || flags ^ INTR_IRQ_SHARED) {
        errorf("conflicts with already registered IRQs");
        return -1;
      }
    }
  }

  // IRQリストへ新しいエントリを追加
  irq_entry *entry = memory_alloc(sizeof(irq_entry));
  if (entry == NULL) {
    errorf("memory_alloc() failure");
    return -1;
  }
  entry->irq = irq;
  entry->handler = handler;
  entry->flags = flags;
  strncpy(entry->name, name, sizeof(entry->name) - 1);
  entry->dev = dev;
  entry->next = irqs;
  irqs = entry;
  sigaddset(&sigmask, irq);
  debugf("registered: irq=%u, name=%s", irq, name);
  return 0;
}

int intr_raise_irq(unsigned int irq) {
  // 割り込み処理スレッドへシグナルを送信
  return pthread_kill(tid, (int)irq);
}

// 割り込みスレッドのエントリポイント
// 専用のスレッドの中でシグナルを待ち続ける
static void *intr_thread(void *arg) {
  int terminate = 0, sig, err;

  debugf("start...");
  while (terminate == 0) {
    // メインスレッドと同期をとる
    err = sigwait(&sigmask, &sig);
    if (err != 0) {
      errorf("sigwait() %s", strerror(err));
      break;
    }
    switch (sig) {
    // SIGHUP: 割り込みスレッドへ終了を通知するためのシグナル
    case SIGHUP:
      terminate = 1;
      break;
    // ここにくるシグナルはデバイス割り込み用
    default:
      for (irq_entry *entry = irqs; entry != NULL; entry = entry->next) {
        // IRQ番号が一致するエントリの割り込みハンドラを呼ぶ
        if (entry->irq == (unsigned int)sig) {
          debugf("irq=%d, name=%s", entry->irq, entry->name);
          entry->handler(entry->irq, entry->dev);
        }
      }
      break;
    }
  }
  debugf("terminated");
  return NULL;
}

int intr_run(void) {
  int err;

  err = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
  if (err != 0) {
    errorf("pthread_sigmask() %s", strerror(err));
    return -1;
  }
  // 割り込みスレッドを起動してシグナルを待ち続ける
  err = pthread_create(&tid, NULL, intr_thread, NULL);
  if (err != 0) {
    errorf("pthread_create() %s", strerror(err));
    return -1;
  }
  // 他のスレッドが起動するまでブロック
  // 他のスレッドも同様にpthread_barrier_waitを起動し、barrierのカウントが指定の数になるまでスレッドをブロック
  pthread_barrier_wait(&barrier);
  return 0;
}

void intr_shutdown(void) {
  // 割り込み処理スレッドが起動ずみかチェック
  if (pthread_equal(tid, pthread_self()) != 0) {
    // スレッドがまだ起動していなかったら何もしない
    return;
  }
  // 割り込み処理スレッドにSIGHUPを送信
  pthread_kill(tid, SIGHUP);
  // 割り込み処理スレッドが完全に終了するのを待つ
  pthread_join(tid, NULL);
}

int intr_init(void) {
  // スレッドの初期値にメインスレッドのIDを設定
  tid = pthread_self();
  // バリアの初期化（カウントは2）
  pthread_barrier_init(&barrier, NULL, 2);
  // シグナル集合を空にする
  sigemptyset(&sigmask);
  // シグナル集合にSIGHUPを追加（割り込みスレッド終了通知用）
  sigaddset(&sigmask, SIGHUP);
  return 0;
}