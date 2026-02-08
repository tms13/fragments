#include <concepts>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <type_traits>
#include <utility>

template<typename Key,
         typename T,
         class Hash = std::hash<Key>,
         class KeyEqual = std::equal_to<Key>,
         class Allocator = std::allocator<std::pair<Key const, T>>
>
class lru_cache
{
    using list_type = std::list<std::pair<const Key, T>, Allocator>;
    using key_ref = std::reference_wrapper<const Key>;
    using map_node = std::pair<key_ref const, typename list_type::iterator>;
    using map_allocator = typename std::allocator_traits<Allocator>::rebind_alloc<map_node>;
    using map_type = std::unordered_map<key_ref,
                                        typename list_type::iterator,
                                        Hash, KeyEqual,
                                        map_allocator>;

    map_type::size_type capacity;

    // The key-value pairs live in 'items' list, oldest first.
    list_type items = {};
    // And 'map' provides fast lookup by key.
    map_type map = {};

public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<key_type const, mapped_type>;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using reference = value_type&;
    using const_reference = value_type const&;
    using pointer = std::allocator_traits<Allocator>::pointer;
    using const_pointer = std::allocator_traits<Allocator>::const_pointer;
    using size_type = map_type::size_type;
    using difference_type = map_type::difference_type;
    using iterator = list_type::iterator;
    using const_iterator = list_type::const_iterator;

    // constructors
    constexpr explicit lru_cache(size_type capacity,
                                 Hash const& hash = hasher(),
                                 key_equal const& equal = key_equal(),
                                 allocator_type const& alloc = allocator_type())
        : capacity{capacity},
          items{alloc},
          map{0, hash, equal, map_allocator(alloc)}
    {
        reserve(capacity);
    }

    constexpr lru_cache(size_type capacity,
                        hasher const& hash,
                        allocator_type const& alloc = allocator_type())
        : lru_cache(capacity, hash, {}, alloc)
    {}

    constexpr lru_cache(size_type capacity,
                        allocator_type const& alloc)
        : lru_cache(capacity, {}, {}, alloc)
    {}

    constexpr lru_cache(list_type&& list,
                        size_type capacity,
                        hasher const& hash = hasher(),
                        key_equal const& equal = key_equal(),
                        allocator_type const& alloc = allocator_type())
        : lru_cache(capacity, hash, equal, alloc)
    {
        items.splice(items.end(), list);
        if (items.size() > capacity) {
            items.erase(items.begin(), std::prev(items.end(), capacity));
        }
        build_map();
    }

    template<std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel>
    constexpr lru_cache(InputIt first, Sentinel last,
                        size_type capacity,
                        hasher const& hash = hasher(),
                        key_equal const& equal = key_equal(),
                        allocator_type const& alloc = allocator_type())
        : lru_cache(capacity, hash, equal, alloc)
    {
        insert(first, last);
    }

    template<std::ranges::input_range R>
    constexpr lru_cache(std::from_range_t, R&& range,
                        size_type capacity,
                        hasher const& hash = hasher(),
                        key_equal const& equal = key_equal(),
                        allocator_type const& alloc = allocator_type())
        : lru_cache(std::ranges::begin(range), std::ranges::end(range),
                    capacity, hash, equal, alloc)
    {}

    constexpr lru_cache(std::initializer_list<value_type> init,
                        size_type capacity,
                        hasher const& hash = hasher(),
                        key_equal const& equal = key_equal(),
                        allocator_type const& alloc = allocator_type())
        : lru_cache(std::from_range, init, capacity, hash, equal, alloc)
    {}

    constexpr lru_cache(std::initializer_list<value_type> init,
                        size_type capacity,
                        allocator_type const& alloc)
        : lru_cache(init, capacity, hasher(), key_equal(), alloc)
    {}

    // copy/move constructors and assigment
    constexpr lru_cache(lru_cache const& other)
        : capacity{other.capacity},
          items{other.items},
          map{other.map}
    {
        build_map();
    }

    constexpr lru_cache(lru_cache const& other, std::type_identity_t<Allocator> const& alloc)
        : capacity{other.capacity},
          items{other.items, alloc},
          map{other.map, alloc}
    {
        build_map();
    }

    constexpr lru_cache(lru_cache&& other)
        : capacity{other.capacity},
          items{std::move(other.items.get_allocator())},
          map{std::move(other.map)}
    {
        items.splice(items.end(), other.items);
    }

