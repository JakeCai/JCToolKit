#include <signal.h>
#include <iostream>
#include <random>
#include "Util/ReusePool.h"
#include "Thread/ThreadGroup.h"
#include <list>
#include <sstream>

using namespace JCToolKit;

class StrPrinter : public std::string {
public:
    StrPrinter() {}

    template<typename T>
    StrPrinter& operator <<(T && data) {
        _stream << std::forward<T>(data);
        this->std::string::operator=(_stream.str());
        return *this;
    }

    std::string operator <<(std::ostream&(*f)(std::ostream&)) const {
        return *this;
    }

private:
    std::stringstream _stream;
};

//程序退出标志
bool g_bExitFlag = false;

class string_imp : public std::string{
public:
    template<typename ...ArgTypes>
    string_imp(ArgTypes &&...args) : std::string(std::forward<ArgTypes>(args)...){
        std::cout << "创建string对象:" << this << " " << *this << std::endl;
    };
    ~string_imp(){
        std::cout << "销毁string对象:" << this << " " << *this << std::endl;
    }
};

void onRun(ReusePool<string_imp> &pool,int threadNum){
    std::random_device rd;
    while(!g_bExitFlag){
        //获取一个可用的对象
        auto obj = pool.obtain();
        if(obj->empty()){
            //这个对象是全新未使用的
            std::cout << "后台线程 " << threadNum << ":" << "obtain a emptry object!" << std::endl;
        }else{
            //这个对象是循环使用的
            std::cout << "后台线程 " << threadNum << ":" << *obj << std::endl;
        }
        //标记该对象被本线程使用
        obj->assign(StrPrinter() << "被线程持有:" << threadNum );

        //随机休眠，打乱循环使用顺序
        std::this_thread::sleep_for(std::chrono::microseconds( 1000 * (rd()% 10)));
        obj.reset();//手动释放
        std::this_thread::sleep_for(std::chrono::microseconds( 1000 * (rd()% 1000)));
    }
}

int main() {
    //大小为50的循环池
    ReusePool<string_imp> pool;
    pool.setSize(50);

    //获取一个对象,该对象将被主线程持有，并且不会被后台线程获取并赋值
    auto reservedObj = pool.obtain();
    //在主线程赋值该对象
    reservedObj->assign("This is a reserved object , and will never be used!");

    ThreadGroup group;
    //创建4个后台线程，该4个线程模拟循环池的使用场景，
    //理论上4个线程在同一时间最多同时总共占用4个对象

    std::cout << "主线程:" << "测试主线程已经获取到的对象应该不会被后台线程获取到:" << *reservedObj << std::endl;

    for(int i = 0 ;i < 4 ; ++i){
        group.createThread([i,&pool](){
            onRun(pool,i);
        });
    }

    //等待3秒钟，此时循环池里面可用的对象基本上最少都被使用过一遍了
    std::this_thread::sleep_for(std::chrono::seconds(3));

    //但是由于reservedObj早已被主线程持有，后台线程是获取不到该对象的
    //所以其值应该尚未被覆盖
    std::cout << "主线程: 该对象还在被主线程持有，其值应该保持不变:" << *reservedObj << std::endl;

    //获取该对象的引用
    auto &objref = *reservedObj;

    //显式释放对象,让对象重新进入循环列队，这时该对象应该会被后台线程持有并赋值
    reservedObj.reset();

    std::cout << "主线程打印: 已经释放该对象,它应该会被后台线程获取到并被覆盖值" << std::endl;

    //再休眠3秒，让reservedObj被后台线程循环使用
    std::this_thread::sleep_for(std::chrono::seconds(3));

    //这时，reservedObj还在循环池内，引用应该还是有效的，但是值应该被覆盖了
    std::cout << "主线程:对象已被后台线程赋值为:" << objref << std::endl;

    {
        std::cout << "主线程: 测试主动放弃循环使用功能" << std::endl;

        List<decltype(pool)::ValuePtr> objlist;
        for (int i = 0; i < 8; ++i) {
            reservedObj = pool.obtain();
            std::string str = StrPrinter() << i << " " << (i % 2 == 0 ? "对象将脱离重用池管理" : "对象将回到重用池");
            reservedObj->assign(str);
            reservedObj.quit(i % 2 == 0);
            objlist.emplace_back(reservedObj);
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));

    g_bExitFlag = true;

    group.joinAll();
    return 0;
}