#include "math_utils.h"

#include "common/log/log.h"

namespace common {

double NormalizeAngle(double ang) {
  return ang - kTwoPi * std::floor((ang + kPi) / kTwoPi);
}

double Degree2Rad(double degree) {
  return degree * kPi / 180.0;
}

double Rad2Degree(double rad) {
  return rad * 180.0 / kPi;
}

const std::vector<int> Range(int begin, int end, int step) {
  ACHECK(step != 0) << "Step must be non-zero";
  int sign = step < 0 ? -1 : 1;
  int num = (end - begin - sign) / step + 1;
  if (num <= 0) return {};
  std::vector<int> seq(num);
  int b = begin;
  for (int i = 0; i != num; ++i) {
    seq[i] = b;
    b += step;
  }
  return seq;
}

const std::vector<int> Range(int end) {
  return Range(0, end, 1);
}

}  // namespace common
