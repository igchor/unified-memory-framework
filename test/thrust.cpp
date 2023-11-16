/*
 *  Copyright 2018 NVIDIA Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*! \file 
 *  \brief A caching and pooling memory resource adaptor which uses separate upstream resources for memory allocation
 *      and bookkeeping.
 */

#pragma once

#include <memory_resource>
#include <algorithm>
#include <cassert>

#include <sycl>

namespace mr
{

namespace detail {
template <typename Integer>
Integer clz(Integer x)
{
  Integer result;
    int num_bits = 8 * sizeof(Integer);
    int num_bits_minus_one = num_bits - 1;
    result = num_bits;
    for (int i = num_bits_minus_one; i >= 0; --i)
    {
        if ((Integer(1) << i) & x)
        {
        result = num_bits_minus_one - i;
        break;
        }
    }
  return result;
}

template <typename Integer>
bool is_power_of_2(Integer x)
{
  return 0 == (x & (x - 1));
}

template <typename Integer>
bool is_odd(Integer x)
{
  return 1 & x;
}

template <typename Integer>
Integer log2(Integer x)
{
  Integer num_bits = 8 * sizeof(Integer);
  Integer num_bits_minus_one = num_bits - 1;

  return num_bits_minus_one - clz(x);
}


template <typename Integer>
Integer log2_ri(Integer x)
{
  Integer result = log2(x);

  // This is where we round up to the nearest log.
  if (!is_power_of_2(x))
    ++result;

  return result;
}
}

struct pool_options
{
    /*! The minimal number of blocks, i.e. pieces of memory handed off to the user from a pool of a given size, in a single
     *      chunk allocated from upstream.
     */
    std::size_t min_blocks_per_chunk;
    /*! The minimal number of bytes in a single chunk allocated from upstream.
     */
    std::size_t min_bytes_per_chunk;
    /*! The maximal number of blocks, i.e. pieces of memory handed off to the user from a pool of a given size, in a single
     *      chunk allocated from upstream.
     */
    std::size_t max_blocks_per_chunk;
    /*! The maximal number of bytes in a single chunk allocated from upstream.
     */
    std::size_t max_bytes_per_chunk;

    /*! The size of blocks in the smallest pool covered by the pool resource. All allocation requests below this size will
     *      be rounded up to this size.
     */
    std::size_t smallest_block_size;
    /*! The size of blocks in the largest pool covered by the pool resource. All allocation requests above this size will
     *      be considered oversized, allocated directly from upstream (and not from a pool), and cached only of \p cache_oversized
     *      is true.
     */
    std::size_t largest_block_size;

    /*! The alignment of all blocks in internal pools of the pool resource. All allocation requests above this alignment
     *      will be considered oversized, allocated directly from upstream (and not from a pool), and cached only of
     *      \p cache_oversized is true.
     */
    std::size_t alignment;

    /*! Decides whether oversized and overaligned blocks are cached for later use, or immediately return it to the upstream
     *      resource.
     */
    bool cache_oversized;

    /*! The size factor at which a cached allocation is considered too ridiculously oversized to use to fulfill an allocation
     *      request. For instance: the user requests an allocation of size 1024 bytes. A block of size 32 * 1024 bytes is
     *      cached. If \p cached_size_cutoff_factor is 32 or less, this block will be considered too big for that allocation
     *      request.
     */
    std::size_t cached_size_cutoff_factor;
    /*! The alignment factor at which a cached allocation is considered too ridiculously overaligned to use to fulfill an
     *      allocation request. For instance: the user requests an allocation aligned to 32 bytes. A block aligned to 1024 bytes
     *      is cached. If \p cached_size_cutoff_factor is 32 or less, this block will be considered too overaligned for that
     *      allocation request.
     */
    std::size_t cached_alignment_cutoff_factor;

