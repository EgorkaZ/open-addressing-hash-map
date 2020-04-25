#pragma once

#include "policy.h"
#include <vector>
#include <functional>

template<
        class Key,
        class CollisionPolicy = LinearProbing,
        class Hash = std::hash<Key>,
        class Equal = std::equal_to<Key>
>
class HashSet {

    static const size_t STANDARD_EXPECTED_SIZE = 100000;

public:
    class Iterator;
    // types
    using key_type = Key;
    using value_type = Key;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = Equal;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;

    using iterator = Iterator;
    using const_iterator = Iterator;


    explicit HashSet(size_type expected_max_size = STANDARD_EXPECTED_SIZE,
                     const hasher &hash = hasher(),
                     const key_equal &equal = key_equal()) {
        m_capacity = expected_max_size;
        m_elements = std::vector<iterator *>(m_capacity);
        m_end_iter = new Iterator(Key(), UINT32_MAX);
        m_begin_iter = new Iterator(Key(), 0);
        m_last_iter = m_begin_iter;
        for (size_type i = 0; i < m_capacity; ++i) {
            m_elements[i] = new Iterator(Key(), i);
        }
        m_size = 0;
        m_hash = hash;
        m_equal = equal;
    }

    template<class InputIt>
    HashSet(InputIt first, InputIt last,
            size_type expected_max_size = STANDARD_EXPECTED_SIZE,
            const hasher &hash = hasher(),
            const key_equal &equal = key_equal()) {
        m_capacity = expected_max_size * 2;
        m_elements = std::vector<iterator>(m_capacity, m_end_iter);
        m_size = 0;
        m_hash = hash;
        m_equal = equal;
        this->insert(first, last);
    }

    HashSet(const HashSet &) = default;

    HashSet(HashSet &&) noexcept = default;

    HashSet(std::initializer_list<value_type> init,
            size_type expected_max_size = STANDARD_EXPECTED_SIZE,
            const hasher &hash = hasher(),
            const key_equal &equal = key_equal()) : HashSet(init.begin(), init.end(), expected_max_size, hash, equal) {}

    HashSet &operator=(const HashSet &) = default;

    HashSet &operator=(HashSet &&) noexcept = default;

    HashSet &operator=(std::initializer_list<value_type> init) {
        m_elements.clear();
        m_elements.resize(init.size() * 2);
        insert(init.begin(), init.end());
    }

    iterator begin() noexcept {
        return *m_begin_iter;
    }

    const_iterator begin() const noexcept {
        return *m_begin_iter;
    }

    const_iterator cbegin() const noexcept {
        return *m_begin_iter;
    }

    iterator end() noexcept {
        return *m_end_iter;
    }

