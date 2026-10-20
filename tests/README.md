# 主机检查

需要Python 3.10或更新版本、可用的C++17编译器（默认g++）和requirements.txt中的Pillow。无需实际开发板、相机、TTF或Arduino核心来运行这些模拟检查。

```text
python -m pip install -r requirements.txt
python tests/run_tests.py
```

可通过`--compiler`指定编译器，通过`--output`指定输出目录；默认将程序和模拟图片放入被gitignore排除的`.build/host`。Windows可用MinGW，其他系统用支持这些参数的g++/clang++。

六组检查：模型（广播/轨迹/UTC/定位包/拍摄及按键调度）；实际CameraRadio.cpp的NimBLE模拟（MTU、安全、确认/取消、超时、SMP原因、停止/重连，以及录像通知/ACK区别、状态变化、长按续期/释放）；实际CameraUi.cpp的触摸模拟（功能分页、桌面整页、长按、防误触、GPX入口）；实际CameraScene.h的328个虚构界面、录像计时和状态栏24组合96个像素边界；实际BufferedDisplay.cpp的条带和动画逐像素检查；实际GpxLogger.cpp及主机临时文件、独立XML解析。

GPX套件只在输出目录中创建唯一的模拟目录，不访问真实SD卡；SD挂载、故障、任务和互斥API使用替身。检查GPX命名空间、元数据、经纬度、时间和分段，未进行完整GPX XSD校验。动画检查不测实际帧率。主机绘图替身不包含Arduino_GFX默认英文字体，固件文字使用项目的Cjk绘制器。

只替代外部API，不另写一套被测业务。测试用相机名称、地址片段、配对码和坐标均为合成数据；没有访问真实设备。测试通过不能当作真实配对、拍摄、EXIF、触摸或长期稳定性验收。

模拟总览会保存为`.build/host/overview.png`，另有`remote-controls.png`、`gpx-backup.png`、`status-bar.png`和`pairing-preview.png`。资源脚本`tools/generate_icons.py`可重生成原创图标，字形脚本见[字体说明](../docs/FONT.md)。
