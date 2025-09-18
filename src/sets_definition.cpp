#include "sets_definition.hpp"
#include <iostream>


// Mandelbrot
static const long double pisqrtpi = PI * std::sqrt(PI);
static const long double pisqrt2  = PI * std::sqrt(2);
Color getColorFromPoint_Mandelbrot(long double a, long double b, float maxIterations) {
    long double ca = a;
    long double cb = b;

    int n;
    long double aa, bb;
    for (n = 0; (a * a + b * b <= 16) && (n < maxIterations); n++) {
        aa = a * a - b * b + ca;
        b  = 2.0L * a * b + cb;
        a  = aa;
    }

    // Coloring
    Color color = BLACK;
    if (n < maxIterations) {
        color.a = 255;
        color.r = ((int)(n * PI))       % 255;
        color.g = ((int)(n * pisqrtpi)) % 255;
        color.b = ((int)(n * pisqrt2))  % 255;
    }
    return color;
}

// Julia
const long double julia_ca = -0.7;    // Real part of c
const long double julia_cb = 0.27015; // Imaginary part of c
Color HSVtoRGB(float h, float s, float v) {
  float r, g, b;

  int i = int(h * 6);
  float f = h * 6 - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);

  switch (i % 6) {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    case 5:
        r = v;
        g = p;
        b = q;
        break;
  }

  return Color{
      (unsigned char)(r * 255),
      (unsigned char)(g * 255),
      (unsigned char)(b * 255),
      255};
}
Color getColorFromPoint_Julia(long double a, long double b, float maxIterations) {
  int n = 0;
  long double aa, bb;

  for (; n < maxIterations; ++n) {
    if ((a * a + b * b) > 4.0) { break; }
    aa = a * a - b * b + julia_ca;
    bb = 2.0 * a * b + julia_cb;
    a = aa;
    b = bb;
  }

  if (n == maxIterations) { return BLACK; }

  // Smooth coloring
  long double zn = sqrt(a * a + b * b);
  long double smooth = n + 1 - log2(log2(zn));

  float hue = (float) (0.95f + 20.0f * smooth / maxIterations); // tweak multiplier
  hue = fmod(hue, 1.0f);                                       // keep hue in [0,1]
  float saturation = 0.8f;
  float value = 1.0f;

  return HSVtoRGB(hue, saturation, value);
}

// Burning ship
Color getColorFromPoint_BurningShip(long double a, long double b, int maxIterations) {
  long double x = 0, y = 0;
  int n = 0;
  while (x * x + y * y <= 4 && n < maxIterations)
  {
    long double xtemp = x * x - y * y + a;
    y = fabs(2 * x * y) + b;
    x = fabs(xtemp);
    n++;
  }

  float t = (float)n / maxIterations;
  return (n == maxIterations) ? BLACK : Color{(unsigned char)(9 * (1 - t) * t * t * t * 255), (unsigned char)(15 * (1 - t) * (1 - t) * t * t * 255), (unsigned char)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255), 255};
}

// Tricorn
Color getColorFromPoint_Tricorn(long double a, long double b, int maxIterations) {
  long double x = 0, y = 0;
  int n = 0;
  while (x * x + y * y <= 4 && n < maxIterations) {
    long double xtemp = x * x - y * y + a;
    y = -2 * x * y + b;
    x = xtemp;
    n++;
  }

  float t = (float)n / maxIterations;
  return (n == maxIterations) ? BLACK : Color{(unsigned char)(255 * t), (unsigned char)(255 * (1 - t)), (unsigned char)(128 * t), 255};
}

// Phoenix
Color getColorFromPoint_Phoenix(long double a, long double b, int maxIterations) {
  // Complex parameters
  long double cRe = a;
  long double cIm = b;

  // Phoenix constant p (can be tweaked for different visuals)
  const long double pRe = -0.5;
  const long double pIm = 0.0;

  long double x = 0.0, y = 0.0;         // z_n
  long double xPrev = 0.0, yPrev = 0.0; // z_{n-1}

  int n = 0;
  while ((x * x + y * y <= 4.0) && n < maxIterations) {
    // Complex multiplication: z_n^2
    long double x2 = x * x - y * y;
    long double y2 = 2 * x * y;

    // Add c and p * z_{n-1}
    long double xTemp = x2 + cRe + (pRe * xPrev - pIm * yPrev);
    long double yTemp = y2 + cIm + (pRe * yPrev + pIm * xPrev);

    xPrev = x;
    yPrev = y;
    x = xTemp;
    y = yTemp;

    n++;
  }

  if (n == maxIterations) { return BLACK; }

  // Smooth coloring
  float zn = sqrt(x * x + y * y);
  float smooth = n + 1 - log(log(zn)) / log(2.0);
  float t = smooth / maxIterations;

  // Gradient: smooth rainbow
  unsigned char r = (unsigned char)(9 * (1 - t) * t * t * t * 255);
  unsigned char g = (unsigned char)(15 * (1 - t) * (1 - t) * t * t * 255);
  unsigned char bCol = (unsigned char)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255);

  return Color{r, g, bCol, 255};
}

// Lyapunov
Color getColorFromPoint_Lyapunov(long double a, long double b, int maxIterations) {
  // 'a' and 'b' represent rA and rB in the logistic map
  const char *pattern = "AABAB"; // Feel free to change the pattern
  int patternLength = strlen(pattern);

  long double x = 0.5; // Starting value
  long double lyap = 0.0;
  bool stable = true;

  for (int n = 0; n < maxIterations; n++) {
    char ch = pattern[n % patternLength];
    long double r = (ch == 'A') ? a : b;
    x = r * x * (1.0 - x);
    if (x <= 0.0 || x >= 1.0) {
      stable = false;
      break;
    }
    long double deriv = fabs(r * (1.0 - 2.0 * x));
    if (deriv > 0.0) { lyap += log(deriv); }
  }

  if (!stable) { return BLACK; }

  lyap /= maxIterations;

  // --- Gradient coloring ---
  float t = (float)((lyap + 2.0) / 4.0); // Normalize exponent range ~[-2,2] to [0,1]
  t = fminf(fmaxf(t, 0.0f), 1.0f);       // Clamp

  // Warm fiery gradient: deep red to yellow
  unsigned char r = (unsigned char)(255 * t);
  unsigned char g = (unsigned char)(200 * sqrt(t));
  unsigned char bl = (unsigned char)(30 * (1.0 - t));

  return Color{r, g, bl, 255};
}
