// -*- coding: utf-8 -*-
// Copyright (C) 2011, 2015, 2016 Laboratoire de Recherche et
// Développement de l'Epita (LRDE)
//
// This file is part of Spot, a model checking library.
//
// Spot is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// Spot is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
// License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <spot/misc/common.hh>
#include <new>
#include <cstddef>
#include <cstdlib>

#ifdef VALGRIND
#include <valgrind/memcheck.h>
#endif

namespace spot
{

  /// A fixed-size memory pool implementation.
  class fixed_size_pool
  {
  public:
    /// Create a pool allocating objects of \a size bytes.
    fixed_size_pool(size_t size)
      : freelist_(nullptr), free_start_(nullptr),
        free_end_(nullptr), chunklist_(nullptr)
    {
      const size_t alignement = 2 * sizeof(size_t);
      size_ = ((size >= sizeof(block_) ? size : sizeof(block_))
               + alignement - 1) & ~(alignement - 1);
      chunk_size_ = (size_ > 128 ? size_ : 128) * 8192 - 64;
    }

    /// Free any memory allocated by this pool.
    ~fixed_size_pool()
    {
#ifdef VALGRIND
      VALGRIND_DO_LEAK_CHECK;
#endif
      while (chunklist_)
        {
          chunk_* prev = chunklist_->prev;
#ifdef VALGRIND
          VALGRIND_DESTROY_MEMPOOL(chunklist_);
#endif
          free(chunklist_);
          chunklist_ = prev;
        }
    }

    /// Allocate \a size bytes of memory.
    void*
    allocate()
    {
      block_* f = freelist_;
      // If we have free blocks available, return the first one.
      if (f)
        {
          freelist_ = f->next;
#ifdef VALGRIND
          VALGRIND_MAKE_MEM_NOACCESS(&f->next, sizeof(f->next));
          chunk_* ff = reinterpret_cast<chunk_*>(f);
          chunk_* c = chunklist_;
          while (c > ff || c + chunk_size_ < ff)
            c = c->prev;
          VALGRIND_MEMPOOL_ALLOC(c, f, size_);
#endif
          return f;
        }

      // Else, create a block out of the last chunk of allocated
      // memory.

      // If all the last chunk has been used, allocate one more.
      if (free_start_ + size_ > free_end_)
        {
          chunk_* c = reinterpret_cast<chunk_*>(malloc(chunk_size_));
          if (!c)
            throw std::bad_alloc();
          c->prev = chunklist_;
          chunklist_ = c;

          free_start_ = c->data_ + size_;
          free_end_ = c->data_ + chunk_size_;
#ifdef VALGRIND
          VALGRIND_MAKE_MEM_NOACCESS(free_start_, free_end_ - free_start_);
          VALGRIND_CREATE_MEMPOOL(chunklist_, 0, false);
#endif
        }

      void* res = free_start_;
      free_start_ += size_;
#ifdef VALGRIND
      VALGRIND_MEMPOOL_ALLOC(chunklist_, res, size_);
#endif
      return res;
    }

    /// \brief Recycle \a size bytes of memory.
    ///
    /// Despite the name, the memory is not really deallocated in the
    /// "delete" sense: it is still owned by the pool and will be
    /// reused by allocate as soon as possible.  The memory is only
    /// freed when the pool is destroyed.
    void
    deallocate (const void* ptr)
    {
      SPOT_ASSERT(ptr);
#ifdef VALGRIND
      chunk_* ff = reinterpret_cast<chunk_*>(const_cast<void*>(ptr));
      chunk_* c = chunklist_;
      while (c > ff || c + chunk_size_ < ff)
        c = c->prev;
      VALGRIND_MEMPOOL_FREE(c, ptr);
#endif
      block_* b = reinterpret_cast<block_*>(const_cast<void*>(ptr));
#ifdef VALGRIND
      VALGRIND_MAKE_MEM_UNDEFINED(&b->next, sizeof(b->next));
#endif
      b->next = freelist_;
      freelist_ = b;
    }

  private:
    size_t size_;
    size_t chunk_size_;
    struct block_ { block_* next; }* freelist_;
    char* free_start_;
    char* free_end_;
    // chunk = several agglomerated blocks
    union chunk_ { chunk_* prev; char data_[1]; }* chunklist_;
  };
}
