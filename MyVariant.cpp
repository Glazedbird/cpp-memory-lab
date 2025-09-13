#include <memory>
#include <type_traits>
#include <utility>

template<typename T1, typename T2>
class MyVariant {
    
    static_assert(std::is_nothrow_destructible_v<T1> && std::is_nothrow_destructible_v<T2>,
                  "类型析构函数应该是nothrow的。");

    private:
        union Data
        {
            T1 t1;
            T2 t2;
            Data() {};
            ~Data() {};
        }m_data;
        

        enum class DataType {
            //表示空状态
            T0,
            T1,
            T2,
        }m_datatype = DataType::T0;  

        //非强异常保证（？？或者说取决于T1，T2的析构函数是不是强异常？）
        void release() noexcept{
            if(m_datatype == DataType::T0) {
                return ;
            } else if(m_datatype == DataType::T1) {
                m_data.t1.~T1();
            } else {
                m_data.t2.~T2();
            }
            m_datatype = DataType::T0;
        }

        //强异常保证
        template<typename T>
        void constructFrom(const T& x) noexcept(std::is_nothrow_copy_constructible_v<T>){
            
            if constexpr(std::is_same_v<T, T1>) {
                ::new(&m_data.t1) T(x);
                m_datatype = DataType::T1;
            } else {
                ::new(&m_data.t2) T(x);
                m_datatype = DataType::T2;
            }
        }


        template<typename T>
        void moveFrom(T& x) noexcept(std::is_nothrow_move_constructible_v<T>){
            if constexpr(std::is_same_v<T, T1>) {
                ::new(&m_data.t1) T(std::move(x));
                m_datatype = DataType::T1;
            } else {
                ::new(&m_data.t2) T(std::move(x));
                m_datatype = DataType::T2;
            }
        }

    public:
        //强异常
        template<typename T>
        requires(!std::is_same_v<T, MyVariant>)
        MyVariant(const T& x) noexcept(std::is_nothrow_copy_constructible_v<T>)
        {
            constructFrom(x);
        }

        ~MyVariant() noexcept {
            release();
        }

        //强异常

        MyVariant(const MyVariant& other) noexcept(std::is_nothrow_copy_constructible_v<T1> && std::is_nothrow_copy_constructible_v<T2>)
        {
            if(other.m_datatype != DataType::T0) {
                if(other.m_datatype == DataType::T1) {
                    constructFrom(other.m_data.t1);
                } else {
                    constructFrom(other.m_data.t2);
                }
            }
        }

        //强异常
        MyVariant& operator=(const MyVariant& other) noexcept(
            std::is_nothrow_copy_constructible_v<T1> &&
            std::is_nothrow_copy_constructible_v<T2> &&
            noexcept(std::declval<MyVariant&>().swap(std::declval<MyVariant&>()))
        )
        {
            if(this != &other){
                MyVariant tmp {other};
                swap(tmp);
            }
            return *this;
        }

        MyVariant(MyVariant&& rhs) noexcept(std::is_nothrow_move_constructible_v<T1> && std::is_nothrow_move_constructible_v<T2>)
        {
            if(rhs.m_datatype != DataType::T0) {
                if(rhs.m_datatype == DataType::T1) {
                    moveFrom(rhs.m_data.t1);
                } else {
                    moveFrom(rhs.m_data.t2);
                }
            }
        }
        
        //强类型保证取决于swap是否nothrow
        MyVariant& operator=(MyVariant&& rhs) noexcept(noexcept(std::declval<MyVariant&>().swap(std::declval<MyVariant&>()))) {
            if(this != &rhs) {
                swap(rhs);
            }
            return *this;
        }
        //强异常保证取决于T1/T2的移动/交换是否nothrow
        void swap(MyVariant& other) noexcept(
            std::is_nothrow_move_constructible_v<T1> &&
            std::is_nothrow_move_constructible_v<T2> &&
            std::is_nothrow_destructible_v<T1> &&
            std::is_nothrow_destructible_v<T2> &&
            std::is_nothrow_swappable_v<T1> &&
            std::is_nothrow_swappable_v<T2>
        ) {
            if (this == &other) return;

            using std::swap;

            // 相同状态：直接交换内部值
            if (m_datatype == other.m_datatype) {
                if (m_datatype == DataType::T1) {
                    swap(m_data.t1, other.m_data.t1);
                } else if (m_datatype == DataType::T2) {
                    swap(m_data.t2, other.m_data.t2);
                }
                return;
            }

            // 处理任一方为空的情况
            if (m_datatype == DataType::T0) {
                if (other.m_datatype == DataType::T1) {
                    moveFrom(other.m_data.t1);
                    other.m_data.t1.~T1();
                } else { // other == T2
                    moveFrom(other.m_data.t2);
                    other.m_data.t2.~T2();
                }
                other.m_datatype = DataType::T0;
                return;
            }

            if (other.m_datatype == DataType::T0) {
                if (m_datatype == DataType::T1) {
                    other.moveFrom(m_data.t1);
                    m_data.t1.~T1();
                } else { // this == T2
                    other.moveFrom(m_data.t2);
                    m_data.t2.~T2();
                }
                m_datatype = DataType::T0;
                return;
            }

            // 类型不同且均非空：T1 <-> T2 交叉交换
            if (m_datatype == DataType::T1 && other.m_datatype == DataType::T2) {
                T1 tmp(std::move(m_data.t1));
                m_data.t1.~T1();

                ::new (&m_data.t2) T2(std::move(other.m_data.t2));
                other.m_data.t2.~T2();

                ::new (&other.m_data.t1) T1(std::move(tmp));

                m_datatype = DataType::T2;
                other.m_datatype = DataType::T1;
                return;
            }

            if (m_datatype == DataType::T2 && other.m_datatype == DataType::T1) {
                T2 tmp(std::move(m_data.t2));
                m_data.t2.~T2();

                ::new (&m_data.t1) T1(std::move(other.m_data.t1));
                other.m_data.t1.~T1();

                ::new (&other.m_data.t2) T2(std::move(tmp));

                m_datatype = DataType::T1;
                other.m_datatype = DataType::T2;
                return;
            }
        }
};

template<typename T1, typename T2>
void swap(MyVariant<T1, T2>& a, MyVariant<T1, T2>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}
