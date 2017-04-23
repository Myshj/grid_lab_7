#include <iostream>

#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tick_count.h>

/*
 * Об'єкт-функція, що виконує додавання елементів з проміжку r,
 * використовуючи значення start як початкове
 */
class array_sum_func
{
  public:
    array_sum_func(double *x_):
        x(x_)
    {}
    double operator()(const tbb::blocked_range<size_t> &r, double start) const
    {
      double result = start;
      for(size_t i = r.begin(); i != r.end(); i++)
      {
        result += x[i];
      }
      return result;
    }
    double *x;
};

/*
 * Об'єкт-функція, що виконує додавання двох елементів
 */
class array_sum_reduction
{
  public:
    double operator()(double a, double b) const
    {
      return a + b;
    }
};

double parallel_array_sum(double *x, size_t N)
{
  return tbb::parallel_reduce(
      tbb::blocked_range<size_t>(0, N),
      0.,
      array_sum_func(x),
      array_sum_reduction());
}

int main()
{
  const int P_max = tbb::task_scheduler_init::default_num_threads();
  for(int P = 1; P <= P_max; P++)
  {
    tbb::task_scheduler_init init(P);
    size_t N = 100000000;
    double *x = new double[N];
    for(size_t i = 0; i < N; i++)
    {
      x[i] = 1.;
    }
    tbb::tick_count t0 = tbb::tick_count::now();
    std::cout << parallel_array_sum(x, N) << std::endl;
    tbb::tick_count t1 = tbb::tick_count::now();
    double t = (t1 - t0).seconds();
    std::cout << "time = " << t << " with " << P << " threads" << std::endl;
    delete [] x;
  }
}

