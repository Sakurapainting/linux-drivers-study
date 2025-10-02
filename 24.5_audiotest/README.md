# 音频

## 音频 - 基础知识

### 音频文件处理过程：

- .mp3, .wav, 等文件里的音频信息
- APP进行软件解码，提取出原始的音频文件
- 送到音频CODEC解码播放
- 耳机、喇叭播放

### WM8960 音频解码芯片

是 wolfson 公司的音频解码芯片，用作CODEC

- **ADCDAT**：ADC 数据输出引脚，采集到的音频数据转换为数字信号以后通过此引脚传输给主控制器。
- **ADCLRC**：ADC 数据对齐时钟，也就是帧时钟(LRCK)，用于切换左右声道数据，此信号的频率就是采样率。此引脚可以配置为 GPIO 功能，配置为 GPIO 以后 ADC 就会使用 DACLRC引脚作为帧时钟。
- **DACDAT**：DAC 数据输入引脚，主控器通过此引脚将数字信号输入给 WM8960 的 DAC。
- **DACLRC**：DAC 数据对齐时钟，功能和 ADCLRC 一样，都是帧时钟(LRCK)，用于切换左右声道数据，此信号的频率等于采样率。
- **BCLK**：位时钟，用于同步。
- **MCLK**：主时钟，WM8960 工作的时候还需要一路主时钟，此时钟由 I.MX6ULL 提供，MCLK 频率等于采样率的 256 或 384 倍，因此大家在 WM8960 的数据手册里面常看到MCLK=256fs 或 MCLK=384fs。

## 音频 - IIS协议分析

I2S(Inter-IC Sound)总线，用于主控制器和音频CODEC芯片之间传输音频数据

- WS：字段（声道）选择信号，也叫LRCK、帧时钟，用于切换左右声道数据，为 1 表示正在传输左声道的数据，为0表示正在传输右声道的数据。频率等于采样率。
- SCK：串行时钟
- SD：串行数据信号，实际的音频数据。如果需要同时实现放音和录音，那么就要2根数据线。
- 有时候，为了使音频CODEC芯片与主控制器之间能够更好的同步，会引入另外一个叫做MCLK的信号，也叫做主时钟或系统时钟，一般是采样率的256倍或384倍

## 音频 - 音频驱动使能

- 需要一个WM8960驱动文件，IIC驱动框架的，用来配置其功能
- 需要一个SOC端SAI外设的驱动文件
- 需要一个驱动文件，将WM8960与imx6ull联系起来

## 音频 - ALSA音频驱动框架

- 用户空间：alsa-lib
- ASoC是在ALSA基础上，针对SOC另外改进的ALSA音频驱动框架。目前ARM处理的音频驱动框架都是ASoC
  - 分为三部分：SOC（platform）、CODEC部分、板载硬件（Machine）
  - SOC：具体的SOC音频接口驱动，比如6ull的SAI接口，都是半导体厂商编写好的
  - CODEC：具体的音频芯片，比如WM8960，IIC驱动，也不需要我们编写，CODEC芯片厂商会写好
  - 板载硬件：Machine部分，将具体的SOC与具体的CODEC结合，与具体的硬件设备相关，也就是我们要处理的部分，使用ASOC驱动框架将SOC与CODEC结合
- 内核： `Documentation/sound/alsa/soc` 

wm8960 i2c 接口设备树

- 看原理图，wm8960接到了imx6ull 的 i2c1 接口。
- 在i2c1节点下添加wm8960信息，要去看设备树的绑定手册 `Documentation/bindings/sound/wm8960.txt` ，适用于所有的主控，不局限于imx6ull
- 把设备树codec放到`i2c1`下

```c
&i2c1 {
	codec: wm8960@1a {
		compatible = "wlf,wm8960";
		reg = <0x1a>;
		clocks = <&clks IMX6UL_CLK_SAI2>;
		clock-names = "mclk";
		wlf,shared-lrclk;
	};
};
```

正点原子ALPHA 开发板

- CODEC部分驱动文件就是 `wm8960.c`  ，IIC接口的
- SOC部分就是imx6ull的SAI驱动，在imx6ull.dtsi里已经有了sai相关节点，驱动文件就是 `fsl_sai.c` 
- 板载硬件部分，sound节点，驱动文件就是 `imx-wm8960.c` 

注释掉两行没用到的

```c
		// hp-det-gpios = <&gpio5 4 0>;
		// mic-det-gpios = <&gpio5 4 0>;
```

