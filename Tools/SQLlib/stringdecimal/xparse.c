// Functions to perform simple expression parsing
// (c) Copyright 2019 Andrews & Arnold Adrian Kennard
/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <ctype.h>
#include "xparse.h"

//#define DEBUG

// The parse function
void *
xparse (xparse_config_t * config, void *context, const char *sum, const char **end)
{
   if (end)
      *end = NULL;
   const char *fail = NULL;
   int level = 0;               // Bracketing level
   int operators = 0,
      operatormax = 0;
   struct operator_s
   {
      const xparse_op_t *op;
      int level;
      int args;
      xparse_operate *func;
      void *data;
      const char *posn;
   } *operator = NULL;          // Operator stack
   int operands = 0,
      operandmax = 0;
   void **operand = NULL;
#ifdef DEBUG
   char **opval = NULL;
#endif
   const char *posn = sum;
   void addarg (void *v, const char *text, int l)
   {
      if (operands + 1 > operandmax)
      {
         operandmax += 10;
         operand = realloc (operand, operandmax * sizeof (*operand));
#ifdef DEBUG
         opval = realloc (opval, operandmax * sizeof (*opval));
#endif
      }
      operand[operands] = v;
#ifdef DEBUG
      if (!l)
         l = strlen (text);
      warnx ("Added operand %.*s", l, text);
      opval[operands] = strndup (text, l);
#endif
      operands++;
   }
   void operate (void)
   {
      if (!operators--)
      {
         fail = "Mission operator";
         return;
      }
      int args = operator[operators].args;
      if (operands < args)
      {
         posn = operator[operators].posn;
         fail = "Missing args";
         return;
      }
      if (args > 3)
      {
         posn = operator[operators].posn;
         fail = "Cannot handle more than 3 args at present";
         return;
      }
#ifdef	DEBUG
      if (args == 1)
         warnx ("Doing %s (%s)", operator[operators].op->op, opval[operands - 1]);
      else if (args == 2)
         warnx ("Doing (%s) %s (%s)", opval[operands - 2], operator[operators].op->op, opval[operands - 1]);
      else if (args == 3)
         warnx ("Doing (%s) %s (%s) (%s)", opval[operands - 3], operator[operators].op->op, opval[operands - 2],
                opval[operands - 1]);
#endif
      void *a[3] = { };
      for (int n = 0; n < args; n++)
         a[n] = operand[operands - args + n];
      void *v = operator[operators].func (context, operator[operators].data, a);
      while (args--)
      {
         operands--;
#ifdef DEBUG
         free (opval[operands]);
#endif
         if (operand[operands] && operand[operands] != v)
            config->dispose (context, operand[operands]);
      }
      if (!v)
      {
         posn = operator[operators].posn;
         fail = "Operation failed";
         return;
      }
      addarg (v, operator[operators].op->op, 0);
   }
   void addop (const xparse_op_t * op, int level, int args, const char *posn)
   {                            // Add an operator
      if (args < 0)
         args = 0 - args;       // Used for prefix unary ops, don't run stack
      else
         while (!fail && operators && operator[operators - 1].level >= level && operator[operators - 1].args)
            operate ();         // Clear stack of pending ops
      if (operators + 1 > operatormax)
         operator = realloc (operator, (operatormax += 10) * sizeof (*operator));
      operator[operators].op = op;
      operator[operators].func = op->func;
      operator[operators].data = op->data;
      operator[operators].level = level;
      operator[operators].args = args;
      operator[operators].posn = posn;
      operators++;
#ifdef DEBUG
      warnx ("Added %s level=%d args=%d", op->op, level, args);
#endif
   }
   int comp (const char *a, const char *b)
   {
      if (!a || !b)
         return 0;
      int l = 0;
      while (a[l] && b[l] && a[l] == b[l])
         l++;
      if (!a[l])
         return l;
      return 0;
   }
   int q = 0,
      l;
   while (!fail)
   {
      posn = sum;
      // Prefix operators and open brackets
      if (*sum == '!' && sum[1] == '!')
      {
         fail = "Error";
         break;
      }
      while (1)
      {
         posn = sum;
         if (config->bracket)
         {
            for (q = 0; config->bracket[q].op; q++)
               if ((l = comp (config->bracket[q].op, sum)))
               {
                  sum += l;
                  addop (&config->bracket[q], level, -1, posn);
                  level += 20;
                  break;
               }
            if (config->bracket[q].op)
               continue;        // again
         }
         if (isspace (*sum) && (!config->eol || (unsigned char) *sum >= ' '))
         {
            sum++;
            continue;
         }
         // Pre unary operators
         if (config->unary)
         {
            for (q = 0; config->unary[q].op; q++)
               if ((l = comp (config->unary[q].op, sum)) || (l = comp (config->unary[q].op2, sum)))
               {
                  sum += l;
                  addop (&config->unary[q], level + config->unary[q].level, -1, posn);
                  break;
               }
            if (config->unary[q].op)
               continue;
         }
         break;
      }
      const char *was = sum;
      if (config->ternary)
         for (int q = 0; config->ternary[q].op; q++)
            if (comp (config->ternary[q].op2, sum))
            {
               was = NULL;
               addarg (NULL, "missing", 0);
               break;
            }
      if (was)
      {                         // Operand
         void *v = config->operand (context, sum, &sum);
         if (!v || sum == was)
         {
            fail = "Missing operand";
            break;
         }
         // Add the operand
         addarg (v, was, sum - was);
      }
      // Postfix operators and close brackets
      while (1)
      {
         posn = sum;
         if (config->bracket)
         {
            for (q = 0; config->bracket[q].op; q++)
               if ((l = comp (config->bracket[q].op2, sum)))
               {
                  while (!fail && operators && (operator[operators - 1].level > level - 20))
                     operate ();        // Clear stack of pending ops
                  if (operators && operator[operators - 1].op == &config->bracket[q])
                  {
                     level -= 20;
                     sum += l;
                     operate ();
                     break;
                  }
               }
            if (config->bracket[q].op)
               continue;        // again
         }
         if (isspace (*sum) && (!config->eol || (unsigned char) *sum >= ' '))
         {
            sum++;
            continue;
         }
         // Post unary operators
         if (config->post)
         {
            for (q = 0; config->post[q].op; q++)
               if ((l = comp (config->post[q].op, sum)) || (l = comp (config->post[q].op2, sum)))
               {
                  sum += l;
                  addop (&config->post[q], level + config->post[q].level, 1, posn);
                  break;
               }
            if (config->post[q].op)
               continue;
         }
         break;
      }
      if (!*sum || (config->eol && (unsigned char) *sum < ' '))
      {
         while (sum > was && isspace (sum[-1]))
            sum--;
         break;                 // clean exit after last operand
      }
      // Operator
      const char *implied = NULL;
      {                         // Implied power
         static const char *sup[11] = { "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹", "⁽" };
         for (q = 0; q < sizeof (sup) / sizeof (*sup); q++)
            if ((l = comp (sup[q], sum)))
            {
               implied = "^";
               break;
            }
      }
      if (config->binary)
      {
         for (q = 0; config->binary[q].op; q++)
            if ((l = comp (config->binary[q].op, implied ? : sum)) || (l = comp (config->binary[q].op2, implied ? : sum)))
            {
               if (!implied)
                  sum += l;
               addop (&config->binary[q], level + config->binary[q].level, 2, posn);
               break;
            }
         if (config->binary[q].op)
            continue;
      }
      if (config->ternary)
      {
         // Left hand side of ternary
         for (q = 0; config->ternary[q].op; q++)
            if ((l = comp (config->ternary[q].op, implied ? : sum)))
            {
               if (!implied)
                  sum += l;
               addop (&config->ternary[q], level + config->ternary[q].level, 0, posn);
               break;
            }
         if (config->ternary[q].op)
            continue;
         // Right hand side of ternary
         for (q = 0; config->ternary[q].op; q++)
            if ((l = comp (config->ternary[q].op2, implied ? : sum)))
            {
               while (!fail && operators
                      && (operator[operators - 1].level > level + config->ternary[q].level
                          || (operator[operators - 1].level == level + config->ternary[q].level
                              && operator[operators - 1].args == 3)))
                  operate ();   // Clear stack of pending ops
               if (operators && operator[operators - 1].op == &config->ternary[q] && operator[operators - 1].args == 0
                   && operator[operators - 1].level == level + config->ternary[q].level)
               {                // matches
#ifdef DEBUG
                  warnx ("Making op %s ternary", operator[operators - 1].op->op);
#endif
                  if (!implied)
                     sum += l;
                  operator[operators - 1].args = 3;     // Extent op
                  break;
               }
            }
         if (config->ternary[q].op)
            continue;
      }
      if (!end || level)
         fail = "Missing/unknown operator";
      while (sum > was && isspace (sum[-1]))
         sum--;
      break;
   }
   while (!fail && operators)
      operate ();               // Final operators
   if (!fail && level)
      fail = "Unclosed bracket";
   void *v = NULL;
   if (!fail && operands == 1)
      v = config->final (context, operand[0]);
   while (operands--)
      if (operand[operands] && operand[operands] != v)
         config->dispose (context, operand[operands]);
   if (operand)
      free (operand);
   if (operator)
      free (operator);
   if (fail && config->fail)
      config->fail (context, fail, posn);
   if (end)
      *end = sum;
   return v;
}
