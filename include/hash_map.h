#pragma once

#include <functional>
#include "policy.h"
#include "hash_set.h"

template<
        class Key,
        class T,
        class CollisionPolicy = LinearProbing,
        class Hash = std::hash<Key>,
        class Equal = std::equal_to<Key>
>
class HashMap {

public:
    // types
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = Equal;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;

    static size_type set_hash(const std::pair<Key, T> &value) {
        return hasher(value.first);
    }

    static bool set_eq(const std::pair<Key, T> &l, const std::pair<Key, T> &r) { return key_equal(l.first, r.first); }

    using container_type = HashSet<value_type, CollisionPolicy,
            decltype(&set_hash),
            decltype(&set_eq)>;
    using iterator = typename container_type::Iterator;
    using const_iterator = iterator;

    explicit HashMap(size_type expected_max_size = 0,
                     const hasher &hash = hasher(),
                     const key_equal &equal = key_equal()) {
        m_set = container_type(expected_max_size, hash, equal);
    }

    template<class InputIt>
    HashMap(InputIt first, InputIt last,
            size_type expected_max_size = 0,
            const hasher &hash = hasher(),
            const key_equal &equal = key_equal()) {
        m_set = container_type(first, last, expected_max_size, hash, equal);
    }

    HashMap(const HashMap &) = default;

    HashMap(HashMap &&) noexcept = default;

    HashMap(std::initializer_list<value_type> init,
            size_type expected_max_size = 0,
            const hasher &hash = hasher(),
            const key_equal &equal = key_equal()) {
        m_set = container_type(init, expected_max_size, hash, equal);
    }

    HashMap &operator=(const HashMap &) = default;

    HashMap &operator=(HashMap &&) noexcept = default;

    HashMap &operator=(std::initializer_list<value_type> init) {
        this = new HashMap(init, init.size(), m_set.m_hash, m_set.m_equal);
        return *this;
    }

    iterator begin() noexcept {
        return m_set.begin();
    }

    const_iterator begin() const noexcept {
        return m_set.begin();
    }

    const_iterator cbegin() const noexcept {
        return m_set.cbegin();
    }

    iterator end() noexcept {
        return m_set.end();
    }

    const_iterator end() const noexcept {
        return m_set.end();
    }

    const_iterator cend() const noexcept {
        return m_set.cend();
    }

    bool empty() const {
        return m_set.empty();
    }

    size_type size() const {
        return m_set.size();
    }

    size_type max_size() const {
        return m_set.max_size();
    }

    void clear() {
        m_set.clear();
    }

    std::pair<iterator, bool> insert(const value_type &value) {
        return m_set.insert(value);
    }

    std::pair<iterator, bool> insert(value_type &&value) {
        return m_set.insert(value);
    }

    template<class P>
    std::pair<iterator, bool> insert(P &&value) {
        return m_set.insert(value);
    }

    iterator insert(const_iterator hint, const value_type &value) {
        return insert(hint, value);
    }

    iterator insert(const_iterator hint, value_type &&value) {
        return insert(hint, value);
    }

    template<class P>
    iterator insert(const_iterator hint, P &&value) {
        return insert(hint, value);
    }

    template<class InputIt>
    void insert(InputIt first, InputIt last) {
        m_set.insert(first, last);
    }

    void insert(std::initializer_list<value_type> init) {
        m_set.insert(init);
    }

    template<class M>
    std::pair<iterator, bool> insert_or_assign(const key_type &key, M &&value) {
        m_set.erase(std::make_pair(key, value)); //сравнение происходит только по ключу, поэтому нам неважно значение
        return m_set.insert(std::make_pair(key, value));
    }

    template<class M>
    std::pair<iterator, bool> insert_or_assign(key_type &&key, M &&value) {
        return insert_or_assign(key, value);
    }

    template<class M>
    iterator insert_or_assign(const_iterator hint, const key_type &key, M &&value) {
        auto element = std::make_pair(key, value);
        m_set.erase(element);
        return m_set.insert(hint, element);
    }

    template<class M>
    iterator insert_or_assign(const_iterator hint, key_type &&key, M &&value) {
        return insert_or_assign(hint, key, value);
    }

    // construct element in-place, no copy or move operations are performed;
    // element's constructor is called with exact same arguments as `emplace` method
    // (using `std::forward<Args>(args)...`)
    template<class... Args>
    std::pair<iterator, bool> emplace(Args &&... args);

    template<class... Args>
    iterator emplace_hint(const_iterator hint, Args &&... args);

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const key_type &key, Args &&... args);

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type &&key, Args &&... args);

    template<class... Args>
    iterator try_emplace(const_iterator hint, const key_type &key, Args &&... args);

    template<class... Args>
    iterator try_emplace(const_iterator hint, key_type &&key, Args &&... args);

    iterator erase(const_iterator pos);

    iterator erase(const_iterator first, const_iterator last);

    size_type erase(const key_type &key);

    // exchanges the contents of the container with those of other;
    // does not invoke any move, copy, or swap operations on individual elements
    void swap(HashMap &&other) noexcept;

    size_type count(const key_type &key) const;

    iterator find(const key_type &key);

    const_iterator find(const key_type &key) const;

    bool contains(const key_type &key) const;

    std::pair<iterator, iterator> equal_range(const key_type &key);

    std::pair<const_iterator, const_iterator> equal_range(const key_type &key) const;

    mapped_type &at(const key_type &key);

    const mapped_type &at(const key_type &key) const;

    mapped_type &operator[](const key_type &key);

    mapped_type &operator[](key_type &&key);

    size_type bucket_count() const;

    size_type max_bucket_count() const;

    size_type bucket_size(const size_type) const;

    size_type bucket(const key_type &key) const;

    float load_factor() const;

    float max_load_factor() const;

    void rehash(const size_type count);

    void reserve(size_type count);

    // compare two containers contents
    template<class A,
            class B,
            class C,
            class D,
            class E>
    friend bool operator==(const HashMap &lhs, const HashMap &rhs);

    template<class A,
            class B,
            class C,
            class D,
            class E>
    friend bool operator!=(const HashMap &lhs, const HashMap &rhs);

private:
    HashSet<value_type, CollisionPolicy, Hash, Equal> m_set;
};
