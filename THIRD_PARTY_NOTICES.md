# Third-party notices / 第三方声明

本工程原创部分使用根目录 MIT 许可证。以下参考项目和字形的原始声明保留；不使用根许可证替代它们的许可条件。没有随包分发 Arduino 核心、NimBLE 二进制、GFX 库或整套上游仓库。

| 来源 | 固定提交 / 用途 | 许可副本 |
| --- | --- | --- |
| [gkoh/furble](https://github.com/gkoh/furble/tree/0cac22aacc3d9ef250be40849118af78a5f39f27) | Sony 遥控/状态通知及安全能力参考；按键调度和板上界面独立组织 | [MIT](licenses/furble-MIT.txt) |
| [finfinack/alphaloc](https://github.com/finfinack/alphaloc/tree/db75493c3ee1750a8d05457c9fda024199458862) | 原生 NimBLE 客户端、Sony GPS 编码与能力标志参考/改写；对应 CameraRadio.cpp、CameraModel.h 中相关部分 | [Apache-2.0](licenses/alphaloc-Apache-2.0.txt) |
| [dzwiedziu-nkg/esp32-a7iv-rc](https://github.com/dzwiedziu-nkg/esp32-a7iv-rc/tree/f79f8601d9eb8111008cdc413ac62a415a985724) | A7 IV 配对和遥控字节参考；未搬用其 Bluedroid 后端 | [MIT](licenses/esp32-a7iv-rc-MIT.txt) |
| [ekutner/camera-gps-link](https://github.com/ekutner/camera-gps-link/tree/3d4aaf551385e94b235e99f80edf13a0106dfd6d) | Android 模式1时序、Sony广播/GPS初始化及重连思路参考；本工程改写为 ESP32 host 任务和单相机状态机 | [MIT](licenses/camera-gps-link-MIT.txt) |
| [Google Fonts: Noto Sans SC](https://github.com/google/fonts/tree/main/ofl/notosanssc) | 修改格式的 4-bit alpha 字形子集，来自 OFL 字体，非 Windows 系统字体 | [SIL OFL 1.1](licenses/NotoSansSC-OFL.txt) |

alphaloc 的发布提交没有独立 NOTICE 文件；保留其完整 Apache-2.0 许可证、项目链接、提交及相关改写范围。本项目新增 Arduino 启动/板层、串行事务、触摸确认、停止/防重复和间隔计划组织，并非原版项目。

板级驱动参照 [Waveshare 本板官方资料](https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47) 的引脚及 JD9853 初始化。此包没有复制官方 PDF、原始演示 ZIP 或其他硬件型号文件。

图标由 tools/generate_icons.py 中的几何绘制规则生成，没有使用厂商品牌徽标。第三方作者的公开署名属于必须保留的来源声明，不是本地使用者的个人资料。
