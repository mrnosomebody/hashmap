#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <utility>
#include <type_traits>
#include <vector>

using namespace std;

enum status {
    DELETED,
    EMPTY,
    FULL
};


template<typename T>
class My_allocator {
public:
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = typename std::add_lvalue_reference<T>::type;
    using const_reference = typename std::add_lvalue_reference<const T>::type;
    using value_type = T;

    My_allocator() noexcept = default;

    My_allocator(const My_allocator &) noexcept = default;

    template<class U>
    explicit My_allocator(const My_allocator<U> &) noexcept {}

    ~My_allocator() = default;

    pointer allocate(size_type n) {
        return static_cast<pointer>(::operator new(sizeof(T) * n));
    }

    void deallocate(pointer p, size_type n) noexcept {
        ::operator delete(p, n * sizeof(value_type));
    }
};

template<typename K, typename T,
        typename Hash = std::hash<K>,
        typename Pred = std::equal_to<K>,
        typename Alloc = My_allocator<std::pair<const K, T>>>
class hash_map;

template<typename ValueType>
class hash_map_iterator {
private:
    ValueType *p;
    static int capacity;
    vector<status> status;
    int hash_index;
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using reference = ValueType &;
    using pointer = ValueType *;

    template<typename K, typename T,
            typename Hash,
            typename Pred,
            typename Alloc>
    friend
    class hash_map;

    hash_map_iterator(pointer p) noexcept: p(p) {}

    hash_map_iterator(const hash_map_iterator &other) noexcept {
        this->p = other.p;
    }

    reference operator*() const {
        return *p;
    }

    pointer operator->() const {
        return p;
    }

    // prefix ++
    hash_map_iterator &operator++() {
        for (int i = 0; i < capacity; ++i) {
            if (status[i] == FULL)
                return p + i;
        }
        return p + capacity;
    }

    // postfix ++
    hash_map_iterator operator++(int) {
        hash_map_iterator temp = *this;
        for (int i = 0; i < capacity; ++i) {
            if (status[i] == FULL)
                this->p = p + i;
        }
        if (this->p == temp.p)
            this->p = p + capacity;
        return temp;
    }

    hash_map_iterator next_free_space() {
        int i = 0;
        while (status[hash_index] == FULL) {
            ++hash_index;
            hash_index %= capacity;
            ++i;
            if (i == capacity - 1)
                return p + capacity;
        }
        return p + hash_index;
    }

    bool operator==(const hash_map_iterator<ValueType> &other) {
        return p == other.p;
    }

    bool operator!=(const hash_map_iterator<ValueType> &other) {
        return p != other.p;
    }
};

template<typename ValueType>
class hash_map_const_iterator {
private:
    ValueType *p;
    static int capacity;
    vector<status> status;
    int hash_index;
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using reference = const ValueType &;
    using pointer = const ValueType *;
    template<typename K, typename T,
            typename Hash,
            typename Pred,
            typename Alloc>
    friend
    class hash_map;

    hash_map_const_iterator() noexcept = default;

    hash_map_const_iterator(const hash_map_const_iterator &other) noexcept = default;

    hash_map_const_iterator(const hash_map_iterator<ValueType> &other) noexcept {};

    const reference operator*() const {
        return *p;
    }

    const pointer operator->() const {
        return p;
    }

    // prefix ++
    hash_map_const_iterator &operator++() {
        for (int i = 0; i < capacity; ++i) {
            if (status[i] == FULL)
                return p + i;
        }
        return p + capacity;
    }

    // postfix ++
    hash_map_const_iterator operator++(int) {
        hash_map_const_iterator temp = *this;
        for (int i = 0; i < capacity; ++i) {
            if (status[i] == FULL)
                this->p = p + i;
        }
        if (this->p == temp.p)
            this->p = p + capacity;
        return temp;
    }

    bool operator==(const hash_map_iterator<ValueType> &other) {
        return p == other.p;
    }

    bool operator!=(const hash_map_iterator<ValueType> &other) {
        return p != other.p;
    }
};


template<typename K, typename T, typename Hash, typename Pred, typename Alloc>
class hash_map {
private:
    using key_type = K;
    using mapped_type = T;
    using hasher = Hash;
    using key_equal = Pred;
    using allocator_type = Alloc;
    using value_type = std::pair<const key_type, mapped_type>;
    using reference = value_type &;
    using const_reference = const value_type &;
    using iterator = hash_map_iterator<value_type>;
    using const_iterator = hash_map_const_iterator<value_type>;
    using size_type = std::size_t;

    float loadfactor = 0, max_loadfactor = 0.5;
    size_type current_size = 0, capacity = 0;
    value_type *arr;
    vector<status> status_ptr;
    Alloc allocator_;
    Hash hasher_;
public:


    /// Default constructor.
    hash_map() = default;

