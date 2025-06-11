#ifndef DISK_ARRAY_H
#define DISK_ARRAY_H

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cassert>
#include <cstring>
#include <string>
#include <stdexcept>
#include <iostream>
#include <climits>
#include <sstream>
#include <random>

// Generate UUID string
inline std::string generate_uuid() {
   static std::random_device rd;
   static std::mt19937 gen(rd());
   static std::uniform_int_distribution<uint64_t> dis;
   std::stringstream ss;
   ss << std::hex << dis(gen);
   return ss.str();
}

// Disk-backed 1D array for POD types, modeled after Array1<T>
template<typename T>
struct DiskArray1 {
   typedef T* iterator;
   typedef const T* const_iterator;
   typedef unsigned long size_type;
   typedef long difference_type;
   typedef T& reference;
   typedef const T& const_reference;
   typedef T value_type;
   typedef T* pointer;
   typedef const T* const_pointer;
   typedef std::reverse_iterator<iterator> reverse_iterator;
   typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

   unsigned long n;
   unsigned long max_n;
   T* data;
   int fd;
   std::string filename;

   DiskArray1() : n(0), max_n(0), data(nullptr), fd(-1) {}
   DiskArray1(unsigned long n_) : n(n_), max_n(n_) { map_file(n); }
   DiskArray1(unsigned long n_, const T& value) : n(n_), max_n(n_) {
      map_file(n);
      for (unsigned long i = 0; i < n; ++i) data[i] = value;
   }
   DiskArray1(unsigned long n_, const T& value, unsigned long max_n_) : n(n_), max_n(max_n_) {
      assert(n_ <= max_n_);
      map_file(max_n_);
      for (unsigned long i = 0; i < n; ++i) data[i] = value;
   }
   DiskArray1(unsigned long n_, const T* data_) : n(n_), max_n(n_) {
      assert(data_);
      map_file(n);
      std::memcpy(data, data_, n * sizeof(T));
   }
   DiskArray1(unsigned long n_, const T* data_, unsigned long max_n_) : n(n_), max_n(max_n_) {
      assert(data_);
      assert(n <= max_n);
      map_file(max_n);
      std::memcpy(data, data_, n * sizeof(T));
   }
   DiskArray1(const DiskArray1<T>& x) : n(x.n), max_n(x.max_n) {
      map_file(max_n);
      std::memcpy(data, x.data, n * sizeof(T));
   }

   ~DiskArray1() {
      if (data) munmap(data, max_n * sizeof(T));
      if (fd >= 0) close(fd);
#ifndef NDEBUG
      data = 0; n = max_n = 0;
#endif
   }

   DiskArray1<T>& operator=(const DiskArray1<T>& x) {
      if (&x == this) return *this;
      resize(x.n);
      std::memcpy(data, x.data, x.n * sizeof(T));
      return *this;
   }

   template<typename InputIterator>
   void assign(InputIterator first, InputIterator last) {
      size_t count = std::distance(first, last);
      resize(count);
      size_t i = 0;
      for (InputIterator it = first; it != last; ++it, ++i) {
         data[i] = *it;
      }
   }

   T& operator[](unsigned long i) { return data[i]; }
   const T& operator[](unsigned long i) const { return data[i]; }
   T& operator()(unsigned long i) { assert(i < n); return data[i]; }
   const T& operator()(unsigned long i) const { assert(i < n); return data[i]; }
   T& at(unsigned long i) { assert(i < n); return data[i]; }
   const T& at(unsigned long i) const { assert(i < n); return data[i]; }

   bool operator==(const DiskArray1<T>& x) const {
      if (n != x.n) return false;
      for (unsigned long i = 0; i < n; ++i) if (!(data[i] == x.data[i])) return false;
      return true;
   }
   bool operator!=(const DiskArray1<T>& x) const {
      if (n != x.n) return true;
      for (unsigned long i = 0; i < n; ++i) if (data[i] != x.data[i]) return true;
      return false;
   }
   bool operator<(const DiskArray1<T>& x) const {
      for (unsigned long i = 0; i < n && i < x.n; ++i) {
         if (data[i] < x[i]) return true;
         else if (x[i] < data[i]) return false;
      }
      return n < x.n;
   }
   bool operator>(const DiskArray1<T>& x) const {
      for (unsigned long i = 0; i < n && i < x.n; ++i) {
         if (data[i] > x[i]) return true;
         else if (x[i] > data[i]) return false;
      }
      return n > x.n;
   }
   bool operator<=(const DiskArray1<T>& x) const {
      for (unsigned long i = 0; i < n && i < x.n; ++i) {
         if (data[i] < x[i]) return true;
         else if (x[i] < data[i]) return false;
      }
      return n <= x.n;
   }
   bool operator>=(const DiskArray1<T>& x) const {
      for (unsigned long i = 0; i < n && i < x.n; ++i) {
         if (data[i] > x[i]) return true;
         else if (x[i] > data[i]) return false;
      }
      return n >= x.n;
   }