    const_iterator end() const noexcept {
        return end();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    bool empty() const {
        return m_size == 0;
    }

    size_type size() const {
        return m_size;
    }

    size_type max_size() const {
        return m_capacity / 2;
    }

    void clear() {
        m_elements.clear();
        m_size = 0;
    }

    std::pair<iterator, bool> insert(const value_type &key) {
        return insert_by_idx(m_hash(key) % m_capacity, key);
    }

    std::pair<iterator, bool> insert(value_type &&key) {
        return insert(key);
    }

    iterator insert(const_iterator hint, const value_type &key) {
        return insert_by_idx(hint.idx, key).first;
    }

    iterator insert(const_iterator hint, value_type &&key) {
        return insert(hint, key);
    }

    template<class InputIt>
    void insert(InputIt first, InputIt last) {
        auto it = first;
        while (it != last) {
            insert(*it);
            ++it;
        }
    }

    void insert(std::initializer_list<value_type> init) {
        insert(init.begin(), init.end());
    }

    // construct element in-place, no copy or move operations are performed;
    // element's constructor is called with exact same arguments as `emplace` method
    // (using `std::forward<Args>(args)...`)
    template<class... Args>
    std::pair<iterator, bool> emplace(Args &&... args) {
        return insert(*(new Key(args...)));
    }

    template<class... Args>
    iterator emplace_hint(const_iterator hint, Args &&... args) {
        return insert(hint, *(new Key(args...)));
    }

    iterator erase(const_iterator pos) {
        auto next = pos.next;
        pos.prev->next = pos.next;
        pos.next->prev = pos.prev;
        pos.state = Iterator::DELETED;

        if (m_size == 1) {
            m_begin_iter = m_end_iter;
        }
        --m_size;
        return *next;
    }

    iterator erase(const_iterator first, const_iterator last) {
        auto it = first;
        iterator last_erased;
        while (it != last) {
            last_erased = erase(it++);
        }
        return last_erased;
    }

    size_type erase(const key_type &key) {
        return erase(find(key));
    }

    // exchanges the contents of the container with those of other;
    // does not invoke any move, copy, or swap operations on individual elements
    void swap(HashSet &&other) noexcept;

    size_type count(const key_type &key) const;

    iterator find(const key_type &key) {
        size_type start_idx = m_hash(key) % m_capacity;
        if (m_elements[start_idx]->state == Iterator::DEFINED && m_equal(m_elements[start_idx]->key, key)) {
            return *m_elements[start_idx];
        }
        size_type step_num = 1;
        size_type idx = CollisionPolicy::next(start_idx, step_num++, m_capacity);
        for (; idx != start_idx && m_elements[idx]->state != Iterator::UNDEFINED;
               idx = CollisionPolicy::next(idx, step_num++, m_capacity)) {
            if (m_elements[idx]->state == Iterator::DEFINED && m_equal(m_elements[idx]->key, key)) {
                return *m_elements[idx];
            }
        }
        return *m_end_iter;
    }

    const_iterator find(const key_type &key) const {
        return find(key);
    }

    bool contains(const key_type &key) const {
        return find(key) != m_end_iter;
    }

    std::pair<iterator, iterator> equal_range(const key_type &key) {
        return equal_range(key);
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type &key) const {
        auto res = find(key);
        return std::make_pair(res, res.next);
    }

    size_type bucket_count() const {
        return m_size;
    }

    size_type max_bucket_count() const {
        return 10000000;
    }

    size_type bucket_size(const size_type n) const {
        return m_elements[n % m_capacity]->state == Iterator::DEFINED ? 1 : 0;
    }

    size_type bucket(const key_type &key) const {
        return find(key).idx;
    }

    float load_factor() const {
        return static_cast<float>(m_size) / static_cast<float>(m_capacity);
    }

    float max_load_factor() const {
        return m_size > 0 ? 1.f : 0.f;
    }

    void rehash(const size_type count) {
        this = new HashSet(*m_begin_iter, *m_end_iter, count, m_hash, m_equal);
    }

    void reserve(size_type count) {
        rehash(count);
    }

    // compare two containers contents
    friend bool operator==(const HashSet &lhs, const HashSet &rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (auto el : lhs) {
            if (!rhs.contains(el)) {
                return false;
            }
        }
        return true;
    }

    friend bool operator!=(const HashSet &lhs, const HashSet &rhs) {
        return !(lhs == rhs);
    }

private:
    size_type m_capacity;
    size_type m_size;
    std::vector<iterator *> m_elements;
    hasher m_hash;
    key_equal m_equal;
    iterator *m_begin_iter;
    iterator *m_last_iter;
    iterator *m_end_iter;

    std::pair<iterator, bool> insert_by_idx(const size_type &start_idx, const value_type &key) {
        size_type free_idx;
        if (m_elements[start_idx]->state == Iterator::UNDEFINED ||
            m_elements[start_idx]->state == Iterator::DELETED) {
            free_idx = start_idx;
        } else {
            if (m_equal(**m_elements[start_idx], key)) {
                return std::make_pair(*m_elements[start_idx], false);
            } else {
                size_type step_num = 1;
                free_idx = CollisionPolicy::next(start_idx, step_num++, m_capacity);
                while (m_elements[free_idx]->state != Iterator::UNDEFINED ||
                       m_elements[free_idx]->state != Iterator::DELETED) {
                    if (free_idx == start_idx) {
                        return std::make_pair(*m_end_iter, false);
                    }
                    free_idx = CollisionPolicy::next(start_idx, step_num++, m_capacity);
                }
            }
        }
        m_elements[free_idx]->key = key;
        m_elements[free_idx]->state = Iterator::DEFINED;

        if (m_size == 0) {
            m_begin_iter = m_elements[free_idx];
        } else {
            m_elements[free_idx]->prev = m_last_iter;
            m_last_iter->next = m_elements[free_idx];
        }

        m_last_iter = m_elements[free_idx];

        m_elements[free_idx]->next = m_end_iter;
        m_end_iter->prev = m_elements[free_idx];

        ++m_size;
        return std::make_pair(*m_elements[free_idx], true);
    }

public:
    class Iterator {
    public:
        Iterator(Key &&key, size_type idx, Iterator *prev, Iterator *next) {
            this->key = key;
            this->idx = idx;
            this->prev = prev;
            this->next = next;
            state = UNDEFINED;
        }

        Iterator(Key &&key, size_type idx) : Iterator(key, idx, nullptr, nullptr) {}

        Iterator operator++() {
            return next;
        }

        reference operator*() {
            return key;
        }

        pointer operator->() {
            return key.operator->();
        }

        enum State {
            DEFINED, UNDEFINED, DELETED
        };

        Key key;
        size_type idx;
        Iterator *prev, *next;
        State state;
    };
};