```c
sound {
		compatible = "fsl,imx6ul-evk-wm8960",
			   "fsl,imx-audio-wm8960";
		model = "wm8960-audio";
		cpu-dai = <&sai2>;
		audio-codec = <&codec>;
		asrc-controller = <&asrc>;
		codec-master;
		gpr = <&gpr 4 0x100000 0x100000>;
		/*
                 * hp-det = <hp-det-pin hp-det-polarity>;
		 * hp-det-pin: JD1 JD2  or JD3
		 * hp-det-polarity = 0: hp detect high for headphone
		 * hp-det-polarity = 1: hp detect high for speaker
		 */
		hp-det = <3 0>;
		// hp-det-gpios = <&gpio5 4 0>;
		// mic-det-gpios = <&gpio5 4 0>;
		audio-routing =
			"Headphone Jack", "HP_L",
			"Headphone Jack", "HP_R",
			"Ext Spk", "SPK_LP",
			"Ext Spk", "SPK_LN",
			"Ext Spk", "SPK_RP",
			"Ext Spk", "SPK_RN",
			"LINPUT2", "Mic Jack",
			"LINPUT3", "Mic Jack",
			"RINPUT1", "Main MIC",
			"RINPUT2", "Main MIC",
			"Mic Jack", "MICB",
			"Main MIC", "MICB",
			"CPU-Playback", "ASRC-Playback",
			"Playback", "CPU-Playback",
			"ASRC-Capture", "CPU-Capture",
			"CPU-Capture", "Capture";
	};
```

- 编译设备树后，打开图形化配置界面
- 取消ALSA模拟OSS：
  - -> Device Drivers
    -> Sound card support (SOUND [=y])
    -> Advanced Linux Sound Architecture (SND [=y])
    -> <> OSS Mixer API //不选择
    -> <> OSS PCM (digital audio) API //不选择

- 接下来使能 WM8960 驱动，进入如下路径：
  - -> Device Drivers
    -> Sound card support (SOUND [=y])
    -> Advanced Linux Sound Architecture (SND [=y])
    -> ALSA for SoC audio support (SND_SOC [=y])
    -> SoC Audio for Freescale CPUs
    -> <*> Asynchronous Sample Rate Converter (ASRC) module support //选中
    -> <*> SoC Audio support for i.MX boards with wm8960    //选中

- 修改后make
- 启动开发板，日志中会找到这样一条：

```log
ALSA device list:
  #0: wm8960-audio
```

- 进入系统后：

```bash
/ # ls /dev/snd
controlC0  pcmC0D0c   pcmC0D0p   pcmC0D1c   pcmC0D1p   timer
```

这些文件就是ALSA音频驱动框架对应的设备文件

- controlC0：用于声卡控制，C0表示声卡0
- pcmC0D0c 和 pcmC0D1c：用于录音的 pcm 设备，其中的“COD0”和“C0D1”分别表示声卡 0 中的设备 0 和设备 1，最后面的“c”是 capture 的缩写，表示录音。
- pcmC0D0p 和 pcmC0D1p：用于播放的 pcm 设备，其中的“COD0”和“C0D1”分别表示声卡 0 中的设备 0 和设备 1，最后面的“p”是 playback 的缩写，表示放音。
- timer：定时器。音频驱动使能以后还不能直接播放音乐或录音，我们还需要移植 alsa-lib 和 alsa-utils 这两
个东西。

## 音频 - ALSA移植与使用

### ALSA-LIB

- aplay 这个软件可以播放wav格式的音频
- 当alsa移植好后，不做任何配置直接播放音频，直接播放音频是没有效果的，且有错误。需要配置声卡
- 查看使用amixer方法

```bash
amixer --help
```

```bash
amixer sset Headphone 100,100   
amixer sset Speaker 120,120
amixer sset 'Right Output Mixer PCM' on
amixer sset 'Left Output Mixer PCM' on
```

```bash
aplay test.wav
```

- arecord 录音测试，单MIC录制双声道，要查看CODEC数据手册，看看有没有相关配置操作

### 开机自动配置声卡

声卡设置的保存通过 alsactl 工具来完成，此工具也是 alsa-utils 编译出来的。因为 alsactl 默认将声卡配置文件保存在/var/lib/alsa 目录下，因此首先在开发板根文件系统下创建/var/lib/alsa目录，命令如下：

```bash
mkdir /var/lib/alsa -p
alsactl -f /var/lib/alsa/asound.state store
```

-f 指定声卡配置文件，store 表示保存。关于 alsactl 的详细使用方法，输入“alsactl -h”即可。保存成功以后就会生成/var/lib/alsa/asound.state 这个文件，asound.state 里面就是关于声卡的各种设置信息

如果要使用 asound.state 中的配置信息来配置声卡，执行如下命令即可：

```bash
alsactl -f /var/lib/alsa/asound.state restore
```

最后面的参数改为 restore 即可，也就是恢复的意思。

### mplayer移植和使用

由于 I.MX6ULL 性能比较差，而且没有硬件视频解码，因此 6ULL 不能播放高分辨率、高码率和高帧率的视频，视频分辨率最好在 640*480 左右

-fs 是居中播放视频

```bash
mplayer test.avi -fs
```

如果报错，视频尺寸大于屏幕尺寸，可以使用这个指定播放尺寸

```bash
mplayer test.avi -fs -vf scale=800:480
```
