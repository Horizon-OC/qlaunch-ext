/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

/* Give plugins everything from stdc/C++*/
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <deque>
#include <array>
#include <queue>
#include <stack>
#include <functional>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <future>
#include <chrono>
#include <random>
#include <atomic>
#include <bitset>
#include <complex>
#include <valarray>
#include <optional>
#include <variant>
#include <any>
#include <tuple>
#include <regex>
#include <filesystem>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

namespace {
volatile bool hostapi_never = false;
volatile unsigned hostapi_idx = 0;
volatile void *hostapi_sink;
volatile int hostapi_sink_int;
} /* namespace */

__attribute__((used)) static void hostapi_heap(void *a, void *b, void *c, void *d)
{
    hostapi_sink = ::operator new(1);
    hostapi_sink = ::operator new[](1);
    ::operator delete(a, 1);
    ::operator delete[](b, 1);
    ::operator delete(c);
    ::operator delete[](d);
}

__attribute__((used)) static void hostapi_mutex(void *opaque)
{
    std::mutex *m = (std::mutex *)opaque;
    m->lock();
    m->unlock();
    std::recursive_mutex *rm = (std::recursive_mutex *)opaque;
    rm->lock();
    rm->unlock();
    std::lock_guard<std::mutex> g(*m);
    std::unique_lock<std::mutex> u(*m);
    (void)g;
    (void)u;
}

__attribute__((used)) static void hostapi_condvar(void *opaque)
{
    std::condition_variable *cv = (std::condition_variable *)opaque;
    std::mutex *m = (std::mutex *)opaque;
    std::unique_lock<std::mutex> u(*m);
    cv->notify_all();
    cv->notify_one();
    if (hostapi_never)
        cv->wait(u);
}

__attribute__((used)) static void hostapi_threads(void)
{
    std::thread t([] {});
    t.detach();
    std::this_thread::yield();
    auto f = std::async(std::launch::async, [] { return 1; });
    int r = f.get();
    (void)r;
    unsigned n = std::thread::hardware_concurrency();
    (void)n;
    std::promise<int> p;
    p.set_value(3);
}

__attribute__((used)) static void hostapi_smartptr(void)
{
    auto sp = std::make_shared<int>(7);
    std::shared_ptr<int> c = sp;
    std::weak_ptr<int> w = sp;
    auto locked = w.lock();
    (void)c;
    (void)locked;
}

__attribute__((used)) static void hostapi_seq(void *opaque)
{
    std::vector<int> *v = (std::vector<int> *)opaque;
    v->push_back(42);
    v->reserve(64);
    hostapi_sink_int = v->at(hostapi_idx);
    v->resize(8);
    std::list<int> l;
    l.push_back(1);
    l.remove(1);
    std::deque<int> d;
    d.push_front(2);
    d.pop_back();
    std::array<int, 4> a;
    a.fill(9);
    (void)a;
    std::queue<int> q;
    q.push(3);
    q.pop();
    std::stack<int> s;
    s.push(4);
    s.pop();
}

__attribute__((used)) static void hostapi_str(void *opaque)
{
    std::string *s = (std::string *)opaque;
    s->append("x");
    hostapi_sink_int = s->compare("y");
    std::string sub = s->substr(0, 1);
    hostapi_sink = (void *)sub.c_str();
    hostapi_sink = (void *)s->c_str();
    hostapi_sink_int = s->at(hostapi_idx);
    std::stringstream ss;
    ss << 42 << "x";
    std::string r = ss.str();
    hostapi_sink = (void *)r.c_str();
}

__attribute__((used)) static void hostapi_assoc(void *opaque)
{
    std::map<int, int> m;
    m[1] = 2;
    m.find(1);
    m.erase(1);
    std::set<int> st;
    st.insert(3);
    st.count(3);
    std::unordered_map<int, int> *um = (std::unordered_map<int, int> *)opaque;
    um->reserve(16);
    um->find(4);
    std::unordered_set<int> us;
    us.insert(5);
    us.bucket_count();
}

__attribute__((used)) static void hostapi_streams(void)
{
    if (hostapi_never) {
        std::cout << 42 << "x" << std::endl;
        std::cerr << "e";
        int x = 0;
        std::cin >> x;
    }
    std::ofstream *f = (std::ofstream *)0;
    (void)f;
}

__attribute__((used)) static void hostapi_func(void)
{
    std::function<void()> f;
    std::function<int(int)> g = [](int v) { return v + 1; };
    if (hostapi_never) {
        f();
        int r = g(2);
        (void)r;
    }
}

__attribute__((used)) static void hostapi_chrono_rand(void)
{
    auto t1 = std::chrono::system_clock::now();
    auto t2 = std::chrono::steady_clock::now();
    (void)t1;
    (void)t2;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(1, 6);
    int r = dist(rng);
    (void)r;
    std::normal_distribution<double> nd(0.0, 1.0);
    double x = nd(rng);
    (void)x;
}

