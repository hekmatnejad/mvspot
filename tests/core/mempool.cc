// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et Développement de l'Epita.
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

#include <spot/misc/fixpool.hh>

namespace
{

  struct B
  {
    B(int i): i(i) {}
    int i;
  };

  class foo_t
  {
    B* b;
  public:
    foo_t(int i): b(new B(i)) {}
    ~foo_t() { delete b; }

    void incr() { ++b->i; }
  };

  class bar_t
  {
    static
    spot::fixed_size_pool&
    pool()
    {
      static spot::fixed_size_pool p{sizeof(bar_t)};
      return p;
    }

    int i;
  public:
    bar_t(int i): i(i) {}

    void*
    operator new(size_t)
    {
      return pool().allocate();
    }
    void
    operator delete(void* ptr)
    {
      pool().deallocate(ptr);
    }

    void
    incr()
    {
      ++i;
    }
  };
} // anonymous namespace

static
void
f1(spot::fixed_size_pool& p)
{
  A* a = new (p.allocate()) A(1);
  a->incr();
  // deleted and deallocated: no problem
  a->~A();
  p.deallocate(a);
}

static
void
f2(spot::fixed_size_pool& p)
{
  A* a = new (p.allocate()) A(2);
  a->incr();
  // deallocated but not deleted: member b leaks
  p.deallocate(a);
}

static
void
f3(spot::fixed_size_pool& p)
{
  A* a = new (p.allocate()) A(3);
  a->incr();
  // deleted but not deallocated
  // we want valgrind to detect a leak here
  // it does not, because memory is reachable from the pool, and will be
  // freed when the pool is destroyed.
  a->~A();
}

static
void
f4(spot::fixed_size_pool&)
{
  C* c = new C(4);
  c->incr();
  // not deleted nor deallocated
  // we want valgrind to detect a leak here
}

int main(int argc, char** argv)
{
  spot::fixed_size_pool p(sizeof(A));


  if (argc != 2)
    throw std::runtime_error("wrong number of arguments");

  switch (atoi(argv[1]))
  {
    case 1:
      f1(p);
      break;
    case 2:
      f2(p);
      break;
    case 3:
      f3(p);
      break;
    case 4:
      f4(p);
      break;
    default:
      throw std::runtime_error("unknown argument");
  }

  return 0;
}
