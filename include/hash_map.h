#pragma once

#include "policy.h"
#include <functional>
#include <iostream>
#include <memory>

template<
        class Key,
        class T,
        class CollisionPolicy = LinearProbing,
        class Hash = std::hash<Key>,
        class Equal = std::equal_to<Key>
>
class HashMap {

    class Iterator;

    class ConstIterator;

    struct Node;

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

    using iterator = Iterator;
    using const_iterator = ConstIterator;

    explicit HashMap(size_type expected_max_size = 1,
                     const hasher &hash = hasher(),
                     const key_equal &equal = key_equal()) : m_hash(hash), m_equal(equal) {
        m_elements = std::vector<std::shared_ptr<Node>>(expected_max_size * 2, nullptr);
        m_states = std::vector<State>(expected_max_size * 2, UNDEFINED);
        m_end = nullptr;
        m_begin = nullptr;
        m_last = nullptr;
        m_size = 0;
    }

    template<class InputIt>
    HashMap(InputIt first, InputIt last,
            size_type expected_max_size = 1,
            const hasher &hash = hasher(),
            const key_equal &equal = key_equal()) : HashMap(expected_max_size, hash, equal) {
        for (auto it = first; it != last; ++it) {
            insert(*it);
        }
    }

    HashMap(const HashMap &hm) : HashMap(hm.m_elements.size() / 2, hm.m_hash, hm.m_equal) {
        for (size_type i = 0; i < m_elements.size(); ++i) {
            m_states[i] = hm.m_states[i];
            if (hm.m_elements[i] == nullptr) {
                continue;
            }
            m_elements[i] = std::make_shared<Node>(hm.m_elements[i]->paired_value);
            m_elements[i]->idx = i;
            if (hm.m_begin == hm.m_elements[i]) {
                m_begin = m_elements[i];
            }
            if (hm.m_last == hm.m_elements[i]) {
                m_last = m_elements[i];
            }
        }
        auto prev = m_begin;
        for (auto it = ++hm.begin(); it != hm.end(); ++it) {
            size_type idx = it.data->idx;
            m_elements[idx]->prev = prev;
            prev->next = m_elements[idx];
        }
        m_size = hm.m_size;
    }

    HashMap(HashMap &&hm) noexcept : HashMap(hm.m_elements.size() / 2, hm.m_hash, hm.m_equal) {
        m_elements = std::move(hm.m_elements);
        m_states = std::move(hm.m_states);
        m_size = std::move(hm.m_size);
        m_begin = hm.m_begin;
        m_last = hm.m_last;
        hm.m_begin = hm.m_last = nullptr;
    };

    HashMap(std::initializer_list<value_type> init,
            size_type expected_max_size = 0,
            const hasher &hash = hasher(),
            const key_equal &equal = key_equal()) : HashMap(init.begin(), init.end(), expected_max_size, hash, equal) {}

    HashMap &operator=(const HashMap &hm) = default;

    HashMap &operator=(HashMap &&hm) noexcept = default;

    HashMap &operator=(std::initializer_list<value_type> init) {
        this(init, init.size());
        return *this;
    }

    ~HashMap() {
        erase(begin(), end());
    }

    iterator begin() noexcept {
        return Iterator(m_begin);
    }

    const_iterator begin() const noexcept {
        return ConstIterator(m_begin);
    }

    const_iterator cbegin() const noexcept {
        return ConstIterator(m_begin);
    }

    iterator end() noexcept {
        return Iterator(m_end);
    }

    const_iterator end() const noexcept {
        return ConstIterator(m_end);
    }

    const_iterator cend() const noexcept {
        return ConstIterator(m_end);
    }

    bool empty() const {
        return m_size == 0;
    }

    size_type size() const {
        return m_size;
    }

    size_type max_size() const {
        return m_elements.size();
    }

    void clear() {
        erase(begin(), end());
        m_elements.clear();
        m_states.clear();
        m_size = 0;
    }

