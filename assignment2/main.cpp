/**
  * Assignment: synchronization
  * Operating Systems
  */

/**
  Hint: F2 (or Control-klik) on a functionname to jump to the definition
  Hint: Ctrl-space to auto complete a functionname/variable.
  */

// function/class definitions you are going to use
#include <algorithm>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <deque>
#include <limits>

// although it is good habit, you don't have to type 'std::' before many objects by including this line
using namespace std;


// This will make the log print every new line written to it during execution.
#define DEBUG_ON true

class Logger
{
private:
  mutex log_mutex;
  vector<string> log;

public: 
  // Get the length of the log
  size_t length() {
    log_mutex.lock();

    // BEGIN CRITICAL SECTION
    size_t lgsz = log.size();
    // END CRITICAL SECTION

    log_mutex.unlock();

    return lgsz;
  }
  // Write a message string to the log.
  void write(string msg) {
    log_mutex.lock();

    // BEGIN CRITICAL SECTION
    log.push_back(msg);
    if (DEBUG_ON) {
      cout << msg << endl;
    }
    // END CRITICAL SECTION


    log_mutex.unlock();
    return;
  }

  // The specification changed!
  string read_oldest_and_delete() {
    log_mutex.lock();

    // BEGIN CRITICAL SECTION
    if (log.size() > 0) {
      string msg = log[0];
      log.erase(log.begin());
      // END1 CRITICAL SECTION

      log_mutex.unlock();
      return msg;
    }
    else {
      // END2 CRITICAL SECTION

      log_mutex.unlock();
      throw out_of_range("log is empty");
    }
  }

  // Read a message string from the log at line line_nr. Throws out_of_range if line_nr >= log.size();
  string read(size_t line_nr) {
    log_mutex.lock();

    // BEGIN CRITICAL SECTION
    if (line_nr >= log.size()) {
      // will result in an out_of_range exception.
      // END1 CRITICAL SECTION

      log_mutex.unlock();

      throw out_of_range("index is out of range of the log's size");
    } else {
      string msg  = log[line_nr];
      // END2 CRITICAL SECTION

      log_mutex.unlock();

      return log[line_nr];
    }
  }
};

class Buffer {
private:
  size_t bound;
  mutex buf_mutex; 
  deque<int> buf;
  Logger log;

public:
  // Create a buffer with bound initial_bound.
  Buffer (size_t initial_bound) : bound{initial_bound} {}

  // Create a buffer with "infinite" bound, i.e. an unbounded buffer.
  Buffer () : bound{SIZE_MAX} {}

  // A function I need for testing: read out the log
  Logger& get_log() {return log;}

  // A function I need for testing: retrieve the deque
  deque<int>& get_deque() {return buf;}

  // Set bound to new bound. If there are more than new_bound elements in the buffer, truncate.
  void set_bound(size_t new_bound) {
    buf_mutex.lock();
    // If we decrease the bound and there are more elements in the buffer than new_bound,
    // Bernard said we should truncate the buffer. I assume truncating starts from the back (not specified).

    // BEGIN CRITICAL SECTION
    if (new_bound < buf.size())
      buf.resize(new_bound); 
        // Only resize if smaller: resizing a deque to a larger value than its current size will produce the unwanted behaviour
        // of initializing new elements at the back (either to 0 or by copying the element deque::back())
    bound = new_bound;
    log.write("succeeded to set bound to " + to_string(new_bound));
    // END CRITICAL SECTION

    buf_mutex.unlock();
  }

  // Set the buffer to unbounded.
  void set_unbounded() {
    buf_mutex.lock();

    // BEGIN CRITICAL SECTION
    bound = SIZE_MAX;
    log.write("succeeded to set buffer unbounded.");
    // END CRITICAL SECTION

    buf_mutex.unlock();
  }

