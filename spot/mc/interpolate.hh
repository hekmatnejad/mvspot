// -*- coding: utf-8 -*-
// Copyright (C) 2016 Laboratoire de Recherche et
// Developpement de l'Epita
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

#include <functional>
#include <stdlib.h>
#include <spot/mc/reachability.hh>
#include <spot/misc/timer.hh>


namespace spot
{
  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class count_valid  : public seq_reach_kripke<State, SuccIterator,
                                       StateHash, StateEqual,
                                       count_valid<State, SuccIterator,
                                                        StateHash, StateEqual>>
  {
  public:
    count_valid(kripkecube<State, SuccIterator>& sys,
                State new_initial,
                std::function<bool(const State)> count_valid_fun,
                std::function<int(State)>  get_depth,
                std::function<int(State)>  get_pos,
                unsigned id)
      : seq_reach_kripke<State, SuccIterator, StateHash, StateEqual,
                         count_valid<State, SuccIterator,
                                     StateHash, StateEqual>>(sys),
      new_initial_(new_initial), count_valid_fun_(count_valid_fun),
      get_depth_(get_depth), get_pos_(get_pos), id_(id)
      {
      }

    virtual ~count_valid()
    {

    }

    void setup()
    {
    }

    void push(State st, unsigned int dfsnum)
    {
      if (dfsnum == 1)
        {
          // Here Hack the reachability to specify startup
          this->visited.erase(st);
          this->sys_.recycle(this->todo.back().it);
          this->todo.pop_back();
          this->todo.push_back({new_initial_, this->sys_.succ(new_initial_)});
          this->visited[new_initial_] = this->dfs_number;
          if (count_valid_fun_(new_initial_))
            ++counter_;
          return;
        }

      if (count_valid_fun_(st))
        {
          ++counter_;
          if (first_depth_ == -1)
            {
              first_depth_ = get_depth_(st);
              first_pos_ = get_pos_(st);
            }
        }
    }

    void edge(unsigned int, unsigned int)
    {
    }

    void finalize()
    {
      float x = (float) counter_ / this->states();
      std::cout << '@' << id_
                << ',' << first_depth_
                << ',' << first_pos_
                << ',' << x
                << ',' << this->states();
    }

  private:
    State new_initial_;
    std::function<bool(const State)> count_valid_fun_;
    std::function<int(State)> get_depth_;
    std::function<int(State)> get_pos_;
    unsigned int counter_ = 0;
    int first_depth_ = -1;
    int first_pos_ = -1;
    unsigned id_ = 0;
  };


  template<typename State, typename SuccIterator,
           typename StateHash, typename StateEqual>
  class interpolate  : public seq_reach_kripke<State, SuccIterator,
                                       StateHash, StateEqual,
                                       interpolate<State, SuccIterator,
                                                        StateHash, StateEqual>>
  {
  public:
    interpolate(kripkecube<State, SuccIterator>& sys,
        std::function<void(State, unsigned int)> display,
        std::function<std::vector<State>*(std::vector<State>&)> interpolate_fun)
      : seq_reach_kripke<State, SuccIterator, StateHash, StateEqual,
                         interpolate<State, SuccIterator,
                                     StateHash, StateEqual>>(sys),
      display_(display), interpolate_fun_(interpolate_fun)
      {
        (void) display;
        depth.reserve(1000);
      }

    virtual ~interpolate()
    {
    }

    void setup()
    {
      tm_.start("original DFS");
    }

    void push(State st, unsigned int dfsnum)
    {
      if (dfsnum <= 150) // FIXME threshold
        sample_.push_back(st);

      depth.insert({st, (int) this->todo.size()});
      dfspos.insert({st, (int) this->dfs_number});
    }

    void edge(unsigned int, unsigned int)
    {
    }

    void finalize()
    {
      tm_.stop("original DFS");

      std::cout << "TOTAL : " << this->states() << std::endl;
      std::cout << "TIME  : " << tm_.timer("original DFS").walltime()
                << std::endl;
      tm_.start("Generation of states");
      auto* gen = interpolate_fun_(sample_);
      tm_.stop("Generation of states");

      std::cout << "GenPop  : "  << tm_.timer("Generation of states").walltime()
                << std::endl;

      for (unsigned i = 0; i < gen->size(); ++i)
        {
          //SPOT_ASSERT(gen[i] != nullptr);
          count_valid<State, SuccIterator, StateHash, StateEqual>
            cv(this->sys_, (*gen)[i], [this](State s) -> bool
               {
                 return this->visited.find(s) != this->visited.end();
               },
               [this](State s) -> int
               {
                 return depth[s];
               },
               [this](State s) -> int
               {
                 return dfspos[s];
               },
               i
               );
          tm_.start("Element " + std::to_string(i));
          cv.run();
          tm_.stop("Element " + std::to_string(i));
          std::cout << ','
                    << abs((int) cv.states() - (int)this->states())
                    << ','
                    << tm_.timer("Element " + std::to_string(i)).walltime()
                    << '\n';
        }
      //      tm_.print(std::cout);
      delete gen;
    }

  private:
    std::function<void(State, unsigned int)> display_;
    std::function<std::vector<State>*(std::vector<State>&)> interpolate_fun_;
    std::vector<State> sample_;
    spot::timer_map tm_;
    typedef std::unordered_map<const State, int,
                               StateHash, StateEqual> visited_map;
    visited_map depth;
    visited_map dfspos;

  };

}