    /*! Checks if the options are self-consistent.
     *
     *  /returns true if the options are self-consitent, false otherwise.
     */
    bool validate() const
    {
        if (!detail::is_power_of_2(smallest_block_size)) return false;
        if (!detail::is_power_of_2(largest_block_size)) return false;
        if (!detail::is_power_of_2(alignment)) return false;

        if (max_bytes_per_chunk == 0 || max_blocks_per_chunk == 0) return false;
        if (smallest_block_size == 0 || largest_block_size == 0) return false;

        if (min_blocks_per_chunk > max_blocks_per_chunk) return false;
        if (min_bytes_per_chunk > max_bytes_per_chunk) return false;

        if (smallest_block_size > largest_block_size) return false;

        if (min_blocks_per_chunk * smallest_block_size > max_bytes_per_chunk) return false;
        if (min_blocks_per_chunk * largest_block_size > max_bytes_per_chunk) return false;

        if (max_blocks_per_chunk * largest_block_size < min_bytes_per_chunk) return false;
        if (max_blocks_per_chunk * smallest_block_size < min_bytes_per_chunk) return false;

        if (alignment > smallest_block_size) return false;

        return true;
    }
};

/** \addtogroup memory_resources Memory Resources
 *  \ingroup memory_management
 *  \{
 */

/*! A memory resource adaptor allowing for pooling and caching allocations from \p Upstream, using \p Bookkeeper for
 *      management of that cached and pooled memory, allowing to cache portions of memory inaccessible from the host.
 *
 *  On a typical memory resource, calls to \p allocate and \p deallocate actually allocate and deallocate memory. Pooling
 *      memory resources only allocate and deallocate memory from an external resource (the upstream memory resource) when
 *      there's no suitable memory currently cached; otherwise, they use memory they have acquired beforehand, to make
 *      memory allocation faster and more efficient.
 *
 *  The disjoint version of the pool resources uses a separate upstream memory resource, \p Bookkeeper, to allocate memory
 *      necessary to manage the cached memory. There may be many reasons to do that; the canonical one is that \p Upstream
 *      allocates memory that is inaccessible to the code of the pool resource, which means that it cannot embed the necessary
 *      information in memory obtained from \p Upstream; for instance, \p Upstream can be a CUDA non-managed memory
 *      resource, or a CUDA managed memory resource whose memory we would prefer to not migrate back and forth between
 *      host and device when executing bookkeeping code.
 *
 *  This is not the only case where it makes sense to use a disjoint pool resource, though. In a multi-core environment
 *      it may be beneficial to avoid stealing cache lines from other cores by writing over bookkeeping information
 *      embedded in an allocated block of memory. In such a case, one can imagine wanting to use a disjoint pool where
 *      both the upstream and the bookkeeper are of the same type, to allocate memory consistently, but separately for
 *      those two purposes.
 *
 *  \tparam Upstream the type of memory resources that will be used for allocating memory blocks to be handed off to the user
 *  \tparam Bookkeeper the type of memory resources that will be used for allocating bookkeeping memory
 */
template<typename Upstream>
class disjoint_unsynchronized_pool_resource final
    : std::pmr::memory_resource
{
public:
    /*! Get the default options for a disjoint pool. These are meant to be a sensible set of values for many use cases,
     *      and as such, may be tuned in the future. This function is exposed so that creating a set of options that are
     *      just a slight departure from the defaults is easy.
     */
    static pool_options get_default_options()
    {
        pool_options ret;

        ret.min_blocks_per_chunk = 16;
        ret.min_bytes_per_chunk = 1024;
        ret.max_blocks_per_chunk = static_cast<std::size_t>(1) << 20;
        ret.max_bytes_per_chunk = static_cast<std::size_t>(1) << 30;

        ret.smallest_block_size = alignof(std::max_align_t);
        ret.largest_block_size = static_cast<std::size_t>(1) << 20;

        ret.alignment = alignof(std::max_align_t);

        ret.cache_oversized = true;

        ret.cached_size_cutoff_factor = 16;
        ret.cached_alignment_cutoff_factor = 16;

        return ret;
    }

    /*! Constructor.
     *
     *  \param upstream the upstream memory resource for allocations
     *  \param bookkeeper the upstream memory resource for bookkeeping
     *  \param options pool options to use
     */
    disjoint_unsynchronized_pool_resource(Upstream * upstream,
        pool_options options = get_default_options())
        : m_upstream(upstream),
        //m_bookkeeper(bookkeeper),
        m_options(options),
        m_smallest_block_log2(detail::log2_ri(m_options.smallest_block_size))
        //m_pools(m_bookkeeper),
        //m_allocated(m_bookkeeper),
        //m_cached_oversized(m_bookkeeper),
        //m_oversized(m_bookkeeper)
    {
        assert(m_options.validate());

        pointer_vector free;
        pool p(free);
        m_pools.resize(detail::log2_ri(m_options.largest_block_size) - m_smallest_block_log2 + 1, p);
    }

    // TODO: C++11: use delegating constructors

    /*! Destructor. Releases all held memory to upstream.
     */
    ~disjoint_unsynchronized_pool_resource()
    {
        release();
    }

private:
    struct chunk_descriptor
    {
        std::size_t size;
        void* pointer;
    };

    // TODO: use bookeeper as allocator
    typedef std::vector<chunk_descriptor> chunk_vector;

    struct oversized_block_descriptor
    {
        std::size_t size;
        std::size_t alignment;
        void* pointer;

        bool operator==(const oversized_block_descriptor & other) const
        {
            return size == other.size && alignment == other.alignment && pointer == other.pointer;
        }

        bool operator<(const oversized_block_descriptor & other) const
        {
            return size < other.size || (size == other.size && alignment < other.alignment);
        }
    };

    struct equal_pointers
    {
    public:
        equal_pointers(void* p) : p(p)
        {
        }

        bool operator()(const oversized_block_descriptor & desc) const
        {
            return desc.pointer == p;
        }

    private:
        void* p;
    };

    struct matching_alignment
    {
    public:

        matching_alignment(std::size_t requested) : requested(requested)
        {
        }

        bool operator()(const oversized_block_descriptor & desc) const
        {
            return desc.alignment >= requested;
        }

    private:
        std::size_t requested;
    };

    // TODO: use bookeeper as allocator
    typedef std::vector<oversized_block_descriptor> oversized_block_vector;
    typedef std::vector<void*> pointer_vector;

    struct pool
    {
        pool(const pointer_vector & free)
            : free_blocks(free),
            previous_allocated_count(0)
        {
        }

        pool(const pool & other)
            : free_blocks(other.free_blocks),
            previous_allocated_count(other.previous_allocated_count)
        {
        }

        pool & operator=(const pool &) = default;

        ~pool() {}

        pointer_vector free_blocks;
        std::size_t previous_allocated_count;
    };

    typedef std::vector<pool> pool_vector;

    Upstream * m_upstream;
    // Bookkeeper * m_bookkeeper;

    pool_options m_options;
    std::size_t m_smallest_block_log2;

    // buckets containing free lists for each pooled size
    pool_vector m_pools;
    // list of all allocations from upstream for the above
    chunk_vector m_allocated;
    // list of all cached oversized/overaligned blocks that have been returned to the pool to cache
    oversized_block_vector m_cached_oversized;
    // list of all oversized/overaligned allocations from upstream
    oversized_block_vector m_oversized;

public:
    /*! Releases all held memory to upstream.
     */
    void release()
    {
        // reset the buckets
        for (std::size_t i = 0; i < m_pools.size(); ++i)
        {
            m_pools[i].free_blocks.clear();
            m_pools[i].previous_allocated_count = 0;
        }

        // deallocate memory allocated for the buckets
        for (std::size_t i = 0; i < m_allocated.size(); ++i)
        {
            m_upstream->do_deallocate(
                m_allocated[i].pointer,
                m_allocated[i].size,
                m_options.alignment);
        }

        // deallocate cached oversized/overaligned memory
        for (std::size_t i = 0; i < m_oversized.size(); ++i)
        {
            m_upstream->do_deallocate(
                m_oversized[i].pointer,
                m_oversized[i].size,
                m_oversized[i].alignment);
        }

        m_allocated.clear();
        m_oversized.clear();
        m_cached_oversized.clear();
    }

    virtual void* do_allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) override
    {
        bytes = (std::max)(bytes, m_options.smallest_block_size);
        assert(detail::is_power_of_2(alignment));

        // an oversized and/or overaligned allocation requested; needs to be allocated separately
        if (bytes > m_options.largest_block_size || alignment > m_options.alignment)
        {
            oversized_block_descriptor oversized;
            oversized.size = bytes;
            oversized.alignment = alignment;

            if (m_options.cache_oversized && !m_cached_oversized.empty())
            {
                typename oversized_block_vector::iterator it = std::lower_bound(
                    m_cached_oversized.begin(),
                    m_cached_oversized.end(),
                    oversized);

                // if the size is bigger than the requested size by a factor
                // bigger than or equal to the specified cutoff for size,
                // allocate a new block
                if (it != m_cached_oversized.end())
                {
                    std::size_t size_factor = (*it).size / bytes;
                    if (size_factor >= m_options.cached_size_cutoff_factor)
                    {
                        it = m_cached_oversized.end();
                    }
                }

                if (it != m_cached_oversized.end() && (*it).alignment < alignment)
                {
                    it = find_if(it + 1, m_cached_oversized.end(), matching_alignment(alignment));
                }

                // if the alignment is bigger than the requested one by a factor
                // bigger than or equal to the specified cutoff for alignment,
                // allocate a new block
                if (it != m_cached_oversized.end())
                {
                    std::size_t alignment_factor = (*it).alignment / alignment;
                    if (alignment_factor >= m_options.cached_alignment_cutoff_factor)
                    {
                        it = m_cached_oversized.end();
                    }
                }

                if (it != m_cached_oversized.end())
                {
                    oversized.pointer = (*it).pointer;
                    m_cached_oversized.erase(it);
                    return oversized.pointer;
                }
            }

            // no fitting cached block found; allocate a new one that's just up to the specs
            oversized.pointer = m_upstream->do_allocate(bytes, alignment);
            m_oversized.push_back(oversized);

            return oversized.pointer;
        }

        // the request is NOT for oversized and/or overaligned memory
        // allocate a block from an appropriate bucket
        std::size_t bytes_log2 = detail::log2_ri(bytes);
        std::size_t bucket_idx = bytes_log2 - m_smallest_block_log2;
        pool & bucket = m_pools[bucket_idx];

        // if the free list of the bucket has no elements, allocate a new chunk
        // and split it into blocks pushed to the free list
        if (bucket.free_blocks.empty())
        {
            std::size_t bucket_size = static_cast<std::size_t>(1) << bytes_log2;

            std::size_t n = bucket.previous_allocated_count;
            if (n == 0)
            {
                n = m_options.min_blocks_per_chunk;
                if (n < (m_options.min_bytes_per_chunk >> bytes_log2))
                {
                    n = m_options.min_bytes_per_chunk >> bytes_log2;
                }
            }
            else
            {
                n = n * 3 / 2;
                if (n > (m_options.max_bytes_per_chunk >> bytes_log2))
                {
                    n = m_options.max_bytes_per_chunk >> bytes_log2;
                }
                if (n > m_options.max_blocks_per_chunk)
                {
                    n = m_options.max_blocks_per_chunk;
                }
            }

            bytes = n << bytes_log2;

            assert(n >= m_options.min_blocks_per_chunk);
            assert(n <= m_options.max_blocks_per_chunk);
            assert(bytes >= m_options.min_bytes_per_chunk);
            assert(bytes <= m_options.max_bytes_per_chunk);

            chunk_descriptor allocated;
            allocated.size = bytes;
            allocated.pointer = m_upstream->do_allocate(bytes, m_options.alignment);
            m_allocated.push_back(allocated);
            bucket.previous_allocated_count = n;

            for (std::size_t i = 0; i < n; ++i)
            {
                bucket.free_blocks.push_back(
                    static_cast<void*>(
                        static_cast<char*>(allocated.pointer) + i * bucket_size
                    )
                );
            }
        }

        // allocate a block from the front of the bucket's free list
        void* ret = bucket.free_blocks.back();
        bucket.free_blocks.pop_back();
        return ret;
    }

