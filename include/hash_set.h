#pragma once

#include "policy.h"
#include <vector>
#include <functional>
#include <iostream>
#include "identical_functions.inl"

template<
        class Key,
        class CollisionPolicy = LinearProbing,
        class Hash = std::hash<Key>,
        class Equal = std::equal_to<Key>
>
class HashSet {
    struct Node;

    class Iterator;

public:
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


    explicit HashSet(size_type expected_max_size = 1,
                     const hasher &hash = hasher(),
                     const key_equal &equal = key_equal()) : m_hash(hash), m_equal(equal) {
        m_capacity = expected_max_size * 2;
        m_elements = std::vector<Node *>(m_capacity, nullptr);
        m_states = std::vector<State>(m_capacity, UNDEFINED);
//        for (size_type i = 0; i < m_capacity; ++i) {
//            m_elements[i] = nullptr;
//            m_states[i] = UNDEFINED;
//        }
        m_end = nullptr;
        m_begin = m_end;
        m_last = m_begin;
        m_size = 0;
    }

    template<class InputIt>
    HashSet(InputIt first, InputIt last,
            size_type expected_max_size = 1,
            const hasher &hash = hasher(),
            const key_equal &equal = key_equal()) : HashSet(expected_max_size, hash, equal) {
        for (auto it = first; it != last; ++it) {
            insert(std::forward<value_type>(*it));
        }
    }

    HashSet(const HashSet &hs) : HashSet(hs.m_capacity / 2, hs.m_hash, hs.m_equal) {
        for (const_iterator it = hs.begin(); it != hs.end(); ++it) {
            emplace(*it);
        }
    }

    HashSet(HashSet &&hs) noexcept : HashSet(hs.m_capacity / 2, hs.m_hash, hs.m_equal) {
        std::vector<Node *> to_move;
        for (auto it = hs.begin(); it != hs.end(); ++it) {
            to_move.push_back(it.data);
        }
        for (size_type i = 0; i < hs.m_capacity; ++i) {
            if (hs.m_elements[i] != nullptr) {
                hs.m_elements[i]->init_rest();
            }
            hs.m_elements[i] = nullptr;
        }
        for (size_type i = 0; i < to_move.size(); ++i) {
            insert_by_hint(m_end, to_move[i]);
        }
        hs.m_begin = hs.m_last = hs.m_end = nullptr;
        hs.clear();
    }

    HashSet(std::initializer_list<value_type> init,
            size_type expected_max_size = 1,
            const hasher &hash = hasher(),
            const key_equal &equal = key_equal()) : HashSet(init.begin(), init.end(), expected_max_size, hash, equal) {}

    HashSet &operator=(const HashSet &) = default;

    HashSet &operator=(HashSet &&) noexcept = default;

    HashSet &operator=(std::initializer_list<value_type> init) {
        this(init, init.size());
    }

    ~HashSet() {
        erase(begin(), end());
    }

    iterator begin() noexcept {
        return iterator(m_begin);
    }

    const_iterator begin() const noexcept {
        return const_iterator(m_begin);
    }

    const_iterator cbegin() const noexcept {
        return const_iterator(m_begin);
    }

    iterator end() noexcept {
        return iterator(m_end);
    }

    const_iterator end() const noexcept {
        return const_iterator(m_end);
    }

    const_iterator cend() const noexcept {
        return const_iterator(m_end);
    }

    bool empty() const {
        return m_size == 0;
    }

    size_type size() const {
        return m_size;
    }

    size_type max_size() const {
        return m_capacity;
    }

    void clear() {
        erase(begin(), end());
        m_elements.clear();
        m_states.clear();
        delete m_begin;
        m_size = 0;
    }

    std::pair<iterator, bool> insert(const value_type &key) {
        return insert_by_hint(m_end, new Node(std::forward<const value_type>(key)));
    }

    std::pair<iterator, bool> insert(value_type &&key) {
        return insert_by_hint(m_end, new Node(std::forward<value_type>(key)));
    }

    iterator insert(const_iterator hint, const value_type &key) {
        return insert_by_hint(hint.data, new Node(std::forward<const value_type>(key))).first;
    }

    iterator insert(const_iterator hint, value_type &&key) {
        return insert_by_hint(hint.data, new Node(std::move(key)));
    }