    constexpr lru_cache(lru_cache&& other, std::type_identity_t<Allocator> const& alloc)
        : capacity{other.capacity},
          items{alloc},
          map{std::move(other.map), alloc}
    {
        items.splice(items.end(), other);
    }

    lru_cache& operator=(lru_cache const& other)
    {
        items.clear();
        items.insert_range(items.end(), other);
        capacity = other.capacity;
        map = other.map;
        build_map();
        return *this;
    }

    lru_cache& operator=(lru_cache&& other)
        noexcept(std::allocator_traits<allocator_type>::is_always_equal::value
                 && std::is_nothrow_swappable<hasher>::value
                 && std::is_nothrow_swappable<key_equal>::value)
    {
        swap(other);
        return *this;
    }

    constexpr lru_cache& operator=(std::initializer_list<value_type> init)
    {
        clear();
        insert(init);
    }

    constexpr allocator_type get_allocator() const noexcept
    {
        return items.get_allocator();
    }

    // iterators
    constexpr iterator begin() { return items.begin(); }
    constexpr const_iterator begin() const { return items.begin(); }
    constexpr const_iterator cbegin() const { return items.begin(); }
    constexpr iterator end() { return items.end(); }
    constexpr const_iterator end() const { return items.end(); }
    constexpr const_iterator cend() const { return items.end(); }
    constexpr iterator rbegin() { return items.rbegin(); }
    constexpr const_iterator rbegin() const { return items.rbegin(); }
    constexpr const_iterator crbegin() const { return items.rbegin(); }
    constexpr iterator rend() { return items.rend(); }
    constexpr const_iterator rend() const { return items.rend(); }
    constexpr const_iterator crend() const { return items.rend(); }

    // capacity
    constexpr size_type size() const noexcept { return items.size(); }
    constexpr size_type max_size() const noexcept { return capacity; }
    constexpr size_type empty() const noexcept { return items.empty(); }

    // modifiers
    constexpr void touch(const_iterator it)
    {
        items.splice(items.end(), items, it);
    }

    constexpr void touch(key_type const& key)
    {
        auto it = map.find(key);
        if (it == map.end()) {
            // not present - ignore
            return;
        }
        touch(it->second);
    }

    template<typename K>
    constexpr void touch(K const& key)
        requires requires { map.find(key); }
    {
        auto it = map.find(key);
        if (it == map.end()) {
            // not present - ignore
            return;
        }
        touch(it->second);
    }

    constexpr void clear() noexcept
    {
        map.clear();
        items.clear();
    }

    constexpr void resize(size_type new_capacity)
    {
        capacity = new_capacity;
        while (items.size() > capacity) {
            map.erase(items.front().first);
            items.pop_front();
        }
        map.reserve(new_capacity);
    }

    constexpr iterator erase(const_iterator pos)
    {
        map.erase(pos->first);
        return items.erase(pos);
    }
    constexpr iterator erase(const_iterator first, const_iterator last)
    {
        for (auto it = first;  it != last;  ++it) {
            map.erase(it->first);
        }
        return items.erase(first, last);
    }
    constexpr size_type erase(key_type const& key)
    {
        auto it = map.find(key);
        if (it == map.end()) {
            return 0;
        }
        items.erase(it->second);
        map.erase(it);
        return 1;
    }

    template<class K> constexpr size_type erase(K&& key)
        requires requires { map.find(key); }
    {
        auto it = map.find(key);
        if (it == map.end()) {
            return 0;
        }
        items.erase(it->second);
        map.erase(it);
        return 1;
    }

    constexpr void swap(lru_cache& other)
        noexcept(std::allocator_traits<allocator_type>::is_always_equal::value
                 && std::is_nothrow_swappable<hasher>::value
                 && std::is_nothrow_swappable<key_equal>::value)
    {
        items.swap(other.items);
        map.swap(other.map);
        std::swap(capacity, other.capacity);
    }

    std::pair<iterator,bool> insert(value_type const& value)
    { return internal_insert(end(), value); }

    std::pair<iterator,bool> insert(value_type&& value)
    { return internal_insert(end(), std::move(value)); }

    template<typename P> requires std::constructible_from<value_type, P>
    std::pair<iterator,bool> insert(P&& value)
    { return emplace(std::forward<P>(value)); }

