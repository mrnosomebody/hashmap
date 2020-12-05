#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <utility>
#include <type_traits>
#include <vector>
#include <limits>

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
    int capacity = 0;
    vector<status> status_;
    int hash_index = 0;
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

    hash_map_iterator() = default;

   hash_map_iterator(pointer p, int capacity,vector<status> status_, int hash_index):
    p(p), capacity(capacity),
    status_(status_),
    hash_index(hash_index) {}

    hash_map_iterator(const hash_map_iterator &other) noexcept {
        this->p = other.p;
        this->capacity = other.capacity;
        this->hash_index = other.hash_index;
        this->status_ = other.status_;
    }

    reference operator*() const {
        return *(p + hash_index);
    }

    pointer operator->() const {
        return p + hash_index;
    }

    // prefix ++
    hash_map_iterator &operator++() {
        for (int i = 0; i < capacity; ++i) {
            if (status_[i] == FULL) {
                hash_index = i;
                return *this;
            }
        }
        hash_index = capacity;
        return *this;
    }

    // postfix ++
    hash_map_iterator operator++(int) {
        auto tmp = hash_map_iterator(*this);
        operator++();
        return tmp;
    }

    hash_map_iterator next_free_space() {
        int i = 0;
        while (status_[hash_index] == FULL) {
            ++hash_index;
            hash_index %= capacity;
            ++i;
            if (i == capacity - 1) {
                hash_index = capacity;
                return *this;
            }
        }
        hash_index = i;
        return *this;
    }

    friend bool operator==(const hash_map_iterator<ValueType> &lhs, const hash_map_iterator<ValueType> &rhs) {
        return lhs.hash_index == rhs.hash_index;
    }

    friend bool operator!=(const hash_map_iterator<ValueType> &lhs, const hash_map_iterator<ValueType> &rhs) {
        return lhs.hash_index != rhs.hash_index;
    }
};

template<typename ValueType>
class hash_map_const_iterator {
private:
    ValueType *p;
    int capacity = 0;
    vector<status> status;
    int hash_index = 0;
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

    hash_map_const_iterator(const hash_map_const_iterator &other) noexcept {
        this->p = other.p;
        this->capacity = other.capacity;
        this->hash_index = other.hash_index;
        this->status = other.status;
    }

    hash_map_const_iterator(const hash_map_iterator<ValueType> &other) noexcept {
        this->p = other.p;
        this->capacity = other.capacity;
        this->hash_index = other.hash_index;
        this->status = other.status;
    };

    const reference operator*() const {
        return *(p + hash_index);
    }

    const pointer operator->() const {
        return (p + hash_index);
    }

    // prefix ++
    hash_map_const_iterator &operator++() {
        for (int i = 0; i < capacity; ++i) {
            if (status[i] == FULL) {
                hash_index = i;
                return *this;
            }
        }
        hash_index = capacity;
        return *this;
    }

    // postfix ++
    hash_map_const_iterator operator++(int) {
        auto tmp = hash_map_const_iterator(*this);
        operator++();
        return tmp;
    }

    friend bool operator==(const hash_map_iterator<ValueType> &lhs, const hash_map_iterator<ValueType> &rhs) {
        return lhs == rhs;
    }

    friend bool operator!=(const hash_map_iterator<ValueType> &lhs, const hash_map_iterator<ValueType> &rhs) {
        return lhs != rhs;
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
    allocator_type allocator_ ;
    hasher hasher_;
    key_equal equal_;
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
        allocator_.deallocate(arr, capacity);
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
    hash_map(hash_map &&other) noexcept {
        swap(other);
    }

    explicit hash_map(const allocator_type &a) {
        allocator_ = a;
    }

    hash_map(std::initializer_list<value_type> l, size_type n = 0) : hash_map(n) {
        for (auto it = l.begin(); it != l.end(); ++it) {
            insert(*it);
        }
    }

    /// Copy assignment operator.
    hash_map &operator=(const hash_map &other) {
        hash_map(other).swap(*this);
        return *this;
    }

    /// Move assignment operator.
    hash_map &operator=(hash_map &&other) noexcept {
        swap(other);
        return *this;
    }

    hash_map &operator=(std::initializer_list<value_type> l) {
        for (auto it = l.begin(); it != l.end(); ++it) {
            insert(*it);
        }
    }

    allocator_type get_allocator() const noexcept {
        return allocator_;
    }

    bool empty() const noexcept {
        return current_size == 0;
    }

    size_type size() const noexcept {
        return current_size;
    }

    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max();
    }

    void swap(hash_map &x) {
        std::swap(arr, x.arr);
        std::swap(allocator_, x.allocator_);
        std::swap(capacity, x.capacity);
        std::swap(loadfactor, x.loadfactor);
        std::swap(max_loadfactor, x.max_loadfactor);
        std::swap(hasher_, x.hasher_);
        std::swap(status_ptr, x.status_ptr);
        std::swap(current_size, x.current_size);
        std::swap(equal_, x.equal_);
    }

