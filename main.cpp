#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <utility>
#include <type_traits>
#include <vector>
#include <limits>
#include <cmath>


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

    hash_map_iterator(pointer p, int capacity, vector<status> status_, int hash_index) :
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
        return (lhs.p + lhs.hash_index == rhs.p + rhs.hash_index);
    }

    friend bool operator!=(const hash_map_iterator<ValueType> &lhs, const hash_map_iterator<ValueType> &rhs) {
        return !(lhs == rhs);
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
        return lhs.p + lhs.hash_index == rhs.p + rhs.hash_index;
    }

    friend bool operator!=(const hash_map_iterator<ValueType> &lhs, const hash_map_iterator<ValueType> &rhs) {
        return !(lhs == rhs);
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
    allocator_type allocator_;
    hasher hasher_;
    key_equal equal_;
public:
    /// Default constructor.
    hash_map() = default;

    /**
     *  @brief  Default constructor creates no elements.
     *  @param n  Minimal initial number of buckets.
     */
    explicit hash_map(size_type n) : status_ptr(n, EMPTY) {
        capacity = n;
        if (n != 0) {
            arr = allocator_.allocate(n);
        }
    }

    ~hash_map() {
        for (int i = 0; i < capacity; ++i) {
            if (status_ptr[i] == FULL)
                arr[i].~value_type();
        }
        allocator_.deallocate(arr, capacity);
    }

    template<typename InputIterator>
    hash_map(InputIterator first, InputIterator last, size_type n = 0) : hash_map(n) {
        for (auto it = first; it != last; ++it) {
            insert(it->first, it->second);
        }

    }


    /// Copy constructor.
    hash_map(const hash_map &other) : hash_map(other.capacity) {
        for (auto it = other.begin(); it != other.end(); ++it) {
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
            insert(it->first, it->second);
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
        it.status_ = status_ptr;
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
    float load_factor(){
        return static_cast<float>(current_size/capacity);
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
    size_type bucket_count() const noexcept {
        return capacity;
    }

    std::pair<iterator, bool> insert(K key, T value) {
        if (capacity == 0) {
            rehash(3);
        }
        if (loadfactor >= max_loadfactor) {
            rehash(capacity * 2);
        }

        bool state;
        int hash_index = hasher_(key) % capacity;
        if (status_ptr[hash_index] == EMPTY) {
            state = true;
            current_size++;
            loadfactor = static_cast<float>(current_size) / capacity;
            status_ptr[hash_index] = FULL;
            new(arr + hash_index) value_type(key, value);
            return pair<iterator, bool>(hash_map_iterator<value_type>(arr, capacity, status_ptr, hash_index), state);
        }
        iterator find_ = find(key);
        if (find_ != end()) {
            state = false;
            return pair<iterator, bool>(find_, state);
        } else{
            iterator it(arr, capacity, status_ptr, hash_index=find_.hash_index);
            it = it.next_free_space();
            state = true;
            current_size++;
            loadfactor = static_cast<float>(current_size) / capacity;
            status_ptr[it.hash_index] = FULL;
            new(arr + it.hash_index) value_type(key, value);
            return pair<iterator, bool>(iterator(arr, capacity, status_ptr, hash_index), state);
        }

    }

    void erase(K key) {
        iterator it = find(key);
        if (it == end()) {
            cout << "There is no such element in the map" << endl;
            return;
        } else {
            (arr + it.hash_index)->~value_type();
            current_size--;
            loadfactor = static_cast<float>(current_size) / capacity;
            status_ptr[it.hash_index] = DELETED;
        }
    }

    void clear() noexcept {
        for (int i = 0; i < capacity; ++i) {
            if (status_ptr[i] == FULL)
                arr[i].~value_type();
        }
    }

    Hash hash_function() const {
        return hasher_;
    }

    Pred key_eq() const {
        return equal_;
    }

    iterator find(K key) {
        int hash_index = hasher_(key) % capacity;
        if (status_ptr[hash_index] == FULL and arr[hash_index].first == key)
            return iterator(arr, capacity, status_ptr, hash_index);
        else {
            int i = 0;
            while (status_ptr[hash_index] != FULL or arr[hash_index].first != key) {
                if (status_ptr[hash_index] == EMPTY)
                    return end();
                ++hash_index;
                hash_index %= capacity;
                if (i == capacity - 1) {
                    return end();
                }
                ++i;
            }
            return iterator(arr, capacity, status_ptr, hash_index);
        }
    }

    void rehash(size_type n) {
        if (n < capacity)
            return;
        hash_map temp(begin(), end(), n);
        swap(temp);
    }

    template<typename _H2, typename _P2>
    void merge(hash_map<K, T, _H2, _P2, Alloc>& source) {
        for (int i = 0; i<source.capacity; ++i){
            if (source.status_ptr[i] == FULL)
                insert(source.arr[i]);
        }
    }

    template<typename _H2, typename _P2>
    void merge(hash_map<K, T, _H2, _P2, Alloc>&& source) {
        for (int i = 0; i<source.capacity; ++i){
            if (source.status_ptr[i] == FULL)
                insert(source.arr[i]);
        }
    }


    mapped_type& operator[](const key_type& k) {
        auto res =insert(k, mapped_type ());
        return res.first->second;
    }

    mapped_type& operator[](key_type&& k) {
        auto res =insert(move(k), mapped_type ());
        return res.first->second;
    }

    mapped_type& at(const key_type& k){
        iterator iter = find(k);
        if(iter == end()) {
            throw std::out_of_range("item not found");
        }
        return (iter->second);
    }


    const mapped_type& at(const key_type& k) const{
        const_iterator iter = find(k);
        if(iter == end()) {
            throw std::out_of_range("item not found");
        }
        return (iter->second);
    }

    void reserve(size_type n) {
        rehash(ceil(n / max_loadfactor));
    }

};

///////////////////////////////////////////

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

TEST_CASE("LAB2") {
SECTION("") {
allocator<int> alloc;
int *place = alloc.allocate(15);
REQUIRE(*place == 15);
}
SECTION("") {
allocator<int> alloc;
int *data = alloc.allocate(50);
alloc.deallocate(data, 50);
}
SECTION("") {
hash_map<int, int>
        table({{21, 1},
               {0, 23},
               {2, 3},
               {-1, 11},
               {-12, 11},
               {21, 1}});
hash_map<int, int> other_table(table.get_allocator());
// table.insert(std::make_pair(7, 1));
// table.insert(std::make_pair(2, 2));
// table.insert(std::make_pair(3, 3));
}

SECTION("") {
hash_map<std::string, int> table(7);
REQUIRE(table.bucket_count() == 7);
REQUIRE(table.size() == 0);
REQUIRE(table.empty());
}
SECTION("") {
hash_map<std::string, int> table;
REQUIRE(table.bucket_count() == 0);
REQUIRE(table.size() == 0);
REQUIRE(table.empty());
REQUIRE(table.load_factor() == 0);
}
SECTION("") {
hash_map<std::string, int> table(2);
std::pair<std::string, int> element{"yes", 1};
// table.insert(element);
// REQUIRE(table.begin()->first == "yes");
// REQUIRE(table.begin()->second == 1);
}
SECTION("") {
std::vector<std::pair<std::string, int>> elements(9);
elements[0] = {"death", 1};
elements[1] = {"rest", 2};
elements[2] = {"in", 3};
elements[3] = {"peace", 4};
elements[4] = {"help", 5};
// fefu::hash_map<std::string, int> table(elements.begin(), elements.end());
// std::cout « table.contains("death");
}
//SECTION("") {
//hash_map<std::string, int> table
//        ({{"death", 1}, {"rest", 23}, {"help", 3}, {"my", 11}, {"give", 11}});
//REQUIRE(table.cbegin() == table.cend());
//}
SECTION("") {
hash_map<int, int> table(10);
table[6] = -120;
table[2] = 12;
REQUIRE(table[6] == -120);
REQUIRE(table[2] == 12);
}
}
