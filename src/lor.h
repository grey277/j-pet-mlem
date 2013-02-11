#pragma once

#include <iostream>
#include <algorithm>

template <typename SType = int> class LOR : public std::pair<SType, SType> {
 public:
  typedef SType S;

  LOR() : std::pair<S, S>(static_cast<S>(0), static_cast<S>(0)) {}

  LOR(S first, S second) : std::pair<S, S>(first, second) {
    if (this->first < this->second) {
      std::swap(this->first, this->second);
    }
  }

  constexpr S index() const {
    return (this->first * (this->first + 1)) / 2 + this->second;
  }

  LOR& operator++() {
    if (++this->second > this->first) {
      this->first++;
      this->second = 0;
    }
    return *this;
  }

  static constexpr LOR end_for_detectors(S n_detectors) {
    return LOR(n_detectors, 0);
  }

  friend std::ostream& operator<<(std::ostream& out, const LOR& lor) {
    return out;
  }
};
