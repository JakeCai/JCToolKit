#pragma once

#include <type_traits>

namespace JCToolKit
{

    template <typename T>
    class List;

    template <typename T>
    class ListNode
    {
    public:
        friend class List<T>;
        ~ListNode() {}

        template <class... Args>
        ListNode(Args &&...args) : _data(std::forward<Args>(args)...) {}

    private:
        T _data;
        ListNode *next = nullptr;
    };

    template <typename T>
    class List
    {
    public:
        typedef ListNode<T> NodeType;
        List() {}

        List(List &&list)
        {
            swap(list);
        }

        ~List()
        {
            clear();
        }

        void clear()
        {
            auto ptr = _front;
            auto last = ptr;
            while (ptr)
            {
                last = ptr;
                ptr = ptr->next;
                delete last;
            }
            _size = 0;
            _front = nullptr;
            _back = nullptr;
        }

        template <typename FUN>
        void for_each(FUN &&fun)
        {
            auto ptr = _front;
            while (ptr)
            {
                fun(ptr->_data);
                ptr = ptr->next;
            }
        }

        size_t size() const
        {
            return _size;
        }

        bool empty() const
        {
            return _size == 0;
        }
        template <class... Args>
        void emplace_front(Args &&...args)
        {
            NodeType *node = new NodeType(std::forward<Args>(args)...);
            if (!_front)
            {
                _front = node;
                _back = node;
                _size = 1;
            }
            else
            {
                node->next = _front;
                _front = node;
                ++_size;
            }
        }

        template <class... Args>
        void emplace_back(Args &&...args)
        {
            NodeType *node = new NodeType(std::forward<Args>(args)...);
            if (!_back)
            {
                _back = node;
                _front = node;
                _size = 1;
            }
            else
            {
                _back->next = node;
                _back = node;
                ++_size;
            }
        }

        T &front() const
        {
            return _front->_data;
        }

        T &back() const
        {
            return _back->_data;
        }

        T &operator[](size_t pos)
        {
            NodeType *front = _front;
            while (pos--)
            {
                front = front->next;
            }
            return front->_data;
        }

        void pop_front()
        {
            if (!_front)
            {
                return;
            }
            auto ptr = _front;
            _front = _front->next;
            delete ptr;
            if (!_front)
            {
                _back = nullptr;
            }
            --_size;
        }

        void swap(List &list)
        {
            NodeType *tmp_node;

            tmp_node = _front;
            _front = list._front;
            list._front = tmp_node;

            tmp_node = _back;
            _back = list._back;
            list._back = tmp_node;

            size_t tmp_size = _size;
            _size = list._size;
            list._size = tmp_size;
        }

        void append(List<T> &list)
        {
            if (list.empty())
            {
                return;
            }
            if (_back)
            {
                _back->next = list._front;
                _back = list._back;
            }
            else
            {
                _front = list._front;
                _back = list._back;
            }
            _size += list._size;

            list._front = list._back = nullptr;
            list._size = 0;
        }

    private:
        NodeType *_front = nullptr;
        NodeType *_back = nullptr;

        size_t _size = 0;
    };

}
