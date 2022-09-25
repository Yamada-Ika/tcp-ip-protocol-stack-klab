#include "driver/dummy.h"
#include "net.h"
#include "test/test.h"
#include "util.h"
#include <signal.h>

static volatile sig_atomic_t terminate;

static void on_signal(int s) {
  (void)s;
  terminate = 1;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, on_signal);
  // プロトコルスタックの初期化
  if (net_init() == -1) {
    errorf("net_init() failure");
    return -1;
  }
  // ダミーデバイスの初期化
  net_device *dev = dummy_init();
  if (dev == NULL) {
    errorf("dummy_init() failure");
    return -1;
  }
  // プロトコルスタックの起動
  if (net_run() == -1) {
    errorf("net_run() failure");
    return -1;
  }
  // Ctrl + C が押されるとon_signal()でterminateに1がセットされる
  while (!terminate) {
    // 1秒おきにデバイスにパケットを書き込む
    // パケットはテストデータを用いる
    if (net_device_output(dev, 0x0000, test_data, sizeof(test_data), NULL) ==
        -1) {
      errorf("net_device_output() failure");
      break;
    }
    sleep(1);
  }
  // プロトコルスタックの停止
  net_shutdown();
  return 0;
}