    std::pair<iterator, bool> insert(const value_type &value) {
        return insert_by_hint(m_end, std::make_shared<Node>(value));
    }

    std::pair<iterator, bool> insert(value_type &&value) {
        return insert_by_hint(m_end, std::make_shared<Node>(std::move(value)));
    }

    template<class P>
    std::pair<iterator, bool> insert(P &&value) {
        return emplace(std::forward<P>(value));
    }

    iterator insert(const_iterator hint, const value_type &value) {
        return insert_by_hint(hint.data, std::make_shared<Node>(value)).first;
    }

    iterator insert(const_iterator hint, value_type &&value) {
        return insert_by_hint(hint.data, std::make_shared<Node>(std::move(value))).first;
    }

    template<class P>
    iterator insert(const_iterator hint, P &&value) {
        return emplace_hint(hint, std::forward<P>(value));
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

    template<class M>
    std::pair<iterator, bool> insert_or_assign(const key_type &key, M &&value) {
        auto found = find(key);
        if (found.data != m_end) {
            found.data->paired_value.second = std::move(value);
            return std::make_pair(found, false);
        } else {
            return emplace(key, std::forward<M>(value));
        }
    }

    template<class M>
    std::pair<iterator, bool> insert_or_assign(key_type &&key, M &&value) {
        auto found = find(std::forward<const key_type>(key));
        if (found.data != m_end) {
            found.data->paired_value.second = std::move(value);
            return std::make_pair(found, false);
        } else {
            return insert_by_hint(m_end, std::make_shared<Node>(std::move(key), std::forward<M>(value)));
        }
    }

    template<class M>
    iterator insert_or_assign(const_iterator hint, const key_type &key, M &&value) {
        auto found = find(key);
        if (found.data == m_end) {
            return insert(hint, value_type(key, std::forward<M>(value)));
        } else {
            return found;
        }
    }

    template<class M>
    iterator insert_or_assign(const_iterator hint, key_type &&key, M &&value) {
        auto found = find(key);
        if (found.data == m_end) {
            return insert_by_hint(hint.data,
                                  std::make_shared<Node>(std::move(key), std::forward<M>(value))).first;
        } else {
            return found;
        }
    }

    // construct element in-place, no copy or move operations are performed;
    // element's constructor is called with exact same arguments as `emplace` method
    // (using `std::forward<Args>(args)...`)
    template<class... Args>
    std::pair<iterator, bool> emplace(Args &&... args) {
        auto to_emplace = std::make_shared<Node>(std::forward<Args>(args)...);
        return insert_by_hint(m_end, to_emplace);
    }

    template<class... Args>
    iterator emplace_hint(const_iterator hint, Args &&... args) {
        return insert_by_hint(hint.data, std::make_shared<Node>(std::forward<Args>(args)...)).first;
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(const key_type &key, Args &&... args) {
        iterator same = find(key);
        if (same.data != m_end) {
            return std::make_pair(same, false);
        } else {
            return insert_by_hint(m_end, std::make_shared<Node>(
                    std::piecewise_construct,
                    std::forward_as_tuple(std::forward<const Key>(key)),
                    std::forward_as_tuple(std::forward<Args>(args)...)));
        }
    }

    template<class... Args>
    std::pair<iterator, bool> try_emplace(key_type &&key, Args &&... args) {
        iterator same = find(key);
        if (same.data != m_end) {
            return std::make_pair(same, false);
        } else {
            return insert_by_hint(m_end,
                                  std::make_shared<Node>(
                                          std::piecewise_construct, std::forward_as_tuple(std::move(key)),
                                          std::forward_as_tuple(std::forward<Args>(args)...)));
        }
    }

    template<class... Args>
    iterator try_emplace(const_iterator hint, const key_type &key, Args &&... args) {
        iterator same = find(key);
        if (same.data != m_end) {
            return same;
        } else {
            return insert_by_hint(hint.data,
                                  std::make_shared<Node>(std::piecewise_construct,
                                                         std::forward_as_tuple(key),
                                                         std::forward_as_tuple(std::forward<Args>(args)...))).first;
        }
    }

    template<class... Args>
    iterator try_emplace(const_iterator hint, key_type &&key, Args &&... args) {
        iterator same = find(key);
        if (same.data != m_end) {
            return same;
        } else {
            return insert_by_hint(hint.data,
                                  std::make_shared<Node>(std::piecewise_construct,
                                                         std::forward_as_tuple(std::move(key)),
                                                         std::forward_as_tuple(std::forward<Args>(args)...))).first;
        }
    }

    iterator erase(const_iterator pos) {
        return pos.data == m_end ? end() : erase_by_idx(pos.data->idx);
    }

    iterator erase(const_iterator first, const_iterator last) {
        std::vector<size_type> to_erase;
        for (auto it = first; it != last; ++it) {
            to_erase.push_back(it.data->idx);
        }
        iterator last_erased = Iterator(m_end);
        for (auto idx : to_erase) {
            last_erased = erase_by_idx(idx);
        }
        to_erase.clear();

        return last_erased;
    }

    size_type erase(const key_type &key) {
        size_type to_erase_idx = m_hash(key) % m_elements.size();
        if (m_states[to_erase_idx] == DEFINED) {
            erase_by_idx(to_erase_idx);
            return 1;
        }
        return 0;
    }

    // exchanges the contents of the container with those of other;
    // does not invoke any move, copy, or swap operations on individual elements
    void swap(HashMap &&other) noexcept {
        std::swap(m_elements, other.m_elements);
        std::swap(m_states, other.m_states);
        std::swap(m_size, other.m_size);
        std::swap(m_begin, other.m_begin);
        std::swap(m_last, other.m_last);
        std::swap(m_end, other.m_end);
    }

    size_type count(const key_type &key) const {
        return find(key).data == m_end ? 0 : 1;
    }

    iterator find(const key_type &key) {
        size_type start_idx = m_hash(key) % m_elements.size();
        if (m_states[start_idx] == DEFINED && m_equal(key, m_elements[start_idx]->paired_value.first)) {
            return iterator(m_elements[start_idx]);
        }
        for (size_type step_num = 1, idx = CollisionPolicy::next(start_idx, step_num++, m_elements.size());
             m_states[idx] != UNDEFINED;
             idx = CollisionPolicy::next(idx, step_num++, m_elements.size())) {
            if (m_states[idx] == DEFINED && m_equal(m_elements[idx]->paired_value.first, key)) {
                return iterator(m_elements[idx]);
            }
        }
        return iterator(m_end);
    }

    const_iterator find(const key_type &key) const {
        size_type start_idx = m_hash(key) % m_elements.size();
        if (m_states[start_idx] == DEFINED && m_equal(key, m_elements[start_idx]->paired_value.first)) {
            return const_iterator(m_elements[start_idx]);
        }
        for (size_type step_num = 1, idx = CollisionPolicy::next(start_idx, step_num++, m_elements.size());
             m_states[idx] != UNDEFINED;
             idx = CollisionPolicy::next(idx, step_num++, m_elements.size())) {
            if (m_states[idx] == DEFINED && m_equal(m_elements[idx]->paired_value.first, key)) {
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
        return (found.data == m_end ? std::make_pair(ConstIterator(m_end), ConstIterator(m_end))
                                    : std::make_pair(found, ConstIterator(found.data->next)));
    }

    mapped_type &at(const key_type &key) {
        auto found = find(key);
        if (found.data != m_end) {
            return found.data->paired_value.second;
        }
        throw std::out_of_range("HashMap::at");
    }

    const mapped_type &at(const key_type &key) const {
        return at(key);
    }

    mapped_type &operator[](const key_type &key) {
        iterator found = find(key);
        if (found == end()) {
            return insert(
                    value_type(std::piecewise_construct, std::forward_as_tuple(key), std::tuple<>())).first->second;
        } else {
            return found->second;
        }
    }

    mapped_type &operator[](key_type &&key) {
        iterator found = find(key);
        if (found == end()) {
            return insert_by_hint(m_end, std::make_shared<Node>(
                    std::piecewise_construct, std::forward_as_tuple(std::move(key)),
                    std::tuple<>())).first->second;
        } else {
            return found->second;
        }
    }

    size_type bucket_count() const {
        return max_bucket_count();
    }

    size_type max_bucket_count() const {
        return m_elements.size();
    }

    size_type bucket_size(const size_type idx) const {
        return m_elements[idx] == nullptr ? 0 : 1;
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
        if (count > m_elements.size() / 2) {
            std::vector<std::shared_ptr<Node>> tmp(std::move(m_elements));
            m_elements.clear();
            m_states.clear();
            m_elements.resize(count * 2, nullptr);
            m_states.resize(count * 2, UNDEFINED);
            m_end = nullptr;
            m_begin = m_last = m_end;
            m_size = 0;
            for (auto it:tmp) {
                if (it != nullptr) {
                    insert_by_hint(m_end, it);
                }
            }
        }
    }

    void reserve(size_type count) {
        if (count > m_elements.size() / 2) {
            rehash(count);
        }
    }

    // compare two containers contents
    friend bool operator==(const HashMap &lhs, const HashMap &rhs) {
        if (lhs.m_size != rhs.m_size) {
            return false;
        }
        for (auto it = lhs.begin(); it != lhs.end(); ++it) {
            auto el = rhs.find(it->first);
            if (el == rhs.end()) { //doesn't contain key
                return false;
            } else if (!(it->second == el->second)){    //let's believe that mapped_value has operator==
                return false;                           //we don't always have operator!=(((
            }
        }
        return true;
    }

    friend bool operator!=(const HashMap &lhs, const HashMap &rhs) {
        return !(lhs == rhs);
    }

private:
    enum State {
        UNDEFINED, DEFINED, DELETED
    };

    std::pair<iterator, bool>
    insert_by_hint(const std::shared_ptr<Node> &hint, const std::shared_ptr<Node> &to_insert) {
        if (m_elements.size() < 2) {
            reserve(1);
        }
        size_type start_idx = m_hash(to_insert->paired_value.first) % m_elements.size();
        size_type free_idx;
        if (m_states[start_idx] == DELETED || m_states[start_idx] == UNDEFINED) {
            free_idx = start_idx;
        } else {
            if (m_equal(m_elements[start_idx]->paired_value.first, to_insert->paired_value.first)) {
                return std::make_pair(Iterator(m_elements[start_idx]), false);
            }
            size_type step_num = 1;
            for (free_idx = CollisionPolicy::next(start_idx, step_num++, m_elements.size());
                 m_states[free_idx] == DEFINED;
                 free_idx = CollisionPolicy::next(free_idx, step_num++, m_elements.size())) {
                if (free_idx == start_idx ||
                    m_equal(m_elements[free_idx]->paired_value.first, to_insert->paired_value.first)) {
                    return std::make_pair(Iterator(m_elements[free_idx]), false);
                }
            }
        }
        to_insert->idx = free_idx;

        std::shared_ptr<Node> prev = (hint == m_end ? m_last : hint->prev);
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
            rehash(m_elements.size());
        }
        return std::make_pair(Iterator(to_insert), true);
    }

    iterator erase_by_idx(size_type to_erase_idx) {
        if (m_states[to_erase_idx] != DEFINED) {
            return Iterator(m_end);
        }
        std::shared_ptr<Node> to_erase = m_elements[to_erase_idx];
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
        m_elements[to_erase_idx] = nullptr;
        --m_size;
        return Iterator(next);
    }

    const hasher &m_hash;
    const key_equal &m_equal;
    std::vector<std::shared_ptr<Node> > m_elements;

    std::vector<State> m_states;
    std::shared_ptr<Node> m_begin;
    std::shared_ptr<Node> m_last;
    std::shared_ptr<Node> m_end;


    size_type m_size;

    struct Node {

        value_type paired_value;

        size_type idx;
        std::shared_ptr<Node> prev, next;

        template<class...Args>
        Node(Args &&...args)
                : paired_value(std::forward<Args>(args)...) {
            init_rest();
        }


        void init_rest(size_type i = UINT32_MAX, std::shared_ptr<Node> p = nullptr, std::shared_ptr<Node> n = nullptr) {
            idx = i;
            prev = p;
            next = n;
        }

        ~Node() = default;
    };


    class Iterator :
            public std::iterator<std::forward_iterator_tag, value_type, difference_type,
                    pointer, reference> {

        friend class ConstIterator;

        friend class HashMap;

        std::shared_ptr<Node> data;

        Iterator(const std::shared_ptr<Node> &to_construct) {
            data = to_construct;
        }

    public:


        Iterator(const Iterator &it) {
            data = it.data;
        }

        Iterator operator++() {
            data = data == nullptr ? nullptr : data->next;
            return *this;
        }

        Iterator operator++(int) {
            Iterator *res = this;
            data = data == nullptr ? nullptr : data->next;
            return *res;
        }

        reference operator*() {
            return data->paired_value;
        }

        pointer operator->() const {
            return &data->paired_value;
        }

        friend bool operator==(const Iterator l, const Iterator r) {
            return l.data == r.data;
        }

        friend bool operator!=(const Iterator l, const Iterator r) {
            return l.data != r.data;
        }

        Iterator &operator=(Iterator &it) {
            data = it.data;
            return *this;
        }

        Iterator &operator=(Iterator &&it) {
            data = it.data;
            return *this;
        }

        friend typename std::iterator_traits<Iterator>::difference_type distance(Iterator &first, Iterator &last) {
            typename std::iterator_traits<Iterator>::difference_type cnt = 0;
            for (auto it = first; it != last; ++it, ++cnt);
            return cnt;
        }

        friend typename std::iterator_traits<Iterator>::difference_type
        distance(const Iterator &first, const Iterator &last) {
            typename std::iterator_traits<Iterator>::difference_type cnt = 0;
            for (auto it = first; it != last; ++it, ++cnt);
            return cnt;
        }
    };

    class ConstIterator :
            public std::iterator<std::forward_iterator_tag, const value_type, difference_type,
                    const value_type *, const value_type &> {

        friend class Iterator;

        friend class HashMap;

        std::shared_ptr<Node> data;

        ConstIterator(const std::shared_ptr<Node> &node) {
            data = node;
        }


    public:

        ConstIterator(const ConstIterator &it) {
            data = it.data;
        }

        ConstIterator(const Iterator it) {
            data = it.data;
        }

        ConstIterator operator++() {
            data = data == nullptr ? nullptr : data->next;
            return *this;
        }

        const value_type &operator*() const {
            return data->paired_value;
        }

        value_type const *operator->() const {
            return &data->paired_value;
        }

        friend bool operator==(const ConstIterator &l, const ConstIterator &r) {
            return l.data == r.data;
        }

        friend bool operator!=(const ConstIterator &l, const ConstIterator &r) {
            return l.data != r.data;
        }

        ConstIterator &operator=(ConstIterator &it) {
            data = it.data;
            return *this;
        }

        friend typename std::iterator_traits<ConstIterator>::difference_type
        distance(const ConstIterator &first, const ConstIterator &last) {
            typename std::iterator_traits<Iterator>::difference_type cnt = 0;
            for (auto it = first; it != last; ++it, ++cnt);
            return cnt;
        }
    };
};
