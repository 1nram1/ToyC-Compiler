int factorial(int n)
{
   int result = 1;
   if (n <= 0)
   {
      return 1;
   }
   else
   {
      while (n > 1)
      {
         result = result * n;
         n = n - 1;
      }
      return result;
   }
}
int main()
{
   int x = -2;
   int y = -4;
   int z;
   if (x > y && (x - y) > 1)
   {
      z = factorial(x);
   }
   else if (x < y || x == y)
   {
      z = factorial(-(x + y));
   }
   else
   {
      z = factorial(x * y);
   }
   while (z > 100)
   {
      if (z % 2 == 0)
      {
         z = z / 2;
      }
      else
      {
         z = z - 1;
      }
   }
   return z / factorial(4);
}