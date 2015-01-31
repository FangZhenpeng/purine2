// Copyright Lin Min 2015
#include <deque>
#include <iterator>
#include <stack>
#include <string>
#include "dispatch/runnable.hpp"

using std::deque;
using std::stack;
using std::to_string;

namespace purine {

Runnable::Runnable(int rank, int device) : Graph(rank, device) {
}

Runnable::~Runnable() {
}

/**
 * @fn prepare_once
 * @brief is called only once before first run.
 *        the purpose of this function is to initialize name_ and root_
 *        for all the subgraphs.
 */
void Runnable::prepare_once() {
  if (prepared_) {
    return;
  } else {
    prepared_ = true;
    deque<Graph*> que;
    std::transform(subgraphs_.begin(), subgraphs_.end(), back_inserter(que),
        [this](const shared_ptr<Graph>& g)->Graph* {
          g->cached_name_ = graph_name_[g.get()];
          g->cached_root_ = this;
          return g.get();
        });
    while (que.size() != 0) {
      Graph* front_g = que.front();
      que.pop_front();
      std::transform(front_g->subgraphs_.begin(), front_g->subgraphs_.end(),
          back_inserter(que),
          [this, front_g](const shared_ptr<Graph>& g)->Graph* {
            g->cached_name_ = front_g->cached_name_
                + "::" + front_g->graph_name_[g.get()];
            g->cached_root_ = this;
            return g.get();
          });
    }
    cached_sources_ = sources();
    cached_sinks_ = sinks();
  }
}

vector<vector<string> > Runnable::print() {
  prepare_once();
  stack<vector<Node*> > stk;
  vector<vector<string> > ret;
  for (Node* node : sources()) {
    stk.push({ node });
  }
  while (stk.empty() == false) {
    vector<Node*> tmp = std::move(stk.top());
    stk.pop();
    Node* end = *tmp.rbegin();
    if (end->outputs().size() == 0) {
      vector<string> tmp_name(tmp.size());
      transform(tmp.begin(), tmp.end(), tmp_name.begin(),
          [] (Node* n)->string {
            return n->cached_name()
                + "[" + to_string(n->rank()) + "]["
                + (n->device() < 0 ? "CPU" : string("GPU")
                    + to_string(n->device())) + "]";
          });
      ret.push_back(tmp_name);
    } else {
      tmp.push_back(end->outputs()[0]);
      for (int i = 1; i < end->outputs().size(); ++i) {
        stk.push({ end, end->outputs()[i] });
      }
      stk.push(tmp);
    }
  }
  return ret;
}

/**
 * @brief return nodes that are sources of the graph
 * (which have no preceding nodes);
 */
vector<Node*> Runnable::sources() {
  vector<Node*> nodes_ = nodes();
  nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
      [](Node* n)->bool { return n->is_source() == false; }), nodes_.end());
  return nodes_;
}

/**
 * @brief return nodes that are sinks of the graph
 * (which have no postceding nodes);
 */
vector<Node*> Runnable::sinks() {
  vector<Node*> nodes_ = nodes();
  nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
          [](Node* n)->bool { return n->is_sink() == false; }), nodes_.end());
  return nodes_;
}

/**
 * @brief run the graph.
 */
void Runnable::run() {
  run_async();
  sync();
}

void Runnable::run_async() {
  prepare_once();
  for (Node* source : cached_sources_) {
    source->compute();
  }
}

void Runnable::sync() {
  if (sink_counter_ == cached_sinks_.size()) {
    return;
  }
}

}