    /**
     *  @brief  Default constructor creates no elements.
     *  @param n  Minimal initial number of buckets.
     */
    explicit hash_map(size_type n) {
        capacity = n;
        if (n != 0) {
            arr = allocator_.allocate(n);
            status_ptr.resize(n);
            for (int i = 0; i < capacity; ++i) {
                status_ptr[i] = EMPTY;
            }
        }
    }

    ~hash_map() {
        for (int i = 0; i < capacity; ++i) {
            if (status_ptr[i] == FULL)
                arr[i].~value_type();
        }
        allocator_.deallocate(arr,capacity);
        //vector<status>().swap(status_ptr);
        status_ptr.clear();
    }

    template<typename InputIterator>
    hash_map(InputIterator first, InputIterator last, size_type n = 0) : hash_map(n) {
        for (auto it = first; it < last; ++it) {
            insert(*it);
        }

    }


    /// Copy constructor.
    hash_map(const hash_map &other) : hash_map(other.capacity) {
        for (auto it = arr; it < capacity; ++it) {
            insert(*it);
        }
    }

    /// Move constructor.
    hash_map(hash_map &&other) {
        std::swap(other.arr, arr);
        std::swap(other.capacity, capacity);
        std::swap(other.loadfactor, loadfactor);
        std::swap(other.max_loadfactor, max_loadfactor);
        std::swap(other.current_size, current_size);
    }

    explicit hash_map(const allocator_type &a) : hash_map(), allocator_(a){}

    hash_map(std::initializer_list<value_type> l, size_type n = 0) : hash_map(n) {
        for (auto it = l.begin(); it != l.end(); ++it) {
            insert(*it);
        }
    }

    /// Copy assignment operator.
    hash_map &operator=(const hash_map &other) {
        for (int i = 0; i < capacity; ++i) {
            other.arr[i] = arr[i];
        }
    }

    /// Move assignment operator.
    hash_map &operator=(hash_map &&other) {
        std::swap(other.arr, arr);
        std::swap(other.capacity, capacity);
        std::swap(other.loadfactor, loadfactor);
        std::swap(other.max_loadfactor, max_loadfactor);
        std::swap(other.current_size, current_size);
    }

    hash_map &operator=(std::initializer_list<value_type> l) {
        for (auto it = l.begin(); it != l.end(); ++it) {
            this->arr.insert(it);
        }
    }

    allocator_type get_allocator() const noexcept {
        return allocator_;
    }

    bool empty() const noexcept {
        for (auto i :arr) {
            if (arr[i] != EMPTY)
                return false;
        }
        return true;
    }

    size_type size() const noexcept {
        return current_size;
    }

    size_type max_size() const noexcept {
        return capacity;
    }

    iterator begin() noexcept {
        iterator it;
        it.capacity = capacity;
        it.status = status_ptr;
        it.p = arr;
        if (status_ptr[0] != FULL) {
            it.operator++();
            return it;
        } else
            return arr;
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator cbegin() const noexcept {
        const_iterator it = arr;
        return it;
    }

    iterator end() noexcept {
        return arr + capacity;
    }

    const_iterator end() const noexcept {
        return cend();
    }

    const_iterator cend() const noexcept {
        const_iterator it = arr + capacity;
        return it;
    }

    std::pair<iterator, bool> insert(K key, T value) {
        if (loadfactor >= max_loadfactor)
            rehash(capacity * 2);
        bool state;
        int hash_index = hasher_(key) % capacity;
        iterator it;

        it.hash_index = hash_index;
        it.p = arr;
        it.capacity = capacity;
        it.status = status_ptr;
        it.next_free_space();

        if (it == arr + capacity) {
            state = false;
            return make_pair(arr + capacity, state);
        } else {
            state = true;
            current_size++;
            loadfactor = static_cast<float>(current_size) / capacity;
            status_ptr[hash_index] = FULL;
            new(arr + hash_index) value_type(key, value);
            arr[hash_index] = make_pair(key,value);
            return make_pair(arr + hash_index, state);

        }
    }

    void erase(K key) {
        int hash_index = hasher_(key) % capacity;
        if (status_ptr[hash_index] == FULL and arr[hash_index].first == key)
            *(arr + hash_index).~value_type();
        else {
            iterator it = find(key);
            if (it == arr + capacity)
                cout << "There is no such element in the map" << endl;
            *it.~value_type();
        }

    }

    iterator find(K key) {
        int hash_index = hasher_(key) % capacity;
        if (status_ptr[hash_index] == FULL and arr[hash_index].first == key)
            return arr + hash_index;
        else {
            int i =0;
            while (status_ptr[hash_index] != FULL and arr[hash_index].first != key and i < capacity){
                ++hash_index;
                hash_index %= capacity;
                ++i;
                if (i == capacity - 1){
                    return arr + capacity;
                }
            }
            return arr + hash_index;
        }
    }


    void rehash(size_type n) {}
};

