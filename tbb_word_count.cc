#include <iostream>
#include <cstdlib>
#include <cstring>

#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tick_count.h>

/* Перевіряє, чи є даний символ пробільним. */
static inline bool is_space(char c)
{
  return c == ' ' || c == '\n' || c == '\t';
}

/* Четвірка <L, C, R, F>, що представляє результат підрахунку слів
 * в частині рядку. */
class subrange_result
{
  public:
    bool left_word;   /* L */
    bool right_word;  /* R */
    bool full;        /* F */
    bool is_identity; /* це -- ліва одиниця? */
    size_t count;     /* C */

    /* Ініціалізує екземпляр класу значенням лівої одиниці. */
    subrange_result():
        left_word(false),
        right_word(false),
        full(false),
        is_identity(true),
        count(0)
    {}

    /* Комбінування двох проміжних результатів. */
    subrange_result operator+(const subrange_result &other) const
    {
      /* Якщо це ліва одиниця, то результатом є інший об'єкт. */
      if(this->is_identity)
        return other;

      subrange_result res;
      res.is_identity = false;
      if(this->full && other.full) /* F_1 && F_2 -- дві частини без пробілів */
      {
        res.left_word = false;
        res.right_word = false;
        res.full = true;
        res.count = 0;
      }
      else if(this->full) /* F_1 -- частина без пробілів зліва */
      {
        res = other;
        res.left_word = true;
      }
      else if(other.full) /* F_2 -- частина без пробілів справа */
      {
        res = *this;
        res.right_word = true;
      }
      else if((this->right_word |
               other.left_word) == false) /* (R_1 | L_2) == false */
      {
        res.left_word = this->left_word;
        res.right_word = other.right_word;
        res.full = false;
        res.count = this->count + other.count;
      }
      else
      {
        res.left_word = this->left_word;
        res.right_word = other.right_word;
        res.full = false;
        res.count = this->count + other.count + 1;
      }
      return res;
    }
};

std::ostream &operator<<(std::ostream &stream, const subrange_result &r)
{
  if(r.is_identity)
  {
    stream << "I";
  }
  else
  {
    stream << "<" << r.left_word << ", "
        << r.count << ", " << r.right_word
        << ", " << r.full << ">";
  }
  return stream;
}

/*
 * Об'єкт-функція, що виконує додавання елементів з проміжку r,
 * використовуючи значення start як початкове.
 */
class word_count_func
{
  public:
    const char *string;

    word_count_func(const char *string_):
        string(string_)
    {}

    subrange_result operator()(const tbb::blocked_range<size_t> &r,
                               subrange_result start) const
    {
      subrange_result res;
      size_t i;

      res.is_identity = false;

      /* пропускаємо перше слово */
      for(i = r.begin(); i != r.end() && !is_space(string[i]); i++)
        ;
      if(i == r.end())
      {
        res.full = true;
        return start + res;
      }
      else
      {
        res.left_word = (i != r.begin());
      }

      /* цикл підрахунку слів */
      while(true)
      {
        /* пропустити пробіли */
        for( ; i != r.end() && is_space(string[i]); i++)
          ;
        /* пропустити слово */
        for( ; i != r.end() && !is_space(string[i]); i++)
          ;
        if(i != r.end())
        {
          res.count++;
        }
        else
        {
          break;
        }
      }

      res.right_word = !is_space(string[r.end() - 1]);

      //std::cout << r.begin() << " " << r.end() << " "
      //          << start << " " << res << " " << start + res << std::endl;

      return start + res;
    }
};

/*
 * Об'єкт-функція, що виконує комбінування двох проміжних результатів.
 */
class word_count_reduction
{
  public:
    subrange_result operator()(const subrange_result &a,
                               const subrange_result &b) const
    {
      return a + b;
    }
};

size_t parallel_word_count(const char *string, size_t length)
{
  subrange_result res = tbb::parallel_reduce(
      tbb::blocked_range<size_t>(0, length),
      subrange_result(),
      word_count_func(string),
      word_count_reduction());
  if(res.full)
  {
    return 1;
  }
  else
  {
    return res.count + res.left_word + res.right_word;
  }
}

/*
 * Створює рядок заданої довжини та повертає кількість слів в ньому.
 */
size_t make_string(char *string, size_t length)
{
  size_t words = 1;
  memset(string, 'A', length);
  for(size_t i = 1; i < length - 1; i += (1 + rand() % 8))
  {
    string[i] = ' ';
    if(string[i - 1] != ' ')
    {
      words++;
    }
  }
  string[length] = '\0';
  return words;
}

int main()
{
  const size_t length = 10000000;
  char *string = new char[length + 1];
  const size_t words = make_string(string, length);

  const int P_max = tbb::task_scheduler_init::default_num_threads();
  for(int P = 1; P <= P_max; P++)
  {
    tbb::task_scheduler_init init(P);
    tbb::tick_count t0 = tbb::tick_count::now();
    size_t words_counted = parallel_word_count(string, length);
    tbb::tick_count t1 = tbb::tick_count::now();
    if(words != words_counted)
    {
      std::cout << "error: " << words << " " << words_counted << std::endl;
    }
    double t = (t1 - t0).seconds();
    std::cout << "time = " << t << " with " << P << " threads" << std::endl;
  }
  delete [] string;
}