    // hints are ignored - we always insert at front
    constexpr iterator insert(const_iterator hint, value_type const& value)
    { return internal_insert(hint, value).first; }

    constexpr iterator insert(const_iterator hint, value_type&& value)
    { return internal_insert(hint, std::move(value)).first; }

    template<typename P>
    constexpr iterator insert(const_iterator hint, P&& value)
    {return emplace(hint, std::forward<P>(value)).first; }

    template<std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel>
    constexpr void insert(InputIt first, Sentinel last)
        requires requires(InputIt it) { insert(*it); }
    {
        if constexpr (std::sized_sentinel_for<InputIt, Sentinel> and std::bidirectional_iterator<InputIt>) {
            if (size_type(last - first) > capacity) {
                first = std::prev(last, capacity);
            }
            if (size_type(last - first) + size() > capacity) {
                auto const surplus = size() + (last - first) - capacity;
                erase(begin(), std::next(begin(), surplus));
            }
        }
        while (first != last) {
            insert(*first++);
        }
    }

    void insert(std::initializer_list<value_type> init)
    {
        insert(init.begin(), init.end());
    }

    template<std::ranges::input_range R>
    requires std::convertible_to<value_type, std::ranges::range_value_t<R>>
    constexpr void insert_range(R&& range)
    {
        insert(std::ranges::begin(range), std::ranges::end(range));
    }

