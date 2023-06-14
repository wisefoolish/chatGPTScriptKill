# chatGPTScriptKill
chatGPT 剧本杀，用线程池实现并发访问chatGPT的api，房间维护一个conversation，从而所有人都能游玩同一个剧本杀。
# 环境配置
1、msvc qt，下载QT5.13 msvc版本，在visual studio上下载Qt插件，然后配置Qt version为Qt5.13 msvc qmake.exe，接着就能在visual studio上运行Qt部分代码。

2、liboai库，https://github.com/D7EAD/liboai.git 这是它的github地址，我略微修改了下它的代码(因为看不太懂，只能自己微调一下来使用)，要用这个库需要nlohmann/json库，libcurl，这两个库的下载可以用vcpkg。

3、ThreadPool，https://github.com/progschj/ThreadPool.git 借用了它的threadpool库，由于这份代码是c++11的，其中std::future<typename std::result_of<F(Args...)>::type>的std::result_of方法在c++17不被支持，而liboai用到了c++17标准的写法，为了统一，修改为std::future<typename std::invoke_result_t<F,Args...>>，std::invoke_result_t是为了推导出函数返回值，future是为了保存一个函数指针，可以用于异步执行函数内容，但是线程池里就没有用到异步调用的功能。

4、设置ChatGPT API的密钥，在Server/源.cpp的795行有一个$CHATGPTKEY$，将它替换为自己的密钥就能在服务器上连接到ChatGPT了。

# 项目介绍
玩家A创建房间，玩家B可以输入房间号加入房间，在玩家给自己命名前不可以使用房间中的询问chatGPT的功能，但是可以在群聊空间里以undefined为名字进行发言，当命名为god的玩家给出剧本，玩家可以各自命名，命名后不可修改，除非重新进入房间，然后玩家将以剧本中的名称进行游戏，可以说全凭自觉，后续设计中可以加入第一个进入房间的人将房间锁定，然后第一个进入房间的人将剧本diy完就可以开放房间，diy完成后询问空间右侧会有人物信息栏，在询问空间只能询问chatGPT，在群聊空间可以跟房间内的其他人一起交流。群聊空间中每当有人完成命名就会多出一个已命名的角色，供玩家交互。当然后续设计的部分是未完成的设想，如何diy剧本还在摸索阶段。
这个项目还提供了单纯聊天的模式，跟网页版的效果差不多，可能会略差一点。