    virtual void do_deallocate(void* p, std::size_t n, std::size_t alignment = alignof(std::max_align_t)) override
    {
        n = (std::max)(n, m_options.smallest_block_size);
        assert(detail::is_power_of_2(alignment));

        // verify that the pointer is at least as aligned as claimed
        // assert(reinterpret_cast<detail::intmax_t>(detail::pointer_traits<void*>::get(p)) % alignment == 0);

        // the deallocated block is oversized and/or overaligned
        if (n > m_options.largest_block_size || alignment > m_options.alignment)
        {
            typename oversized_block_vector::iterator it = find_if(m_oversized.begin(), m_oversized.end(), equal_pointers(p));
            assert(it != m_oversized.end());

            oversized_block_descriptor oversized = *it;

            if (m_options.cache_oversized)
            {
                typename oversized_block_vector::iterator position = lower_bound(m_cached_oversized.begin(), m_cached_oversized.end(), oversized);
                m_cached_oversized.insert(position, oversized);
                return;
            }

            m_oversized.erase(it);

            m_upstream->do_deallocate(p, oversized.size, oversized.alignment);

            return;
        }

        // push the block to the front of the appropriate bucket's free list
        std::size_t n_log2 = detail::log2_ri(n);
        std::size_t bucket_idx = n_log2 - m_smallest_block_log2;
        pool & bucket = m_pools[bucket_idx];

        bucket.free_blocks.push_back(p);
    }
};

/*! \} // memory_resource
 */

} // end mr

int main() {

}