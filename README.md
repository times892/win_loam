# win_loam
一个windows平台运行的a-loam的SLAM程序
## 说明
本项目用于离线运行录制的pcd数据，基于a-loam算法,代码请参考原仓库，仅做了windows平台移植。

## 注意：
当前为debug测试版本，请必须放在windows虚拟机里运行！避免因潜在bug导致损失。

## 使用方法：
下载三个压缩包，解压，并合并放同一个文件夹里
新建一个test_data文件夹，把你的pcd按帧号排序放在这里（建议二进制格式），
在yaml文件里根据你的cpu核心数和线程数来调整里程计和建图的占用核心。

1.终端cd到此exe目录。

2.使用命令
.\win_loam.exe `.\configs\win_loam.yaml `.\test_data
![Uploading win_loam.gif…]()
