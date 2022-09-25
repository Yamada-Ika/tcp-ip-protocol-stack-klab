#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <stdint.h>

extern int net_init(void);
extern int net_run(void);
extern int net_shutdown(void);

#define NET_DEVICE_ADDR_LEN 16
#define NET_DEVICE_FLAG_UP 0x0001
#define NET_DEVICE_TYPE_DUMMY 0x0000

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#define NET_DEVICE_IS_UP(x) ((x)->flags & NET_DEVICE_FLAG_UP)
#define NET_DEVICE_STATE(x) (NET_DEVICE_IS_UP(x) ? "up" : "down")

typedef struct net_device net_device;
typedef struct net_device_ops net_device_ops;

// デバイス構造体
struct net_device {
  net_device *next;
  unsigned int index;
  char name[IFNAMSIZ];
  uint16_t type; // デバイスの種類
  // デバイスの種類によって変化する値
  uint16_t mtu;
  uint16_t flags;
  uint16_t hlen; // header length
  uint16_t alen; // address length
  // デバイスのハードウェアアドレスなど
  uint8_t addr[NET_DEVICE_ADDR_LEN];
  union {
    uint8_t peer[NET_DEVICE_ADDR_LEN];
    uint8_t broadcast[NET_DEVICE_ADDR_LEN];
  };

  net_device_ops *ops;
  void *priv; // デバイスドライバが使うプライベートなデータへのポインタ
};

// デバイスドライバに実装されている関数へのポインタを格納
// transmit(送信関数)は実装必須
struct net_device_ops {
  int (*open)(net_device *dev);
  int (*close)(net_device *dev);
  int (*transmit)(net_device *dev, uint16_t type, const uint8_t *data,
                  size_t len, const void *dst);
};

net_device *net_device_alloc(void);
int net_device_register(net_device *dev);
int net_device_output(net_device *dev, uint16_t type, const uint8_t *data,
                      size_t len, const void *dst);

#endif
