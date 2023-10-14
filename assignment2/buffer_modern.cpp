#include <algorithm>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <deque>

// although it is good habit, you don't have to type 'std::' before many objects by including this line

class Logger
{
private:
  std::mutex log_mutex;
  std::vector<std::string> log;

public: 
  size_t length() {
    std::unique_lock<std::mutex> lock(log_mutex);
    return log.size();
  }

  void write(std::string msg) {
    std::unique_lock<std::mutex> lock(log_mutex);
    log.push_back(msg);
    return;
  }

  std::string read_oldest_and_delete () {
    std::unique_lock<std::mutex> lock(log_mutex);
    std::string msg = log[0];
    log.erase(log.begin());
    return msg;
  }

  std::string read(size_t line_nr) {
    std::unique_lock<std::mutex> lock(log_mutex);
    if (line_nr >= log.size())
      throw std::out_of_range("line_nr >= log.size()");
    else 
      return log[line_nr];
    }
};


struct Buffer {
private:
  size_t bound;
  std::mutex buf_mutex;
  std::condition_variable not_empty;
  std::condition_variable not_full;
  std::deque<int> buf;
  Logger log;

public:
  Buffer (size_t initial_bound) : bound{initial_bound} {}

  Buffer () : bound{SIZE_MAX} {}

  void set_bound(size_t new_bound) {
    std::unique_lock<std::mutex> lock(buf_mutex);
    if (buf.size() >= new_bound)
      buf.resize(new_bound);

    bound = new_bound;
    log.write("succeeded to set bound to " + std::to_string(new_bound));
    return;
  }

  void set_unbounded() {
    bound = SIZE_MAX;
  }

  void add (int i) {
    std::unique_lock<std::mutex> lock(buf_mutex);
    while (buf.size() == bound) {
      not_full.wait(lock);
    }
    buf.push_back(i);
    log.write("succeeded to add " + std::to_string(i));
    not_empty.notify_all();
  }

  int take () {
    std::unique_lock<std::mutex> lock(buf_mutex);
    while (buf.empty()) {
      not_empty.wait(lock);
    }
    int elem = buf.front();
    buf.pop_front();
    not_full.notify_all();
    log.write("succeeded to remove " + std::to_string(elem));
    return elem;
  }
};
