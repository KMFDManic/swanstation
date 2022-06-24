#include "timer.h"
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include "windows_headers.h"
#else
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

namespace Common {

#ifdef _WIN32

static double s_counter_frequency;
static bool s_counter_initialized = false;

static HANDLE GetSleepTimer(void)
{
  // This gets leaked... oh well.
  static thread_local HANDLE s_sleep_timer;
  static thread_local bool s_sleep_timer_created = false;
  if (s_sleep_timer_created)
    return s_sleep_timer;

  s_sleep_timer_created = true;
  s_sleep_timer = CreateWaitableTimer(nullptr, TRUE, nullptr);
  return s_sleep_timer;
}

Timer::Value Timer::GetValue(void)
{
  // even if this races, it should still result in the same value..
  if (!s_counter_initialized)
  {
    LARGE_INTEGER Freq;
    QueryPerformanceFrequency(&Freq);
    s_counter_frequency = static_cast<double>(Freq.QuadPart) / 1000000000.0;
    s_counter_initialized = true;
  }

  Timer::Value ReturnValue;
  QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&ReturnValue));
  return ReturnValue;
}

double Timer::ConvertValueToNanoseconds(Timer::Value value)
{
  return (static_cast<double>(value) / s_counter_frequency);
}

double Timer::ConvertValueToMilliseconds(Timer::Value value)
{
  return ((static_cast<double>(value) / s_counter_frequency) / 1000000.0);
}

double Timer::ConvertValueToSeconds(Timer::Value value)
{
  return ((static_cast<double>(value) / s_counter_frequency) / 1000000000.0);
}

Timer::Value Timer::ConvertSecondsToValue(double s)
{
  return static_cast<Value>((s * 1000000000.0) * s_counter_frequency);
}

Timer::Value Timer::ConvertMillisecondsToValue(double ms)
{
  return static_cast<Value>((ms * 1000000.0) * s_counter_frequency);
}

Timer::Value Timer::ConvertNanosecondsToValue(double ns)
{
  return static_cast<Value>(ns * s_counter_frequency);
}

void Timer::SleepUntil(Value value, bool exact)
{
  if (exact)
  {
    while (GetValue() < value)
      SleepUntil(value, false);
  }
  else
  {
    const std::int64_t diff = static_cast<std::int64_t>(value - GetValue());
    if (diff <= 0)
      return;

#ifndef _UWP
    HANDLE timer = GetSleepTimer();
    if (timer)
    {
      FILETIME ft;
      GetSystemTimeAsFileTime(&ft);

      LARGE_INTEGER fti;
      fti.LowPart = ft.dwLowDateTime;
      fti.HighPart = ft.dwHighDateTime;
      fti.QuadPart += diff;

      if (SetWaitableTimer(timer, &fti, 0, nullptr, nullptr, FALSE))
      {
        WaitForSingleObject(timer, INFINITE);
        return;
      }
    }
#endif

    // falling back to sleep... bad.
    Sleep(static_cast<DWORD>(static_cast<std::uint64_t>(diff) / 1000000));
  }
}

#else

Timer::Value Timer::GetValue(void)
{
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return ((Value)tv.tv_nsec + (Value)tv.tv_sec * 1000000000);
}

double Timer::ConvertValueToNanoseconds(Timer::Value value)
{
  return static_cast<double>(value);
}

double Timer::ConvertValueToMilliseconds(Timer::Value value)
{
  return (static_cast<double>(value) / 1000000.0);
}

double Timer::ConvertValueToSeconds(Timer::Value value)
{
  return (static_cast<double>(value) / 1000000000.0);
}

Timer::Value Timer::ConvertSecondsToValue(double s)
{
  return static_cast<Value>(s * 1000000000.0);
}

Timer::Value Timer::ConvertMillisecondsToValue(double ms)
{
  return static_cast<Value>(ms * 1000000.0);
}

Timer::Value Timer::ConvertNanosecondsToValue(double ns)
{
  return static_cast<Value>(ns);
}

void Timer::SleepUntil(Value value, bool exact)
{
  if (exact)
  {
    while (GetValue() < value)
      SleepUntil(value, false);
  }
  else
  {
    // Apple doesn't have TIMER_ABSTIME, so fall back to nanosleep in such a case.
#ifdef __APPLE__
    const Value current_time = GetValue();
    if (value <= current_time)
      return;

    const Value diff = value - current_time;
    struct timespec ts;
    ts.tv_sec = diff / UINT64_C(1000000000);
    ts.tv_nsec = diff % UINT64_C(1000000000);
    nanosleep(&ts, nullptr);
#else
    struct timespec ts;
    ts.tv_sec = value / UINT64_C(1000000000);
    ts.tv_nsec = value % UINT64_C(1000000000);
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
#endif
  }
}

#endif

Timer::Timer()
{
  Reset();
}

void Timer::Reset()
{
  m_tvStartValue = GetValue();
}

double Timer::GetTimeSeconds() const
{
  return ConvertValueToSeconds(GetValue() - m_tvStartValue);
}

double Timer::GetTimeMilliseconds() const
{
  return ConvertValueToMilliseconds(GetValue() - m_tvStartValue);
}

} // namespace Common