   void assign(const T& value) { for (unsigned long i = 0; i < n; ++i) data[i] = value; }
   void assign(unsigned long num, const T& value) { fill(num, value); }
   void assign(unsigned long num, const T* copydata) {
      assert(num == 0 || copydata);
      if (num > max_n) resize(num);
      std::memcpy(data, copydata, num * sizeof(T));
      n = num;
   }
   void add_unique(const T& value) {
      for (unsigned long i = 0; i < n; ++i) if (data[i] == value) return;
      push_back(value);
   }
   void erase(unsigned long index) {
      assert(index < n);
      for (unsigned long i = index; i < n - 1; ++i) data[i] = data[i + 1];
      pop_back();
   }
   void insert(unsigned long index, const T& entry) {
      assert(index <= n);
      push_back(data[n - 1]);
      for (unsigned long i = n - 1; i > index; --i) data[i] = data[i - 1];
      data[index] = entry;
   }
   void set_zero() { std::memset(data, 0, n * sizeof(T)); }

   void resize(unsigned long new_n) {
      if (new_n <= max_n) {
         n = new_n;
         return;
      }
      T* old_data = data;
      size_t old_size = max_n * sizeof(T);
      std::string old_filename = filename;
      int old_fd = fd;
      n = new_n;
      max_n = new_n;
      map_file(new_n);
      if (old_data) {
         std::memcpy(data, old_data, old_size);
         munmap(old_data, old_size);
         close(old_fd);
         unlink(old_filename.c_str());
      }
   }

   void grow() {
      unsigned long new_size = max_n < ULONG_MAX / 2 ? 2 * max_n + 1 : ULONG_MAX / sizeof(T);
      resize(new_size);
   }

   void push_back(const T& value) {
      if (n == max_n) grow();
      data[n++] = value;
   }

   void pop_back() {
      assert(n > 0);
      --n;
   }

   void reserve(unsigned long r) {
      if (r > max_n) resize(r);
   }

   void swap(DiskArray1<T>& x) {
      std::swap(n, x.n);
      std::swap(max_n, x.max_n);
      std::swap(data, x.data);
      std::swap(fd, x.fd);
      std::swap(filename, x.filename);
   }

   void trim() {
      if (n == max_n) return;
      T* old_data = data;
      size_t old_size = max_n * sizeof(T);
      std::string old_filename = filename;
      int old_fd = fd;
      max_n = n;
      map_file(n);
      std::memcpy(data, old_data, n * sizeof(T));
      munmap(old_data, old_size);
      close(old_fd);
      unlink(old_filename.c_str());
   }

   size_type size() const { return n; }
   size_type capacity() const { return max_n; }
   size_type max_size() const { return ULONG_MAX / sizeof(T); }

   T* begin() { return data; }
   const T* begin() const { return data; }
   T* end() { return data + n; }
   const T* end() const { return data + n; }
   reverse_iterator rbegin() { return reverse_iterator(end()); }
   const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
   reverse_iterator rend() { return reverse_iterator(begin()); }
   const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

   const T& front() const { assert(n > 0); return *data; }
   T& front() { assert(n > 0); return *data; }
   const T& back() const { assert(n > 0); return data[n - 1]; }
   T& back() { assert(n > 0); return data[n - 1]; }

   bool empty() const { return n == 0; }
   void clear() {
      if (data) munmap(data, max_n * sizeof(T));
      if (fd >= 0) close(fd);
      data = nullptr;
      fd = -1;
      n = 0;
      max_n = 0;
      filename.clear();
   }

private:
   void map_file(unsigned long elems) {
      if (filename.empty()) {
         std::stringstream ss;
         ss << "file_" << getpid() << "_" << generate_uuid();
         filename = ss.str();
      }
      size_t bytes = elems * sizeof(T);
      fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
      if (fd < 0) throw std::runtime_error("open failed");
      if (ftruncate(fd, bytes) != 0) throw std::runtime_error("ftruncate failed");
      data = (T*)mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (data == MAP_FAILED) throw std::runtime_error("mmap failed");
   }
};

#endif // DISK_ARRAY_H
