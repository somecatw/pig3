#ifndef UTILS_H
#define UTILS_H

#include <iterator>
#include <optional>
#include <utility>

template <typename Container>
concept is_forward_iterable = requires(Container c) {
    { c.begin() } -> std::forward_iterator;
    { c.end() } -> std::sentinel_for<decltype(std::begin(c))>;
};

template<typename T>
    requires is_forward_iterable<T>
class enumerate_iterator{
public:
    using iterator_type = decltype(std::declval<T>().begin());

    using value_type = std::pair<size_t, typename iterator_type::value_type>;
    using reference = std::pair<size_t, typename iterator_type::reference>;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    enumerate_iterator(){index=0;}
    enumerate_iterator(const iterator_type &c){
        iterator = c;
        index = 0;
    }
    enumerate_iterator<T>& operator ++(){
        iterator++;
        index++;
        return *this;
    }
    enumerate_iterator<T> operator ++(int){
        enumerate_iterator<T> ret=*this;
        iterator++;
        index++;
        return ret;
    }
    reference operator *()const{
        return {index, *iterator};
    }
    bool operator ==(const enumerate_iterator<T> &other) const{
        return iterator == other.iterator;
    }

private:
    iterator_type iterator;
    size_t index;


};
template<typename T>
    requires is_forward_iterable<T>
class enumerate_container{
public:

    // 仅在传入右值引用时 T 才会为非引用
    static constexpr bool is_enumerating_over_rvalue = !std::is_reference<T>::value;

    using base_container_type = typename std::remove_reference<T>::type;
    using container_type = typename
        std::conditional<is_enumerating_over_rvalue, const T, T>::type;

    // T = vector<int>& &&c -> vector<int>&
    // T = vector<int>
    enumerate_container(T &&c):container_view(c){
        if constexpr (is_enumerating_over_rvalue){
            owned.emplace(std::forward<T>(c));
        }
    }

    container_type &get_container()const{
        if constexpr(!is_enumerating_over_rvalue) return container_view.get();
        else return owned.value();
    }

    enumerate_iterator<container_type> begin()const{
        return enumerate_iterator<container_type>(get_container().begin());
    }
    // end() 的 index 无效
    enumerate_iterator<container_type> end()const{
        return enumerate_iterator<container_type>(get_container().end());
    }

private:
    std::reference_wrapper<base_container_type> container_view;
    std::optional<base_container_type> owned;
};
template<typename T>
    requires is_forward_iterable<T>
auto enumerate(T &&container){
    return enumerate_container<T>(std::forward<T>(container));
}

#endif // UTILS_H
