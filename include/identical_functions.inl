#define M_SWAP(type)\
    std::swap(m_elements, other.m_elements);\
    std::swap(m_states, other.m_states);\
    std::swap(m_size, other.m_size);\
    std::swap(m_capacity, other.m_capacity);\
    std::swap(m_begin, other.m_begin);\
    std::swap(m_last, other.m_last);\
    std::swap(m_end, other.m_end);\

