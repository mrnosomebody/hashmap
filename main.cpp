#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <utility>
#include <type_traits>

using namespace std;

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

template<typename ValueType>
class hash_map_iterator {
private:
    ValueType *p;
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using reference = ValueType &;
    using pointer = ValueType *;

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
        ++p;
        return *this;
    }

    // postfix ++
    hash_map_iterator operator++(int) {
        hash_map_iterator temp = *this;
        p++;
        return temp;
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
// Shouldn't give non const references on value
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using reference = const ValueType &;
    using pointer = const ValueType *;

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
        ++p;
        return *this;
    }

    // postfix ++
    hash_map_const_iterator operator++(int) {
        hash_map_const_iterator temp = *this;
        p++;
        return temp;
    }

    bool operator==(const hash_map_iterator<ValueType> &other) {
        return p == other.p;
    }

    bool operator!=(const hash_map_iterator<ValueType> &other) {
        return p != other.p;
    }
};

enum status {
    DELETED,
    EMPTY,
    FULL
};

template<typename K, typename V>
class HashNode {
    V value;
    K key;
    HashNode *next;
public:
    HashNode(K key, V value) {
        this->value = value;
        this->key = key;
        this->next = nullptr;
    }
};

template<typename K, typename T,
        typename Hash = std::hash<K>,
        typename Pred = std::equal_to<K>,
        typename Alloc = My_allocator<std::pair<const K, T>>>
class hash_map {
private:
    double loadfactor, max_loadfactor = 0.5;
    int current_size, capacity;
    HashNode<K, T> arr[];
public:
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

    /// Default constructor.
    hash_map() = default;

    /**
     *  @brief  Default constructor creates no elements.
     *  @param n  Minimal initial number of buckets.
     */
    explicit hash_map(size_type n) {
        capacity = n;
        if (n != 0) {
            My_allocator<T> allocator;
            arr = allocator.allocate(n);
            for (int i = 0; i < capacity; ++i) {
                arr[i] = EMPTY;
            }
        }
    }

    template<typename InputIterator>
    hash_map(InputIterator first, InputIterator last, size_type n = 0) : hash_map(n) {
        for (auto it = first; it < last; ++it) {
            this->arr.insert(it);
        }
    }

    //если вставлять только валидные элементы то в копии во время вставки вызовется рехеш
    /// Copy constructor.
    hash_map(const hash_map &other) : hash_map(capacity) {
        for (auto it = arr; it < capacity; ++it) {
            other.arr.insert(*it);
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

    explicit hash_map(const allocator_type &a) {
        arr = a.allocate(1);
    }

    hash_map(std::initializer_list<value_type> l, size_type n = 0) : hash_map(n) {
        for (auto it = l.begin(); it != l.end(); ++it) {
            this->arr.insert(it);
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

    /**
     *  @brief  %hash_map list assignment operator.
     *  @param  l  An initializer_list.
     *
     *  This function fills an %hash_map with copies of the elements in
     *  the initializer list @a l.
     *
     *  Note that the assignment completely changes the %hash_map and
     *  that the resulting %hash_map's size is the same as the number
     *  of elements assigned.
     */
    hash_map &operator=(std::initializer_list<value_type> l) {
        for (auto it = l.begin(); it != l.end(); ++it) {
            this->arr.insert(it);
        }
    }

    ///  Returns the allocator object used by the %hash_map.
    allocator_type get_allocator() const noexcept {
        My_allocator<T> alloc;
        return alloc;
    }

    // size and capacity:

    ///  Returns true if the %hash_map is empty.
    bool empty() const noexcept {
        for (auto i :arr) {
            if (arr[i] != EMPTY)
                return false;
        }
        return true;
    }

    ///  Returns the size of the %hash_map.
    size_type size() const noexcept {
        return current_size;
    }

    ///  Returns the maximum size of the %hash_map.
    size_type max_size() const noexcept {
        return capacity;
    }
    // iterators.

    /**
     *  Returns a read/write iterator that points to the first element in the
     *  %hash_map.
     */
    iterator begin() noexcept {
        return arr;
    }

    //@{
    /**
     *  Returns a read-only (constant) iterator that points to the first
     *  element in the %hash_map.
     */
    const_iterator begin() const noexcept {
        return cbegin();
    }

    const_iterator cbegin() const noexcept {
        const_iterator it = arr;
        return it;
    }

    /**
     *  Returns a read/write iterator that points one past the last element in
     *  the %hash_map.
     */
    iterator end() noexcept {
        return arr + capacity;
    }

    //@{
    /**
     *  Returns a read-only (constant) iterator that points one past the last
     *  element in the %hash_map.
     */
    const_iterator end() const noexcept {
        return cend();
    }

    const_iterator cend() const noexcept {
        const_iterator it = arr + capacity;
        return it;
    }
    //@}
    /**
     *  @brief A template function that attempts to insert a range of
     *  elements.
     *  @param  first  Iterator pointing to the start of the range to be
     *                   inserted.
     *  @param  last  Iterator pointing to the end of the range.
     *
     *  Complexity similar to that of the range constructor.
     */
    unsigned int hash_(const string key) {
        unsigned long int value = 0;
        unsigned int i = 0;
        unsigned int key_len = key.length();

        // do several rounds of multiplication
        for (; i < key_len; ++i) {
            value = value * 37 + key[i];
        }

        // make sure value is 0 <= value < TABLE_SIZE
        value = value % capacity;

        return value;
    }
    unsigned int hash_(const int key) {
        return key % capacity;
    }
    void insert(K key, T value) {
        if (loadfactor >= max_loadfactor)
            rehash(capacity*2);

        HashNode<K,T> temp = pair<K,T>(key,value);
        int hash_index = hash_(key);

        while (arr[hash_index] != EMPTY or arr[hash_index] != DELETED
        and hash_index < capacity) {
            ++hash_index;
        }
        if (arr[capacity-1] == FULL)
            rehash(capacity*2);

        arr[hash_index] = temp;
        arr[hash_index] = FULL;
        ++current_size;
    }

    void erase (K key) {
        if (this->find(key) != this->arr.end()) {
            arr[this->find(key)] = nullptr;
            arr[this->find(key)] = DELETED;
        }
    }

    iterator find (K key) {
        int hash_index = hash_(key);
        if (arr[hash_index].key == key)
            return arr[hash_index];
        else {
            ++hash_index;
            for (iterator it = arr[hash_index]; it < capacity; ++it){
                if (arr[hash_index].key == key)
                    return arr[hash_index];
            }
            return arr.end();
        }
    }


    void rehash(size_type n){}
};
