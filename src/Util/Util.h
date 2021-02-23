#pragma once

#include <functional>
#include <type_traits>

namespace JCToolKit {
class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    noncopyable(const noncopyable &that) = delete;
    noncopyable(noncopyable &&that) = delete;
    noncopyable &operator=(const noncopyable &that) = delete;
    noncopyable &operator=(noncopyable &&that) = delete;
};

class onceToken {
public:
    typedef std::function<void(void)> task;

    template<typename FUNC>
    onceToken(const FUNC &onConstructed, std::function<void(void)> onDestructed = nullptr) {
        onConstructed();
        _onDestructed = std::move(onDestructed);
    }

    onceToken(std::nullptr_t, std::function<void(void)> onDestructed = nullptr) {
        _onDestructed = std::move(onDestructed);
    }

    ~onceToken() {
        if (_onDestructed) {
            _onDestructed();
        }
    }

private:
    onceToken() = delete;
    onceToken(const onceToken &) = delete;
    onceToken(onceToken &&) = delete;
    onceToken &operator=(const onceToken &) = delete;
    onceToken &operator=(onceToken &&) = delete;

private:
    task _onDestructed;
};

}
