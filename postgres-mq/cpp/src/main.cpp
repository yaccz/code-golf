#include <cmath>
#include <iostream>
#include <pqxx/pqxx>
#include <thread>
#include <chrono>
#include <queue>
#include <ranges>
#include <syncstream>
#include <functional>
#include <memory>
#include <optional>

#if O_VERBOSE == 1
#define VERBOSE(x) osyncstream(cout) << x << endl
#else
#define VERBOSE(x)
#endif

#define INFO(x) osyncstream(cout) << x << endl
#define ERR(x) osyncstream(cerr) << x << endl
#define WVERBOSE(id, x) VERBOSE("Worker " << id << ": " << x)
#define WERR(id, x) ERR("Worker " << id << ": " << x)

#define igetenv(x, def) getenv(x) ? stoi(getenv(x)) : def

using namespace std;
using namespace pqxx;

void insert(connection &C, int i) {
  work W(C);
  W.exec("insert into queue (data) values (" + W.quote(i) + ")");
  W.commit();
}

void worker(
int worker_id,
bool& exit,
shared_ptr<queue<optional<int>>>& result
) {
  WVERBOSE(worker_id, "starting");
  WVERBOSE(worker_id, "got result q " << result.get());
  try {
    connection C("postgres://mq@localhost/mq");
    if (!C.is_open()) {
      WERR(worker_id, "Can't open database");
      return;
    }

    int i=0;
    while(!exit) {
      insert(C, i++);
    }

    WVERBOSE(worker_id, "pushing " << i << " into " << result.get());
    result->push(i);
    try {
      C.disconnect();
    } catch (const std::exception &e) {
    }
  } catch (const std::exception &e) {
    cerr << e.what() << std::endl;
    result->push(nullopt);
    return;
  }
}

optional<int> sample_workers(int n) {
  INFO("Starting " << n << " workers");
  bool exit = false;
  auto results = make_shared<queue<optional<int>>>();
  std::vector<shared_ptr<jthread>> threads;

  for (int worker_id : ranges::views::iota(1, n+1)) {
    shared_ptr<jthread> w = make_shared<jthread>(
      bind(worker, worker_id, ref(exit), ref(results))
    );
    threads.push_back(w);
  }

  int dur = igetenv("WORK_DURATION", 3);
  VERBOSE("Duration: " << dur << "s");
  INFO("Waiting");
  for(auto i : ranges::views::iota(0, dur)) {
    INFO((dur-i) << "s");
    this_thread::sleep_for(chrono::seconds(1));
  }

  VERBOSE("stopping workers");
  exit = true;

  VERBOSE("joining threads");
  for(auto& t : threads) {
    t->join();
  }

  VERBOSE("collecting results");
  int total=0;
  for(int i : ranges::views::iota(0, n)) {
    i=i; // i is unused unless VERBOSE=1
    while(results->empty()) {
      VERBOSE("awaiting results from " << results.get() << ": " << n-i << " left");
      this_thread::sleep_for(chrono::milliseconds(1));
    }
    auto r = results->front();
    if (r.has_value()) {
      total += r.value();
      results->pop();
      INFO(r.value());
    }else{
      return nullopt;
    }
  }

  INFO("Total: " << total);
  cout << endl;
  return total;
}

int main(void) {
  int last=0;
  auto start = igetenv("START_POWER", 0);
  for(auto i : ranges::views::iota(start)) {
    auto r = sample_workers(pow(2, i));
    if (r.has_value()) {
      if (r <= last)
        break;

      last = r.value();
    }else{
      ERR("failed to sample 2^" << i << " workers");
      return 1;
    }
  }

  return 0;
}
