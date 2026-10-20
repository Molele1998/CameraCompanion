# 主机检查

需要Python 3.10或更新版本、可用的C++17编译器（默认g++）和requirements.txt中的Pillow。无需实际开发板、相机、TTF或Arduino核心来运行这些模拟检查。

```text
python -m pip install -r requirements.txt
python tests/run_tests.py
```

可通过`--compiler`指定编译器，通过`--output`指定输出目录；默认将程序和模拟图片放入被gitignore排除的`.build/host`。Windows可用MinGW，其他系统用支持这些参数的g++/clang++。

四组检查：模型（广播/轨迹/UTC/定位包/拍摄调度）；实际CameraRadio.cpp的NimBLE模拟（MTU、安全、确认/取消、超时、SMP原因、停止/重连）；实际CameraUi.cpp的触摸模拟；实际CameraScene.h的252个虚构界面与状态栏像素边界。

只替代外部API，不另写一套被测业务。测试用相机名称、地址片段、配对码和坐标均为合成数据；没有访问真实设备。测试通过不能当作真实配对、拍摄、EXIF、触摸或长期稳定性验收。

模拟总览会保存为`.build/host/overview.png`。资源脚本`tools/generate_icons.py`可重生成原创图标，字形脚本见[字体说明](../docs/FONT.md)。
