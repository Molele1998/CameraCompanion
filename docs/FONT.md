# 开放字体

公开版的 `CameraCompanion/ChineseFont.h` 使用 Noto Sans SC 派生的4-bit alpha字形子集，保留 [SIL OFL 1.1](../licenses/NotoSansSC-OFL.txt)。这是修改格式的位图表示，没有包含本机系统字体或原始TTF。代码的MIT许可不替代字体许可；单独复制草图时也须保留草图内的字体许可。

预生成字形已经包含，常规Arduino编译无需Python、Pillow或原始字体。只在增加界面字符/重新生成字形时才需要：

```text
python -m pip install -r requirements.txt
python tools/generate_font.py --font path/to/NotoSansSC.ttf
```

输入为[Google Fonts的Noto Sans SC可变TTF](https://github.com/google/fonts/blob/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf)，SHA-256：

```text
a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da
```

生成器核对输入哈希，以避免无意换成其他授权字体。上游链接随版本可能变化；新字体需核对来源、许可证和生成结果后再更新哈希。标题取600字重，其余400，字形调整到本小屏的统一视觉框；顶部可见内容保持15像素高。没有使用字体保留名称作为修改字形的产品名。

资源生成后应再次运行主机布局检查并检查实际屏幕。替换字体不改变相机连接/控制代码，但公开版字形尚未完成上板观感复验。
