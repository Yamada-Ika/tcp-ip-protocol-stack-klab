#include "net.h"
#include "util.h"
#include "platform.h"

// ダミーデバイスのMTU
#define DUMMY_MTU UINT16_MAX

// ダミーデバイスが使うIRQ番号
#define DUMMY_IRQ INTR_IRQ_BASE

static int dummy_transmit(net_device *dev, uint16_t type, const uint8_t *data,
                          size_t len, const void *dst) {
  debugf("dev=%s, type=0x%04x, len=%zu", dev->name, type, len);
  debugdump(data, len);
  // テスト用に割り込みを発生させる
  intr_raise_irq(DUMMY_IRQ);
  return 0;
}

static int dummy_isr(unsigned int irq, void *id) {
  // 呼び出されたことが分かれば良いのでデバッグ出力
  debugf("irq=%u, dev=%s", irq, ((net_device *)id)->name);
  return 0;
}

static struct net_device_ops dummy_ops = {
    .transmit = dummy_transmit,
};

/*
    ダミーデバイスの仕様
    - 入力なし（データを受信することはない）
    - 出力（データを破棄）
*/
net_device *dummy_init(void) {
  net_device *dev;

  dev = net_device_alloc();
  if (dev == NULL) {
    errorf("net_device_alloc() failure");
    return NULL;
  }
  dev->type = NET_DEVICE_TYPE_DUMMY;
  dev->mtu = DUMMY_MTU;
  dev->hlen = 0; // non header
  dev->alen = 0; // non address
  dev->ops = &dummy_ops;
  if (net_device_register(dev) == -1) {
    errorf("net_device_register() failure");
    return NULL;
  }
  // 割り込みハンドラとしてdummy_isrを登録
  intr_request_irq(DUMMY_IRQ, dummy_isr, INTR_IRQ_SHARED, dev->name, dev);
  debugf("initialized, dev=%s", dev->name);
  return dev;
}