    template<class InputIt>
    void insert(InputIt first, InputIt last) {
        for (auto it = first; it != last; ++it) {
            insert(*it);
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
        auto to_emplace = new Node(std::forward<Args>(args)...);
        return insert_by_hint(m_end, to_emplace);
    }

    template<class... Args>
    iterator emplace_hint(const_iterator hint, Args &&... args) {
        return insert_by_hint(hint.data, new Node(std::forward<Args>(args)...)).first;
    }

    iterator erase(const_iterator pos) {
        return pos.data == m_end ? end() : erase_by_idx(pos.data->idx);
    }

    iterator erase(const_iterator first, const_iterator last) {
        std::vector<size_type> to_erase;
        for (auto it = first; it != last; ++it) {
            to_erase.push_back(it.data->idx);
        }
        iterator last_erased = iterator(m_end);
        for (auto idx : to_erase) {
            last_erased = erase_by_idx(idx);
        }
        to_erase.clear();

        return last_erased;
    }

    size_type erase(const key_type &key) {
        size_type to_erase_idx = m_hash(key) % m_capacity;
        if (m_states[to_erase_idx] == DEFINED) {
            erase_by_idx(to_erase_idx);
            return 1;
        }
        return 0;
    }

    // exchanges the contents of the container with those of other;
    // does not invoke any move, copy, or swap operations on individual elements
    void swap(HashSet &&other) noexcept {
        std::swap(m_elements, other.m_elements);
        std::swap(m_states, other.m_states);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
        std::swap(m_begin, other.m_begin);
        std::swap(m_last, other.m_last);
        std::swap(m_end, other.m_end);
    }

    size_type count(const key_type &key) const {
        return find(key).data == m_end ? 0 : 1;
    }

    iterator find(const key_type &key) {
        size_type start_idx = m_hash(key) % m_capacity;
        if (m_states[start_idx] == DEFINED && m_equal(key, m_elements[start_idx]->value)) {
            return iterator(m_elements[start_idx]);
        }
        for (size_type step_num = 1, idx = CollisionPolicy::next(start_idx, step_num++, m_capacity);
             m_states[idx] != UNDEFINED;
             idx = CollisionPolicy::next(idx, step_num++, m_capacity)) {
            if (m_states[idx] == DEFINED && m_equal(m_elements[idx]->value, key)) {
                return iterator(m_elements[idx]);
            }
        }
        return iterator(m_end);
    }

    const_iterator find(const key_type &key) const {
        size_type start_idx = m_hash(key) % m_capacity;
        if (m_states[start_idx] == DEFINED && m_equal(key, m_elements[start_idx]->value)) {
            return const_iterator(m_elements[start_idx]);
        }
        for (size_type step_num = 1, idx = CollisionPolicy::next(start_idx, step_num++, m_capacity);
             m_states[idx] != UNDEFINED;
             idx = CollisionPolicy::next(idx, step_num++, m_capacity)) {
            if (m_states[idx] == DEFINED && m_equal(m_elements[idx]->value, key)) {
                return const_iterator(m_elements[idx]);
            }
        }
        return const_iterator(m_end);
    }

    bool contains(const key_type &key) const {
        return find(key).data != m_end;
    }

    std::pair<iterator, iterator> equal_range(const key_type &key) {
        iterator found = find(key);
        return (found.data == m_end ? std::make_pair(Iterator(m_end), Iterator(m_end)) : std::make_pair(found, Iterator(
                found.data->next)));
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type &key) const {
        const_iterator found = find(key);
        return (found.data == m_end ? std::make_pair(const_iterator(m_end), const_iterator(m_end))
                                    : std::make_pair(found, const_iterator(found.data->next)));
    }

    size_type bucket_count() const {
        return max_bucket_count();
    }

    size_type max_bucket_count() const {
        return m_capacity;
    }

    size_type bucket_size(const size_type n) const {
        return m_elements[n] == nullptr ? 0 : 1;
    }

    size_type bucket(const key_type &key) const {
        auto found = find(key);
        return found.data == m_end ? 0 : found.data->idx;
    }

    float load_factor() const {
        return static_cast<float>(m_size) / static_cast<float>(bucket_count());
    }

    float max_load_factor() const {
        return m_size > 0 ? 1.0f : 0.0f;
    }

    void rehash(const size_type count) {
        if (count > m_capacity / 2) {
            std::vector<Node *> tmp(m_elements);
            m_capacity = count * 2;
            m_elements.clear();
            m_states.clear();
            m_elements.resize(m_capacity);
            m_states.resize(m_capacity);
            m_begin = m_last = m_end;
            m_size = 0;
            for (auto it:tmp) {
                if (it != nullptr) {
                    it->init_rest();
                    insert_by_hint(m_end, it);
                }
            }
        }
    }

    void reserve(size_type count) {
        if (count > m_capacity / 2) {
            rehash(count);
        }
    }

    // compare two containers contents
    friend bool operator==(const HashSet &lhs, const HashSet &rhs) {
        if (lhs.m_size != rhs.m_size) {
            return false;
        }
        for (auto it = lhs.begin(); it != lhs.end(); ++it) {
            if (!rhs.contains(*it)) {
                return false;
            }
        }
        return true;
    }

    friend bool operator!=(const HashSet &lhs, const HashSet &rhs) {
        return !(lhs.m_set == rhs.m_set);
    }

private:
    enum State {
        UNDEFINED, DEFINED, DELETED
    };

    size_type m_size;
    std::vector<Node *> m_elements;
    std::vector<State> m_states;
    size_type m_capacity;
    const hasher &m_hash;
    const key_equal &m_equal;
    Node *m_begin;
    Node *m_last;
    Node *m_end;

    std::pair<iterator, bool> insert_by_hint(Node *hint, Node *to_insert) {
        if (m_capacity < 2) {
            reserve(1);
        }
        size_type start_idx = m_hash(to_insert->value) % m_capacity;
        size_type free_idx;
        if (m_states[start_idx] == UNDEFINED ||
            m_states[start_idx] == DELETED) {
            free_idx = start_idx;
        } else {
            if (m_equal(m_elements[start_idx]->value, to_insert->value)) {
                delete to_insert;
                return std::make_pair(Iterator(m_elements[start_idx]), false);
            } else {
                size_type step_num = 1;
                for (free_idx = CollisionPolicy::next(start_idx, step_num++, m_capacity);
                     m_states[free_idx] == DEFINED;
                     free_idx = CollisionPolicy::next(free_idx, step_num++, m_capacity)) {
                    if (free_idx == start_idx || m_equal(m_elements[free_idx]->value, to_insert->value)) {
                        delete to_insert;
                        return std::make_pair(Iterator(m_elements[free_idx]), false);
                    }
                }
            }
        }
        to_insert->idx = free_idx;

        Node *prev = (hint == m_end ? m_last : hint->prev);
        if (prev != nullptr) {
            prev->next = to_insert;
        } else { // если перед элементом никого, то он должен быть начальным
            m_begin = to_insert;
        }
        to_insert->prev = prev;
        if (hint != m_end) {
            hint->prev = to_insert;
        } else { // если после элемента никого, то он последний
            m_last = to_insert;
        }
        to_insert->next = hint;

        m_elements[free_idx] = to_insert;
        m_states[free_idx] = DEFINED;
        ++m_size;

        if (load_factor() > 0.5) {
            rehash(m_capacity);
        }
        return std::make_pair(iterator(to_insert), true);
    }

    iterator erase_by_idx(size_type to_erase_idx) {
        if (m_states[to_erase_idx] != DEFINED) {
            return iterator(m_end);
        }
        Node *to_erase = m_elements[to_erase_idx];
        if (to_erase->next != nullptr) {
            to_erase->next->prev = to_erase->prev;
        } else {
            m_last = to_erase->prev;
        }

        if (to_erase->prev != nullptr) {
            to_erase->prev->next = to_erase->next;
        } else {
            m_begin = to_erase->next;
        }

        m_states[to_erase_idx] = DELETED;

        auto next = to_erase->next;
        delete to_erase;
        m_elements[to_erase_idx] = nullptr;
        --m_size;
        return iterator(next);
    }

    struct Node {
        Key value;

        size_type idx;
        Node *prev, *next;

        template<class...Args>
        Node(Args &&...args)
                :value(std::forward<Args>(args)...) {
            init_rest();
        }

        Node(Node *other) : value(std::forward<Key &>(other->value)) {
            init_rest();
        }

        void init_rest(size_type i = UINT32_MAX, Node *p = nullptr, Node *n = nullptr) {
            idx = i;
            prev = p;
            next = n;
        }

        ~Node() {}
    };

    class Iterator : public std::iterator<std::forward_iterator_tag, value_type, difference_type,
            pointer, reference> {

        friend class HashSet;

        Node *data;

        Iterator(Node *node) {
            data = node;
        }

    public:
        Iterator(const Iterator &it) {
            data = it.data;
        }

        Iterator(Iterator &&) = default;

        Iterator operator++() {
            data = data->next;
            return *this;
        }

        Iterator operator++(int) {
            auto res = *this;
            data = data->next;
            return res;
        }

        const value_type &operator*() const {
            return data->value;
        }

        value_type const *operator->() const {
            return &data->value;
        }

        friend bool operator==(const Iterator &l, const Iterator &r) {
            return l.data == r.data;
        }

        friend bool operator!=(const Iterator &l, const Iterator &r) {
            return l.data != r.data;
        }

        Iterator &operator=(Iterator &&it) noexcept {
            data = it.data;
            return *this;
        }

        Iterator &operator=(const Iterator &it) noexcept {
            data = it.data;
            return *this;
        }

        typename std::iterator_traits<Iterator>::difference_type distance(const Iterator &first, const Iterator &last) {
            typename std::iterator_traits<Iterator>::difference_type cnt = 0;
            for (auto it = first; it != last; ++it, ++cnt);
            return cnt;
        }
    };
};
