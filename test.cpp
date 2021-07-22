#include <condition_variable> // std::condition_variale
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>
using namespace std;

std::mutex g_mutex;
std::condition_variable g_cv;

bool g_ready = false;
int g_data = 0;

int produceData() {
  int randomNumber = rand() % 1000;
  std::cout << "produce data: " << randomNumber << "\n";
  return randomNumber;
}

void consumeData(int data) { std::cout << "receive data: " << data << "\n"; }

void consumer() {
  int data = 0;
  while (true) {
    std::unique_lock<std::mutex> ul(g_mutex);
    g_cv.wait(ul, []() { return g_ready; });
    consumeData(g_data);
    g_ready = false;
    ul.unlock();
    g_cv.notify_one();
  }
}

void producer() {
  while (true) {
    std::unique_lock<std::mutex> ul(g_mutex);
    g_data = produceData();
    g_ready = true;
    ul.unlock();
    g_cv.notify_one();
    ul.lock();
    g_cv.wait(ul, []() { return g_ready == false; });
  }
}

void consumerThread(int n) { consumer(); }

void producerThread(int n) { producer(); }

int main() {
  int times = 100;
  std::thread t1(consumerThread, times);
  std::thread t2(producerThread, times);
  t1.join();
  t2.join();
  return 0;
}
