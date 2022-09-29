#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "net.h"
#include "platform.h"
#include "util.h"

// maximum size of IP datagram
#define LOOPBACK_MTU UINT16_MAX
// キューに保存できるデータの塊?の個数
#define LOOPBACK_QUEUE_LIMIT 16
// ループバックデバイスの割り込み番号
#define LOOPBACK_IRQ (INTR_IRQ_BASE + 1)

#define PRIV(x) ((struct loopback *)x->priv)

typedef struct loopback loopback;
struct loopback {
  int irq;
  mutex_t mutex;
  queue_head queue;
};

typedef struct loopback_queue_entry loopback_queue_entry;
struct loopback_queue_entry {
  uint16_t type;
  size_t len;
  uint8_t data[]; // flexible array member
};

// 送信するデータをキューに保存する
// ループバックしないならどっかに送信するはず
static int loopback_transmit(net_device *dev, uint16_t type,
                             const uint8_t *data, size_t len, const void *dst) {
  // キューへのアクセスはmutexで保護
  mutex_lock(&PRIV(dev)->mutex);
  // キューの上限が超えていたらエラーに
  if (PRIV(dev)->queue.num >= LOOPBACK_QUEUE_LIMIT) {
    mutex_unlock(&PRIV(dev)->mutex);
    errorf("queue is full");
    return -1;
  }
  // キューに格納するエントリ
  loopback_queue_entry *entry =
      memory_alloc(sizeof(loopback_queue_entry) + len);
  if (entry == NULL) {
    mutex_unlock(&PRIV(dev)->mutex);
    errorf("memory_alloc() failure");
    return -1;
  }
  // メタデータの設定とデータ本体のコピー
  entry->type = type;
  entry->len = len;
  memcpy(entry->data, data, len);
  // エントリをキューに突っ込む
  queue_push(&PRIV(dev)->queue, entry);
  unsigned int num = PRIV(dev)->queue.num;
  mutex_unlock(&PRIV(dev)->mutex);
  debugf("queue pushed (num:%u), dev=%s, type=0x%04, len=%zd", num, dev->name,
         type, len);
  debugdump(data, len);
  // 割り込みを発生
  // ここで送り返す的な？よくわからん
  intr_raise_irq(PRIV(dev)->irq);
  return 0;
}

// キューに詰められているデータを書き出す？
static int loopback_isr(unsigned int irq, void *id) {
  net_device *dev = (net_device *)id;
  loopback_queue_entry *entry;

  mutex_lock(&PRIV(dev)->mutex);
  while (1) {
    entry = queue_pop(&PRIV(dev)->queue);
    if (entry == NULL) {
      break;
    }
    debugf("queue popped (num:%u), dev=%s, type=0x%04x, len=%zd",
           PRIV(dev)->queue.num, dev->name, entry->type, entry->len);
    debugdump(entry->data, entry->len);
    // ここで書き込む。。。はず
    net_input_handler(entry->type, entry->data, entry->len, dev);
    memory_free(entry);
  }
  mutex_unlock(&PRIV(dev)->mutex);
  return 0;
}

static net_device_ops loopback_ops = {
    .transmit = loopback_transmit,
};

// ループバックするネットワークデバイスを初期化
net_device *loopback_init(void) {
  net_device *dev = net_device_alloc();
  if (dev == NULL) {
    errorf("net_device_alloc() failure");
    return NULL;
  }
  dev->type = NET_DEVICE_TYPE_LOOPBACK;
  dev->mtu = LOOPBACK_MTU;
  dev->hlen = 0;
  dev->alen = 0;
  dev->flags = NET_DEVICE_FLAG_LOOPBACK;
  dev->ops = &loopback_ops;
  // 外部からはアクセスできない情報
  loopback *lo = memory_alloc(sizeof(loopback));
  if (lo == NULL) {
    errorf("memory_alloc() failure");
    return NULL;
  }
  lo->irq = LOOPBACK_IRQ;
  mutex_init(&lo->mutex);
  queue_init(&lo->queue);
  dev->priv = lo;
  if (net_device_register(dev) == -1) {
    errorf("net_device_register() failure");
    return NULL;
  }
  intr_request_irq(lo->irq, loopback_isr, INTR_IRQ_SHARED, dev->name, dev);
  debugf("initialized, dev=%s", dev->name);
  return dev;
}