__attribute__((used)) static void hostapi_misc(void)
{
    std::atomic<int> at{0};
    at.fetch_add(1);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    std::bitset<64> b;
    b.set(3);
    b.count();
    std::complex<double> c(1.0, 2.0);
    double n = std::norm(c);
    (void)n;
    std::valarray<double> va(5);
    va.sum();
    std::optional<int> o = 4;
    o.value_or(0);
    std::variant<int, double> vv = 3.5;
    (void)std::get<double>(vv);
    std::any a = 6;
    (void)std::any_cast<int>(a);
    auto t = std::make_tuple(1, 2.0);
    (void)t;
}

__attribute__((used)) static void hostapi_regex_fs(void *opaque)
{
    std::regex re("a+");
    bool m = std::regex_match("aaa", re);
    (void)m;
    std::filesystem::path *p = (std::filesystem::path *)opaque;
    std::filesystem::path q = *p / "x";
    bool ex = std::filesystem::exists(q);
    (void)ex;
    std::error_code ec;
    std::uintmax_t sz = std::filesystem::file_size(q, ec);
    (void)sz;
    (void)ec;
}


#define H_D1(f) (const void *)(double (*)(double))&f
#define H_D2(f) (const void *)(double (*)(double, double))&f
__attribute__((used)) static const void *kHostLibc[] = {
    (const void *)fopen,
    (const void *)fclose,
    (const void *)fread,
    (const void *)fwrite,
    (const void *)fseek,
    (const void *)ftell,
    (const void *)fgetc,
    (const void *)fputc,
    (const void *)fgets,
    (const void *)fputs,
    (const void *)printf,
    (const void *)fprintf,
    (const void *)snprintf,
    (const void *)vsnprintf,
    (const void *)sprintf,
    (const void *)sscanf,
    (const void *)fflush,
    (const void *)setvbuf,
    (const void *)remove,
    (const void *)rename,
    (const void *)perror,
    (const void *)malloc,
    (const void *)calloc,
    (const void *)realloc,
    (const void *)free,
    (const void *)abort,
    (const void *)exit,
    (const void *)qsort,
    (const void *)bsearch,
    (const void *)rand,
    (const void *)srand,
    (const void *)(int (*)(int))&abs,
    (const void *)(long (*)(long))&labs,
    (const void *)strtol,
    (const void *)strtoul,
    (const void *)strtod,
    (const void *)atoi,
    (const void *)atof,
    (const void *)getenv,
    (const void *)memcpy,
    (const void *)memmove,
    (const void *)memset,
    (const void *)memcmp,
    (const void *)memchr,
    (const void *)strlen,
    (const void *)strcpy,
    (const void *)strncpy,
    (const void *)strcat,
    (const void *)strncat,
    (const void *)strcmp,
    (const void *)strncmp,
    (const void *)strchr,
    (const void *)strrchr,
    (const void *)strstr,
    (const void *)strtok,
    (const void *)strspn,
    (const void *)strcspn,
    (const void *)strdup,
    (const void *)strndup,
    (const void *)strcasecmp,
    (const void *)strncasecmp,
    H_D1(sin),
    H_D1(cos),
    H_D1(tan),
    H_D1(asin),
    H_D1(acos),
    H_D1(atan),
    H_D1(sinh),
    H_D1(cosh),
    H_D1(tanh),
    H_D1(exp),
    H_D1(exp2),
    H_D1(log),
    H_D1(log2),
    H_D1(log10),
    H_D1(sqrt),
    H_D1(cbrt),
    H_D1(ceil),
    H_D1(floor),
    H_D1(round),
    H_D1(trunc),
    H_D1(fabs),
    H_D2(atan2),
    H_D2(pow),
    H_D2(fmod),
    H_D2(fmin),
    H_D2(fmax),
    H_D2(hypot),
    (const void *)(double (*)(double, int))&ldexp,
    (const void *)(double (*)(double, int *))&frexp,
    (const void *)(double (*)(double, double *))&modf,
    (const void *)time,
    (const void *)mktime,
    (const void *)localtime,
    (const void *)gmtime,
    (const void *)strftime,
    (const void *)difftime,
    (const void *)mkdir,
    (const void *)rmdir,
    (const void *)stat,
    (const void *)fstat,
    (const void *)chmod,
    (const void *)opendir,
    (const void *)readdir,
    (const void *)closedir,
    (const void *)rewinddir,
    (const void *)sleep,
    (const void *)usleep,
    (const void *)close,
    (const void *)read,
    (const void *)write,
    (const void *)lseek,
    (const void *)unlink,
    (const void *)access,
    (const void *)getcwd,
    (const void *)chdir,
};
