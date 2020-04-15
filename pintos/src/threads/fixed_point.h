#define F (1 << 14) // fixed point 1
#define INT_MAX ((1 << 31) -1)
#define INT_MIN (-(1 << 31))

// x and y denote fixed_point numbers in 17.14 format
// n is an integer

int int_to_fp (int n);         // int to fp
int fp_to_int_round (int x);   // fp to int (rounding to nearest)
int fp_to_int (int x);         // fp to int (rounding toward zero)

int add_fp (int x, int y);     // + between fp numbers
int add_mixed (int x, int n);  // + between fp number and int

int sub_fp (int x, int y);     // - between fp numbers
int sub_mixed (int x, int n);  // - between fp number and int

int mult_fp (int x, int y);    // * between fp numbers
int mult_mixed (int x, int n); // * between fp number and int

int div_fp (int x, int y);     // / between fp numbers
int div_mixed (int x, int n);  // / between fp number and int

// int to fp
int
int_to_fp (int n)
{
  return n * F;
}

// fp to int (rounding to nearest)
int
fp_to_int_round (int x)
{
  if (x >= 0)
    return (x + F / 2) / F;
  else
    return (x - F / 2) / F;
}

// fp to int (rounding toward zero)
int
fp_to_int (int x)
{
  return x / F;
}

// + between fp numbers
int
add_fp (int x, int y)
{
  return x + y;
}

// + between fp number and int
int
add_mixed (int x, int n)
{
  return x + n * F;
}

// - between fp numbers
int
sub_fp (int x, int y)
{
  return x - y;
}

// - between fp number and int
int
sub_mixed (int x, int n)
{
  return x - n * F;
}

// * between fp numbers
int
mult_fp (int x, int y)
{
  return ((int64_t) x) * y / F;
}

// * between fp number and int
int
mult_mixed (int x, int n)
{
  return x * n;
}

// / between fp numbers
int
div_fp (int x, int y)
{
  return ((int64_t) x) * F / y;
}

// / between fp number and int
int
div_mixed (int x, int n)
{
  return x / n;
}
