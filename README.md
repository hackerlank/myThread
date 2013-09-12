myThread
========

扩平台的线程库【linux /mac /window】

可以方便的与你的应用联系在一起，直接将源文件拖到你工程里 即可.

简单 轻量的linux线程库 面向对象的封装

class MyThread:public thread::Thread{
public:
    void run()
    {
      // while(true)
      {
        // 处理相关逻辑
      }
    }
};

