#ifndef UTILS_H
#define UTILS_H

#include <iterator>
#include <optional>
#include <utility>
#include <vector>
#include <atomic>
#include <latch>
#include <thread>

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

template<std::semiregular TaskType> requires std::invocable<TaskType>
class TaskDispatcher{

    struct WorkerControl {
        std::atomic<int> state{0}; // 0: 等待, 1: 执行, 2: 退出
        std::vector<TaskType> bucket;
        std::atomic<int> head;
        int tail;
    };
public:
    TaskDispatcher(int _threadCount):finishedCount(0), threadCount(_threadCount){
        for(int i=0;i<threadCount;i++){
            controls.push_back(new WorkerControl());
            workers.emplace_back([this, i] {
                auto* ctrl = controls[i];
                while (true) {
                    ctrl->state.wait(0);
                    if (ctrl->state.load() == 2) break;
                    ctrl->head = 0;
                    ctrl->tail = ctrl->bucket.size()-1;

                    while(ctrl->head <= ctrl->tail) {
                        int chead = ctrl->head;
                        int ctail = ctrl->tail;
                        if(chead > ctail)continue;
                        int tmp = chead + 1;
                        if(ctrl->head.compare_exchange_strong(chead, tmp)){
                            // if(ctrl->head > ctail) continue;
                            ctrl->bucket[chead]();
                        }

                    }
                    ctrl->state.store(0);
                    finishedCount.fetch_add(1);
                    while(true){
                        if(this->finishedCount.load() == this->threadCount) break;
                        std::vector<int> ids;
                        for(auto [id, cxk]: enumerate(controls))
                            if(cxk->head <= cxk ->tail) ids.push_back(id);

                        if(!ids.size()) continue;
                        int id = ids[rand()%ids.size()];

                        int chead = controls[id]->head;
                        int ctail = controls[id]->tail;
                        if(chead > ctail)continue;
                        int tmp = chead + 1;
                        if(controls[id]->head.compare_exchange_strong(chead, tmp)){
                            // if(controls[id]->head > ctail) continue;
                            controls[id]->bucket[chead]();
                        }
                    }
                    workDone->count_down();
                }
            });

        }
    }
    ~TaskDispatcher(){
        for (auto* ctrl : controls) {
            ctrl->state.store(2);
            ctrl->state.notify_one();
        }
        for (auto& t : workers) t.join();
        for (auto* ctrl : controls) delete ctrl;
    }

    void runBatch(std::vector<std::vector<TaskType>> &&buckets){
        finishedCount.store(0, std::memory_order_relaxed);
        int taskCount = 0;

        workDone = std::make_unique<std::latch>(threadCount);

        for (int i = 0; i < threadCount; ++i) {
            taskCount += buckets[i].size();
            controls[i]->bucket = std::move(buckets[i]);
            controls[i]->state.store(1);
            controls[i]->state.notify_one();
        }
        workDone->wait();
    }

private:

    int threadCount;
    std::vector<std::thread> workers;
    std::vector<WorkerControl*> controls;
    std::atomic<int> finishedCount;

    std::unique_ptr<std::latch> workDone;

};

#endif // UTILS_H