    iterator begin() noexcept {
        iterator it;
        it.capacity = capacity;
        it.status = status_ptr;
        it.p = arr;
        if (status_ptr[0] != FULL) {
            it.operator++();
            return it;
        } else {
            it.hash_index = 0;
            return it;
        }
    }

    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator cbegin() const noexcept {
        const_iterator it;
        it.capacity = capacity;
        it.status = status_ptr;
        it.p = arr;
        if (status_ptr[0] != FULL) {
            it.operator++();
            return it;
        } else {
            it.hash_index = 0;
            return it;
        }
    }

    iterator end() noexcept {
        iterator it;
        it.capacity = capacity;
        it.status_ = status_ptr;
        it.p = arr;
        it.hash_index = capacity;
        return it;
    }

    const_iterator end() const noexcept {
        return cend();
    }

    const_iterator cend() const noexcept {
        const_iterator it;
        it.capacity = capacity;
        it.status = status_ptr;
        it.p = arr;
        it.hash_index = capacity;
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
        it.status_ = status_ptr;
        it = it.next_free_space();

        if (it == end()) {
            it = end();
            state = false;
            return pair<iterator, bool>(it, state);
        } else {
            state = true;
            current_size++;
            loadfactor = static_cast<float>(current_size) / capacity;
            status_ptr[hash_index] = FULL;
            new(arr + hash_index) value_type(key, value);
            return pair<iterator, bool>(hash_map_iterator<value_type>(arr,capacity,status_ptr,hash_index),state);
        }
    }

    void erase(K key) {
        int hash_index = hasher_(key) % capacity;
        if (status_ptr[hash_index] == FULL and arr[hash_index].first == key) {
            (arr + hash_index)->~value_type();
            current_size--;
            loadfactor = static_cast<float>(current_size) / capacity;
            status_ptr[hash_index] = DELETED;
        } else {
            iterator it = find(key);
            if (it.operator->() == arr + capacity) {
                cout << "There is no such element in the map" << endl;
                return;
            } else {
                (arr + it.hash_index)->~value_type();
                current_size--;
                loadfactor = static_cast<float>(current_size) / capacity;
                status_ptr[hash_index] = DELETED;
            }

        }

    }
    
    void clear() noexcept {
        for (int i = 0; i < capacity; ++i) {
            if (status_ptr[i] == FULL)
                arr[i].~value_type();
        }
    }

    iterator find(K key) {
        int hash_index = hasher_(key) % capacity;
        if (status_ptr[hash_index] == FULL and arr[hash_index].first == key)
            return hash_map_iterator<value_type>(arr,capacity,status_ptr,hash_index);
        else {
            int i = 0;
            while (status_ptr[hash_index] != FULL or status_ptr[hash_index] != EMPTY and
                   arr[hash_index].first != key and i < capacity) {
                ++hash_index;
                hash_index %= capacity;
                ++i;
                if (i == capacity - 1) {
                    return end();
                }
            }
            return hash_map_iterator<value_type>(arr,capacity,status_ptr,hash_index);
        }
    }


//     void rehash(size_type n) {
//         if (capacity == 0)
//             capacity = 3;

//         value_type *temp_arr;
//         vector<status> temp_status;
//         temp_arr = allocator_.allocate(n);
//         temp_status.resize(n);
//         capacity = n;
//         loadfactor = static_cast<float>(current_size) / capacity;

//         iterator it;
//         it.p = arr;
//         it.capacity = capacity;
//         it.status_ = status_ptr;

//         for (int i = 0; i < capacity; ++i) {
//             temp_status[i] = EMPTY;
//         }

//         if (status_ptr[0] != FULL){

//         }

//             it.operator++();

//         for (int i = 0; i < capacity; ++i) {
//             new(temp_arr + i) value_type(arr[i].first, arr[i].second);
//             temp_status[i] = FULL;
//             if (it. == arr + capacity)
//                 break;
//         }
//         arr = allocator_.allocate(n);
//         status_ptr.resize(n);
//         for (int i = 0; i < capacity; ++i) {
//             status_ptr[i] = EMPTY;
//         }

//         for (int i = 0; status_ptr[i] == FULL; i++) {
//             new(arr + i) value_type(temp_arr[i].first, temp_arr[i].second);
//             temp_arr[i].~value_type();
//             status_ptr[i] = FULL;
//         }
//         allocator_.deallocate(temp_arr, n);
//         temp_status.clear();

//     }
};

int main() {
    hash_map<string, int> object(5);
    object.insert("name121", 1);
    cout<<object.find("name121")->second<<endl;
    object.erase("name121");
    cout<<object.find("name121")->second<<endl;
    object.erase("name121");

}
