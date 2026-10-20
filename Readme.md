#### win_lidar_slam

一个windows平台运行的a-loam的纯激光雷达SLAM程序

本项目的建图模块引用oh-my-loam算法，封装了GUI用于windows电脑运行，第三方依赖库可点击云盘链接下载完整third_party，也可自行查看oh-my-loam的依赖清单下载。云盘链接提供已编译的一键运行版exe可供体验。

建图模块参考https://github.com/feixyz10/oh_my_loam，


第三方依赖库与一键运行版，链接: https://pan.baidu.com/s/12M52I3Il6W84sHOpm5x0dA?pwd=2rvp 提取码: 2rvp 

#### 注意：

本项目是只可运行在windows电脑上的slam程序。编译本项目需包含3个文件夹，分别为：gui,src,third_party.



#### 使用方法

1.把这三个文件夹放在同一个目录下，先编译src\processing，这个是loam的算法模块，然后编译出oh_my_loam_noros.exe，把这个exe存放到gui\runtime\core即可，这个目录下的第三方依赖库文件可直接使用，也可自行去官方仓库下载更换为新版，gui\runtime\core\configs是loam的yaml调参文件。

2.完成第一步后，编译win_Gui.sln,这个是GUI模块，用于可视化查看。

3.如果只是想先体验，直接点击

#### 视频效果

运行效果，请查看此视频网站【windows版的离线SLAM程序】 https://www.bilibili.com/video/BV14RTh6HEgW/?share_source=copy_web&vd_source=7623de01a9adb609eb1dd0a0c8d48bf9