    template<typename... Args>
    constexpr std::pair<iterator,bool> emplace(Args&&... args)
    {
        return internal_emplace(end(), std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr iterator emplace_hint(const_iterator position, Args&&... args)
    {
        // we always ignore the hint
        return internal_emplace(position, std::forward<Args>(args)...).first;
    }

    template<typename... Args>
    constexpr std::pair<iterator,bool> try_emplace(key_type const& key, Args&&... args)
    {
        return internal_try_emplace(end(), key, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr std::pair<iterator,bool> try_emplace(key_type&& key, Args&&... args)
    {
        return internal_try_emplace(end(), std::move(key), std::forward<Args>(args)...);
    }

    template<typename K, typename... Args>
    constexpr std::pair<iterator,bool> try_emplace(K&& key, Args&&... args)
        requires requires { map.find(key); }
    {
        return internal_try_emplace(end(), std::forward<K>(key), std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr iterator try_emplace(const_iterator position, key_type const& key, Args&&... args)
    {
        return internal_try_emplace(position, key, std::forward<Args>(args)...).first;
    }

    template<typename... Args>
    constexpr std::pair<iterator,bool> try_emplace(const_iterator position, key_type&& key, Args&&... args)
    {
        return internal_try_emplace(position, std::move(key), std::forward<Args>(args)...).first;
    }

    template<typename K, typename... Args>
    constexpr std::pair<iterator,bool> try_emplace(const_iterator position, K&& key, Args&&... args)
        requires requires { map.find(key); }
    {
        return internal_try_emplace(position, std::forward<K>(key), std::forward<Args>(args)...).first;
    }

    template<class M>
    constexpr std::pair<iterator, bool> insert_or_assign(key_type const& key, M&& obj) {
        return insert_or_assign(end(), key, std::forward<M>(obj));
    }

    template<class M>
      constexpr std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& obj) {
        return insert_or_assign(end(), std::move(key), std::forward<M>(obj));
    }

    template<class K, class M>
      constexpr std::pair<iterator, bool> insert_or_assign(K&& key, M&& obj) {
        return insert_or_assign(end(), std::forward<K>(key), std::forward<M>(obj));
    }

    template<class M>
      constexpr iterator insert_or_assign(const_iterator hint, key_type const& key, M&& obj) {
        auto [position, inserted] = internal_try_emplace(hint, key, std::forward<M>(obj));
        if (!inserted) {
            position->second = std::forward<M>(obj);
        }
        return {position, inserted};
    }

    template<class M>
      constexpr iterator insert_or_assign(const_iterator hint, key_type&& key, M&& obj) {
        auto [position, inserted] = internal_try_emplace(hint, std::move(key), std::forward<M>(obj));
        if (!inserted) {
            position->second = std::forward<M>(obj);
        }
        return {position, inserted};
    }

    template<class K, class M>
      constexpr iterator insert_or_assign(const_iterator hint, K&& key, M&& obj) {
        auto [position, inserted] = internal_try_emplace(hint, std::forward<K>(key), std::forward<M>(obj));
        if (!inserted) {
            position->second = std::forward<M>(obj);
        }
        return {position, inserted};
    }


    // observers
    hasher hash_function() const { return map.hash_function(); }
    key_equal key_eq() const { return map.key_eq(); }

    // map operations
    constexpr iterator find(key_type const& k)
    {
        if (auto it = map.find(k);  it != map.end) {
            return it->second;
        }
        return end();
    }

    constexpr const_iterator find(key_type const& k) const
    {
        if (auto it = map.find(k);  it != map.end) {
            return it->second;
        }
        return end();
    }

    template<typename K>
    constexpr iterator find(K const& k)
        requires requires { map.find(k); }
    {
        if (auto it = map.find(k);  it != map.end) {
            return it->second;
        }
        return end();
    }

    template<typename K>
    constexpr const_iterator find(K const& k) const
        requires requires { map.find(k); }
    {
        if (auto it = map.find(k);  it != map.end) {
            return it->second;
        }
        return end();
    }

    constexpr size_type count(key_type const& k) const { return map.count(k); }

    template<typename K>
    constexpr size_type count(K const& k) const { return map.count(k); }

    constexpr bool contains(key_type const& k) const { return map.contains(k); }

    template<typename K>
    constexpr bool contains(K const& k) const { return map.contains(k); }

    constexpr std::pair<iterator, iterator> equal_range(key_type const& k)
    {
        auto it = find(k);
        return { it, it + (it != map.end()) };
    }

    constexpr std::pair<const_iterator, const_iterator> equal_range(key_type const& k) const
    {
        auto it = find(k);
        return { it, it + (it != map.end()) };
    }

    template<typename K>
    constexpr std::pair<iterator, iterator> equal_range(K const& k)
        requires requires { map.find(k); }
    {
        auto it = find(k);
        return { it, it + (it != map.end()) };
    }

    template<typename K>
    constexpr std::pair<const_iterator, const_iterator> equal_range(K const& k) const
        requires requires { map.find(k); }
    {
        auto it = find(k);
        return { it, it + (it != map.end()) };
    }

    constexpr mapped_type& operator[](key_type const& key)
    {
        auto [it, inserted] = emplace(key, mapped_type()); // Q: non-movable mapped_type?
        touch(it);
        return it->second;
    }

    constexpr mapped_type& operator[](key_type&& key)
        requires std::constructible_from<mapped_type>
    {
        auto [it, inserted] = emplace(std::move(key), mapped_type());
        touch(it);
        return it->second;
    }

    template<typename K>
    constexpr mapped_type& operator[](K&& key)
    {
        auto [it, inserted] = emplace(std::forward<K>(key), mapped_type());
        touch(it);
        return it->second;
    }

    constexpr mapped_type& at(key_type const& k)
    {
        auto it = map.at(k);
        touch(it);
        return it->second;
    }

    constexpr mapped_type const& at(key_type const& k) const
    {
        auto it = map.at(k);
        touch(it);
        return it->second;
    }

    template<typename K>
    constexpr mapped_type& at(K const& k)
        requires requires { map.at(k); }
    {
        auto it = map.at(k);
        touch(it);
        return it->second;
    }

    template<typename K>
    constexpr mapped_type const& at(K const& k) const
        requires requires { map.at(k); }
    {
        auto it = map.at(k).second;
        return it->second;
    }

    // peek() functions perform lookup without promoting the entry to most
    // recently used position.  They return a null pointer if the key is
    // not present.
    // Q: return std::optional<std::reference_wrapper<mapped_type>> instead?
    constexpr mapped_type* peek(key_type const& k)
    {
        auto it = map.find(k);
        return it == map.end() ? nullptr : &it->second->second;
    }

    constexpr mapped_type const* peek(key_type const& k) const
    {
        auto it = map.find(k);
        return it == map.end() ? nullptr : &it->second->second;
    }

    template<typename K>
    constexpr mapped_type* peek(K const& k)
        requires requires { map.find(k); }
    {
        auto it = map.find(k);
        return it == map.end() ? nullptr : &it->second->second;
    }

    template<typename K>
    constexpr mapped_type const* peek(K const& k) const
        requires requires { map.find(k); }
    {
        auto it = map.find(k);
        return it == map.end() ? nullptr : &it->second->second;
    }

    // hash policy
    constexpr float load_factor() const noexcept { return map.load_factor(); }
    constexpr float max_load_factor() const noexcept { return map.max_load_factor(); }
    constexpr void max_load_factor(float z) { map.max_load_factor(z); }
    constexpr void rehash(size_type n) { map.rehash(n); }
    constexpr void reserve(size_type n) { map.reserve(n); }

private:
    void build_map()
    {
        // This function assumes that if map has any entries, then
        // they are the correct keys for the items in the list.
        for (auto it = items.begin();  it != items.end();  ++it) {
            map[it->first] = it;
        }
    }

    template<typename V>
    std::pair<iterator,bool> internal_insert(iterator it, V&& value) {
        if (auto map_it = map.find(value.first); map_it != map.end()) {
            return { map_it->second, false };
        }
        // Trim list if needed
        if (items.size() >= capacity) {
            map.erase(items.front().first);
            items.pop_front();
        }
        items.emplace(it, std::forward<V>(value));
        --it;                   // now points at the new item
        map.emplace(it->first, it).first;
        return { it, true };
    }

    template<typename... Args>
    constexpr std::pair<iterator,bool> internal_emplace(iterator it, Args&&... args)
    {
        return internal_insert(it, value_type(std::forward<Args>(args)...));
    }

    template<typename K, typename... Args>
    constexpr std::pair<iterator,bool> internal_try_emplace(iterator it, K&& key, Args&&... args)
        requires requires { map.find(key); }
    {
        if (auto map_it = map.find(key);  map_it != map.end()) {
            return { map_it->second, false };
        }
        // Trim list if needed
        if (items.size() >= capacity) {
            map.erase(items.front().first);
            items.pop_front();
        }
        items.emplace(it,
                      std::piecewise_construct,
                      std::forward_as_tuple(std::forward<K>(key)),
                      std::forward_as_tuple(std::forward<Args>(args)...));
        --it;                   // now points at the new item
        map.emplace(it->first, it).first;
        return { it, true };
    }

};

// Deduction guides

namespace traits
{
    template<std::input_iterator I>
    using iter_value_t = std::iterator_traits<I>::value_type;

    template<std::input_iterator I>
    using iter_key_t = std::remove_const_t<typename iter_value_t<I>::first_type>;

    template<std::input_iterator I>
    using iter_mapped_t = typename iter_value_t<I>::second_type;

    template<std::ranges::input_range R>
    using range_value_t = std::ranges::range_value_t<R>;

    template<std::ranges::input_range I>
    using range_key_t = std::remove_const_t<typename range_value_t<I>::first_type>;

    template<std::ranges::input_range I>
    using range_mapped_t = typename range_value_t<I>::second_type;
}

template<std::input_iterator InputIt, std::sentinel_for<InputIt> Sentinel,
         typename Hash = std::hash<traits::iter_key_t<InputIt>>,
         typename KeyEqual = std::equal_to<traits::iter_key_t<InputIt>>,
         typename Allocator = std::allocator<traits::iter_value_t<InputIt>>>
lru_cache(InputIt, Sentinel, std::size_t,
          Hash = Hash(), KeyEqual = KeyEqual(), Allocator = Allocator())
    -> lru_cache<traits::iter_key_t<InputIt>, traits::iter_mapped_t<InputIt>,
                 Hash, KeyEqual, Allocator>;

template<std::ranges::input_range R,
         typename Hash = std::hash<traits::range_key_t<R>>,
         typename KeyEqual = std::equal_to<traits::range_key_t<R>>,
         typename Allocator = std::allocator<traits::range_value_t<R>>>
lru_cache(std::from_range_t, R&&, std::size_t,
          Hash = Hash(), KeyEqual = KeyEqual(), Allocator = Allocator())
    -> lru_cache<traits::range_key_t<R>, traits::range_mapped_t<R>, Hash, KeyEqual, Allocator>;

template<typename Key, typename T,
         typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>,
         typename Allocator = std::allocator<std::pair<Key const, T>>>
lru_cache(std::initializer_list<std::pair<Key const, T>>, std::size_t,
          Hash = Hash(), KeyEqual = KeyEqual(), Allocator = Allocator())
    -> lru_cache<Key, T, Hash, KeyEqual, Allocator>;


#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <format>
#include <memory>
#include <string>

static auto numbers()
{
    auto a = lru_cache<int,std::string>(3);
    a[1] = "one";
    a[2] = "two";
    return a;
}

TEST(constructor, list)
{
    std::list<std::pair<const int, int>> content
        = { {4, 40},  {6, 60},  {8, 80} };
    auto cache = lru_cache(std::move(content), 2);

    auto keys = cache | std::views::keys | std::ranges::to<std::vector>();
    EXPECT_THAT(keys, testing::ElementsAre(6, 8));
}

TEST(constructor, iterators)
{
    std::array<std::pair<const int, int>, 3> content
        = {{ {4, 40},  {6, 60},  {8, 80} }};
    auto cache = lru_cache(content.begin(), content.end(), 2);

    auto keys = cache | std::views::keys | std::ranges::to<std::vector>();
    EXPECT_THAT(keys, testing::ElementsAre(6, 8));
}

TEST(constructor, range)
{
    std::array<std::pair<const int, int>, 3> content
        = {{ {4, 40},  {6, 60},  {8, 80} }};
    auto cache = lru_cache(std::from_range, content, 2);

    auto keys = cache | std::views::keys | std::ranges::to<std::vector>();
    EXPECT_THAT(keys, testing::ElementsAre(6, 8));
}

TEST(constructor, init_list)
{
    std::initializer_list<std::pair<int const, int>> content
        = { {4, 40},  {6, 60},  {8, 80} };
    auto cache = lru_cache(content, 2);

    auto keys = cache | std::views::keys | std::ranges::to<std::vector>();
    EXPECT_THAT(keys, testing::ElementsAre(6, 8));
}

TEST(constructor, copy)
{
    auto a = numbers();

    auto b = a;
    EXPECT_EQ(*a.peek(1), "one");
    EXPECT_EQ(*b.peek(1), "one");
    EXPECT_NE(a.peek(1), b.peek(1));
}

TEST(constructor, move)
{
    auto a = numbers();
    auto two_addr = std::addressof(a[2]);
    ASSERT_FALSE(a.empty());

    auto b = std::move(a);
    EXPECT_TRUE(a.empty());
    // Check that the value has moved to b
    EXPECT_EQ(two_addr, std::addressof(b[2]));
}

TEST(assignment, copy)
{
    auto a = numbers();
    auto b = lru_cache<int,std::string>(20);
    b[5] = "five";

    b = a;
    // Ensure values are the same, but addresses are different.
    EXPECT_EQ(a[1], "one");
    EXPECT_EQ(b[1], "one");
    EXPECT_NE(a.peek(1), b.peek(1));

    // Check that modifying b doesn't affect a.
    b[1] = "ONE";
    EXPECT_EQ(a[1], "one");
    EXPECT_EQ(b[1], "ONE");
}

TEST(assignment, move)
{
    auto a = numbers();
    auto two_addr = std::addressof(a[2]);
    ASSERT_FALSE(a.empty());
    auto b = lru_cache<int,std::string>(20);
    b[5] = "five";

    b = std::move(a);
    EXPECT_THROW(b.at(5), std::out_of_range);
    EXPECT_EQ(two_addr, std::addressof(b[2]));
}

TEST(touch, key)
{
    auto a = numbers();
    auto keys_before = a | std::views::keys | std::ranges::to<std::vector>();
    EXPECT_THAT(keys_before, testing::ElementsAre(1, 2));

    a.touch(1);
    auto keys_after = a | std::views::keys | std::ranges::to<std::vector>();
    EXPECT_THAT(keys_after, testing::ElementsAre(2, 1));

    EXPECT_EQ(*a.peek(2), "two");
    // has not altered the sequence
    EXPECT_THAT(keys_after, testing::ElementsAre(2, 1));
}

TEST(clear, all)
{
    auto a = numbers();
    a.clear();
    EXPECT_EQ(a.size(), 0uz);
    EXPECT_EQ(nullptr, a.peek(1));
}

TEST(resize, smaller)
{
    auto a = numbers();
    a.resize(1);
    EXPECT_EQ(a.size(), 1uz);
    auto keys_after = a | std::views::keys | std::ranges::to<std::vector>();
    EXPECT_THAT(keys_after, testing::ElementsAre(2));
}


TEST(resize, bigger)
{
    auto a = numbers();
    a.resize(4);
    EXPECT_EQ(a.size(), 2uz);
    auto keys_after = a | std::views::keys | std::ranges::to<std::vector>();
    EXPECT_THAT(keys_after, testing::ElementsAre(1, 2));
}
