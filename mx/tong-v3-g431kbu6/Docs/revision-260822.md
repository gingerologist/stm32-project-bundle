# Revision 260822

第一次修改，是小改版，主要修改目的：

1. 替换一颗处理器，增加flash容量，之前的型号flash竟然不够用了。
2. 增加一颗高速开关和两组bleeding resistor用于放电。
3. 需要MCU提供两个GPIO，一个选择放电电阻大小，一个用于使能放电开关。
4. 减少原有的GPIO使用。
5. 其它。



处理器从STM32G431K6U6（32KB Flash）更换为STM32G431KBu6（128KB flash），pin-to-pin，除了Flash容量其它都一样。64KB版本的K8U6在立创上没有QFN32封装。



高速开关复制MUX7234（2:1 SPDT x4），其实可以使用MUX7212，是1:1 SPDT x4，但封装上都是5x5mm，不减少面积。MUX7234因为是2:1，提供了两种bleeding电阻大小的可能性，实际选择100R和22R，每路。注意模拟开关本身也有平均3.6欧姆，最大5.5欧姆/25摄氏度或7.1欧姆/-40至85摄氏度的电阻。



需要提供的两个GPIO是BLEED_SEL和BLEED_EN；同时增加两个测试点，不需要串行电阻。



之前的设计里可以减少的GPIO包括：

1. （opamp使能） CH0_SD和CH1_SD可以合并，不过如果合并将无法独立控制每一路；
2. MUX_SEL1和MUX_SEL2可以合并，MUX_SEL3和MUX_SEL4可以合并；如果确定是两路同步，则四路MUX_SEL可以合并在一起。
3. 高压使能EN_POS_HV和EN_NEG_HV可以合并。



实际修改仅做保守修改，合并MUX_SEL1/2，保留2，合并MUX_SEL3/4，保留3。



其它新增4pin连接器，更换了一个33欧姆电阻到22欧姆（R20，OSC_IN）。









