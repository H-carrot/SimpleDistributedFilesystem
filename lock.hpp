// Full disclosure, I have used this code in previous classes
#ifndef base_lock_h
#define base_lock_h

#include <pthread.h>
#include <sys/time.h>

namespace base {

  class Mutex {
  public:
    pthread_mutex_t m_;

    Mutex() { pthread_mutex_init(&m_, NULL); }
    ~Mutex() { pthread_mutex_destroy(&m_); }

    void lock() { pthread_mutex_lock(&m_); }
    void unlock() { pthread_mutex_unlock(&m_); }

    private:
      Mutex& operator = (Mutex&);
  };

class ConditionVar {
  public:

    ConditionVar()          { pthread_cond_init(&cv_, NULL); }
    ~ConditionVar()         { pthread_cond_destroy(&cv_); }

    void wait(Mutex* mutex) {
      struct timeval tv;
      struct timespec ts;
      int nTimeOut = 50;

      gettimeofday(&tv, NULL);
      ts.tv_sec = time(NULL) + nTimeOut / 1000;
      ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (nTimeOut % 1000);
      ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
      ts.tv_nsec %= (1000 * 1000 * 1000);

      pthread_cond_timedwait(&cv_, &(mutex->m_), &ts);
    }
    void signal()           { pthread_cond_signal(&cv_); }
    void signalAll()        { pthread_cond_broadcast(&cv_); }

    void timedWait(Mutex* mutex, const struct timespec* timeout) {
      pthread_cond_timedwait(&cv_, &(mutex->m_), timeout);
    }

  private:
    pthread_cond_t cv_;
};

}

#endif