# Migration

CubeIDE从2.x版本开始，分离了CubeIDE和CubeMX。相应的，项目代码树和工程文件也分离了。迁移步骤如下：



第一步：复制代码树目录，但不要在IDE中打开；

第二步：修改新的目录名，和ioc文件名，应反映新的硬件版本和处理器名称；

第三步：在CubeMX里打开ioc文件，在pinout中找到“list pinout compatible MCUs"，找到要替换的MCU名称，选择新的MCU型号（STM32G431KBU6），点击ok,import按钮；

第四步：替换之后绝大多数原有配置都会保留，但会弹出一个对话框询问是不是选择timer而不是systick，只是目前发现唯一在更换MCU时会丢失、需要重新设置的选项，它的位置在System Core->SYS->Timebase Source选项上，选择TIM1，和之前的项目一致。

第五步：在Project Manager中核对Project Name，Location设置是否符合要求；确定Toolchain / IDE设置选择为STM32CubeIDE并勾选Generate Under Root。核对Mcu Reference的设置是新的MCU型号。

第六步：四五步检查无误后，可以点击Generate Code生成代码。



到这里会遇到一个bug，linker文件未正确生成。没找到好的解决方案，只好选择了新的MCU型号单独创建了一个新项目，把ld文件copy过来。



然后是项目文件迁移。需要先手动修改

- `.cproject`
- `.project`

这两个文件中和name或path有关的名字，比如之前包含g431k6u6的字符串都修改成包含g431kbu6，其中有一个是linker文件的路径，这个可以在IDE中修改。

修改完成后，打开CubeIDE，找到新的mx项目目录，导入后如果直接build会看到linker文件有错误，需要修改。位置在Project->Properties->C/C++ Build->Settings->Tool Settings->MCU/MPU GCC Linker->General，在Linker Script输入框中更正Linker文件名称，然后build即可通过。



# 功能迁移



因为项目使用Flash存储，在最后一个bank，所以应该修改Linker文件，原来的Linker文件中，MEMORY.FLASH设置为LENGTH=30K，用最后一个2K page保存了配置；新的Linker文件中的缺省128K改为126K。

Flash的具体定义在rm0440文档中，category的定义在1.5，G431都是Category 2；然后在第5.3节可以看到，G431系列都是2k page的，KBU6是有64个page的，最后的page 63的起始和结束地址是

```
0x0801 F800 - 0x0801 FFFF
```



代码修改首先从这里开始。





