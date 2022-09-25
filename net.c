#include "net.h"
#include "platform.h"
#include "util.h"

// ネットワークデバイスを開く
static int net_device_open(net_device *dev) {
  if (NET_DEVICE_IS_UP(dev)) {
    errorf("already opened, dev=%s", dev->name);
    return -1;
  }
  // openが実装されていない場合もある
  if (dev->ops->open != NULL) {
    if (dev->ops->open(dev) == -1) {
      errorf("failure, dev=%s", dev->name);
      return -1;
    }
  }
  dev->flags |= NET_DEVICE_FLAG_UP;
  infof("dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
  return 0;
}

// ネットワークデバイスを閉じる
static int net_device_close(net_device *dev) {
  if (!NET_DEVICE_IS_UP(dev)) {
    errorf("not opened, dev=%s", dev->name);
    return -1;
  }
  // closeが実装されていない場合もある
  if (dev->ops->close != NULL) {
    if (dev->ops->close(dev) == -1) {
      errorf("failure, dev=%s", dev->name);
      return -1;
    }
  }
  dev->flags &= ~NET_DEVICE_FLAG_UP;
  infof("dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
  return 0;
}

/*
  NOTE: if you want to add/delete the entries after net_run(), you need to
  protect these lists with a mutex.
*/

static net_device *devices;

static void net_device_foreach(net_device *dev, int (*func)(net_device *dev)) {
  for (net_device *device = dev; device != NULL; device = device->next) {
    func(device);
  }
}

int net_run(void) {
  debugf("open all devices...");
  net_device_foreach(devices, net_device_open);
  debugf("running...");
  return 0;
}

int net_shutdown(void) {
  debugf("close all devices...");
  net_device_foreach(devices, net_device_close);
  debugf("shutting down");
  return 0;
}

int net_init(void) {
  infof("initialized");
  return 0;
}

net_device *net_device_alloc(void) {
  net_device *dev;
  dev = memory_alloc(sizeof(net_device));
  if (dev == NULL) {
    errorf("memory_alloc() failure");
    return NULL;
  }
  return dev;
}

/*
  NOTE: must not be call after net_run()
*/

int net_device_register(net_device *dev) {
  static unsigned int index = 0;

  dev->index = index++;
  snprintf(dev->name, sizeof(dev->name), "net%d", dev->index);
  dev->next = devices;
  devices = dev;
  infof("registerd, dev=%s, type=0x%04x", dev->name, dev->type);
  return 0;
}

// ネットワークデバイスへの書き込み
int net_device_output(net_device *dev, uint16_t type, const uint8_t *data,
                      size_t len, const void *dst) {
  if (!NET_DEVICE_IS_UP(dev)) {
    errorf("not opened, dev=%s", dev->name);
    return -1;
  }
  // MTU (Maximum Transmission Unit)
  // データリンク層で一度に送信できるデータサイズの最大値
  if (len > dev->mtu) {
    errorf("too long, dev=%s, mtu=%u, len=%zu", dev->name, dev->mtu, len);
    return -1;
  }
  debugf("dev=%s, type=0x%04x, len=%zu", dev->name, type, len);
  debugdump(data, len);
  if (dev->ops->transmit == NULL) {
    errorf("transmit must implement, dev=%s", dev->name);
    return -1;
  }
  if (dev->ops->transmit(dev, type, data, len, dst) == -1) {
    errorf("device transmit failure, dev=%s, len=%zu", dev->name, len);
    return -1;
  }
  return 0;
}

// デバイスが受信したパケットをプロトコルスタックに渡す関数
int net_input_handler(uint16_t type, const uint8_t *data, size_t len,
                      net_device *dev) {
  debugf("dev=%s, type=0x%04x, len=%zu", dev->name, type, len);
  debugdump(data, len);
  return 0;
}