  // Add integer to back of buffer. Return true iff succesful. Log success/failure.
  void add (int i) {
    buf_mutex.lock();


    // BEGIN CRITICAL SECTION
    if (buf.size() == bound) {
      log.write("failed to add " + to_string(i) + " : buffer full.");
      // END1 CRITICAL SECTION

      buf_mutex.unlock();
      throw logic_error("failed to add element: buffer full");
    } else {
      try {
        buf.push_back(i);
      } catch (exception const& e) {
        log.write("failed to add " + to_string(i) + ": exception in buffer: " + e.what());
        // END2 CRITICAL SECTION

        buf_mutex.unlock();

        // propagate
        throw logic_error(string("failed to add element: exception in buffer: ") + e.what());
      }
      log.write("succeeded to add " + to_string(i));
      // END3 CRITICAL SECTION

      buf_mutex.unlock();
    }
  }

  int remove () {
    buf_mutex.lock();

    // BEGIN CRITICAL SECTION
    if (buf.empty()) {
      log.write("failed to remove element: buffer empty.");
      // END1 CRITICAL SECTION
      buf_mutex.unlock();
      throw logic_error("failed to remove element: buffer empty.");
    } else {
      int elem = buf.front();
      buf.pop_front();
      log.write("succeeded to remove " + to_string(elem));
      // END2 CRITICAL SECTION
      buf_mutex.unlock();
      return elem;
    }
  }
};


Buffer b; 
// A global buffer. Couldn't be done differently since it needs to be shared and threads are defined outside main()

void test_even () {
  b.set_bound(200);
  for (int t = 0; t < 250; t++) {
    try {
      b.add(2*t);
    } catch (exception const& _) {}
  }
}

void test_odd () {
  for (int t = 0; t < 249; t++) {
    try {
      b.add(2*t+1);
    } catch (exception const& _) {}
  }
}

void test_work_1 () {
  for(int t = 0; t < 5; t ++) {
    try {
      int k = b.remove();
      b.add(k * 5);
    } catch (exception const& _) {
      t--; //try again
    }
  }
}

void test_work_2 () {
  int t = 0;
  for(; t < 300; t ++) {
    try {
      b.add(b.remove() * 5); 
    } catch (logic_error const& _) {
      t--; //try again
    }
  }
}

void test_work_3() {
  for(int t = 0; t < 800; t ++) {
    b.set_bound(rand() %30);
    b.set_unbounded();
    // if we set the bound to 0, the buffer will stay
    // empty forever, so we let this thread artificially
    // add back some elements
    try {
      b.add(56);
      b.add(78);
      b.add(3);
    } catch (exception const& _) {
      continue;
    }
  }
}

void test_lenght() {
  b.set_unbounded();
  for(int i = 0; i < 10000000; i++)
  try {
    b.add(i);
  } catch (logic_error const& e) {
    // expected behaviour:
    // "failed to add i: exception in buffer: ..."
    cout << e.what() << endl;
    return;
  }
}


// MAIN: turn on/off test cases appropriately
int main(int argc, char* argv[]) {
  ///*
  thread te(test_even);
  thread to(test_odd);
  te.join();
  to.join();

  cout << "buffer state : ";
  for (auto i = b.get_deque().begin(); i < b.get_deque().end(); i++)
    cout << *i << " ";
  cout << endl;

  //*/
  ///*

  // Clear the buffer, set bound to 50:
  b.set_bound(0);
  b.set_bound(50);

  for (int i =0 ; i < 20; i++)
    b.add(i); 

  thread t1(test_work_1);
  thread t2(test_work_2);
  thread t3(test_work_1);
  thread t4(test_work_3);
  
  t1.join();
  t2.join();
  t3.join();
  t4.join();

  //*/

  ///*
  thread t5(test_lenght);
  t5.join();

  //*/

  ///*
  
  cout << "The first 22 elements of the log are: " << endl;
  cout << b.get_log().read_oldest_and_delete() << endl;
  
  // Read out the first 21 messages in the buffer
  for(size_t i = 0; i < 21; i++) 
    cout << b.get_log().read(i) << endl;
  //*/
  return 0;
}
